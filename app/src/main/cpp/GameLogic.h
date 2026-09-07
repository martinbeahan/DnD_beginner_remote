#ifndef DND_GAME_LOGIC_H
#define DND_GAME_LOGIC_H

#include <string>
#include <random>
#include <vector>
#include <memory>
#include <sstream>
#include <algorithm>

namespace dnd {

enum class CharacterClass {
    FIGHTER,
    WIZARD,
    ROGUE,
    CLERIC
};

struct Attributes {
    int strength;
    int dexterity;
    int constitution;
    int intelligence;
    int wisdom;
    int charisma;

    static int getModifier(int score) {
        return (score - 10) / 2;
    }
};

enum class ItemType {
    WEAPON,
    ARMOR,
    POTION
};

struct Item {
    std::string name;
    ItemType type;
    int bonus;

    std::string getDescription() const {
        std::stringstream ss;
        ss << name;
        if (bonus != 0) ss << " (+" << bonus << ")";
        return ss.str();
    }
};

struct Spell {
    std::string name;
    int damageDice;
    int damageSides;
    int cost; // Resource cost
    std::string description;
};

struct Character {
    std::string name;
    std::string uid;
    CharacterClass characterClass;
    int level;
    int xp;
    int maxHp;
    int currentHp;
    Attributes attributes;
    int armorClass;

    int resources;
    int maxResources;
    int pendingStatPoints = 0;
    int gold = 0;

    int initiative = 0;
    bool isDowned = false;
    bool isStable = false;   // 3 death-save successes: skip further saves until heal/damage
    bool isDead = false;     // 3 death-save failures: out of the fight (not party Game Over alone)
    int deathSaveSuccesses = 0;
    int deathSaveFailures = 0;

    std::shared_ptr<Item> equippedWeapon;
    std::shared_ptr<Item> equippedArmor;

    Character(std::string n, CharacterClass c, std::string id = "local")
        : name(std::move(n)), uid(std::move(id)), characterClass(c), level(1), xp(0) {
        applyStatsForLevel();
        currentHp = maxHp;
        resources = maxResources;
        if (characterClass == CharacterClass::FIGHTER) {
            equippedWeapon = std::make_shared<Item>(Item{"Longsword", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Chain Shirt", ItemType::ARMOR, 3});
        } else if (characterClass == CharacterClass::ROGUE) {
            equippedWeapon = std::make_shared<Item>(Item{"Shortsword", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Leather Armor", ItemType::ARMOR, 1});
        } else if (characterClass == CharacterClass::WIZARD) {
            equippedWeapon = std::make_shared<Item>(Item{"Quarterstaff", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Traveler Clothes", ItemType::ARMOR, 0});
        } else {
            equippedWeapon = std::make_shared<Item>(Item{"Mace", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Scale Mail", ItemType::ARMOR, 4});
        }
        calculateAC();
    }

    void calculateAC() {
        int dexMod = Attributes::getModifier(attributes.dexterity);
        int armorBonus = (equippedArmor && equippedArmor->type == ItemType::ARMOR) ? equippedArmor->bonus : 0;
        armorClass = 10 + dexMod + armorBonus;
    }

    void applyStatsForLevel() {
        switch (characterClass) {
            case CharacterClass::FIGHTER:
                attributes = {16, 12, 14, 8, 10, 10};
                maxResources = 2 + (level / 2); // Action Surge uses
                break;
            case CharacterClass::WIZARD:
                attributes = {8, 14, 12, 16, 10, 10};
                maxResources = 2 + level; // spell slots (simplified)
                break;
            case CharacterClass::ROGUE:
                attributes = {10, 16, 12, 12, 10, 14};
                maxResources = 1 + (level / 3);
                break;
            case CharacterClass::CLERIC:
                attributes = {14, 10, 14, 10, 16, 12};
                maxResources = 2 + level;
                break;
        }
        int conMod = Attributes::getModifier(attributes.constitution);
        int hd = hitDie();
        // Level 1: max hit die + CON; later levels: average hit die + CON
        maxHp = (hd + conMod);
        for (int lvl = 2; lvl <= level; ++lvl) {
            int gain = (hd / 2 + 1) + conMod; if (gain < 1) gain = 1; maxHp += gain;
        }
    }

    bool addXp(int amount) {
        xp += amount;
        if (xp >= level * 100) {
            level++;
            xp -= (level - 1) * 100;
            pendingStatPoints += 2;
            applyStatsForLevel();
            currentHp = maxHp;
            resources = maxResources;
            return true;
        }
        return false;
    }

    void increaseAttribute(int index) {
        if (pendingStatPoints <= 0) return;
        switch(index) {
            case 0: attributes.strength++; break;
            case 1: attributes.dexterity++; break;
            case 2: attributes.constitution++; break;
            case 3: attributes.intelligence++; break;
            case 4: attributes.wisdom++; break;
            case 5: attributes.charisma++; break;
        }
        pendingStatPoints--;
        calculateAC();
        if (index == 2) { maxHp += level; currentHp += level; }
    }

    bool performSavingThrow(int attributeScore, int dc) {
        return (rand() % 20 + 1 + Attributes::getModifier(attributeScore)) >= dc;
    }

    // amountCritical: treat as a critical hit while dying (2 failures) — 5e style.
    void takeDamage(int amount, bool amountCritical = false) {
        if (amount <= 0) return;
        // Already dead: ignore further damage.
        if (isDead) return;
        // While dying (or stable at 0 HP): damage adds death-save failures, does not reset counters.
        if (isDowned || (currentHp <= 0 && isStable)) {
            if (isStable) {
                isStable = false; // damage knocks you unconscious / unstable again
                isDowned = true;
            }
            deathSaveFailures += amountCritical ? 2 : 1;
            if (deathSaveFailures >= 3) {
                isDead = true;
                isDowned = true;
                isStable = false;
                deathSaveFailures = 3;
            }
            currentHp = 0;
            return;
        }
        currentHp = std::max(0, currentHp - amount);
        if (currentHp == 0) {
            isDowned = true;
            isStable = false;
            isDead = false;
            deathSaveSuccesses = 0;
            deathSaveFailures = 0;
        }
    }

    void heal(int amount) {
        if (amount <= 0) return;
        if (isDead) return; // permanently dead — needs magic not modeled here
        isDowned = false;
        isStable = false;
        deathSaveSuccesses = 0;
        deathSaveFailures = 0;
        if (currentHp <= 0) {
            currentHp = std::min(maxHp, amount);
        } else {
            currentHp = std::min(maxHp, currentHp + amount);
        }
    }

    /** Party-wipe continue: clear death locks and set HP (difficulty-tuned by caller). */
    void reviveAfterWipe(int hp) {
        isDead = false;
        isDowned = false;
        isStable = false;
        deathSaveSuccesses = 0;
        deathSaveFailures = 0;
        currentHp = std::max(1, std::min(maxHp, hp));
    }

    /** Hard difficulty: strip gold + equipped gear back to class starters. Stats/level kept. */
    void stripGearAndGoldToStarters() {
        gold = 0;
        if (characterClass == CharacterClass::FIGHTER) {
            equippedWeapon = std::make_shared<Item>(Item{"Longsword", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Chain Shirt", ItemType::ARMOR, 3});
        } else if (characterClass == CharacterClass::ROGUE) {
            equippedWeapon = std::make_shared<Item>(Item{"Shortsword", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Leather Armor", ItemType::ARMOR, 1});
        } else if (characterClass == CharacterClass::WIZARD) {
            equippedWeapon = std::make_shared<Item>(Item{"Quarterstaff", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Traveler Clothes", ItemType::ARMOR, 0});
        } else {
            equippedWeapon = std::make_shared<Item>(Item{"Mace", ItemType::WEAPON, 0});
            equippedArmor = std::make_shared<Item>(Item{"Scale Mail", ItemType::ARMOR, 4});
        }
        calculateAC();
    }

    std::string getDetailedSheet() const {
        std::stringstream ss;
        ss << "--- " << name << " ---\n";
        ss << "Lvl " << level << " " << getClassName() << " | Gold: " << gold << "\n";
        if (pendingStatPoints > 0) ss << "POINTS TO SPEND: " << pendingStatPoints << "\n";
        ss << "HP: " << currentHp << "/" << maxHp << " | AC: " << armorClass << "\n";
        ss << "Resources: " << resources << "/" << maxResources << "\n";
        ss << "STR: " << attributes.strength << " (" << showMod(attributes.strength) << ")\n";
        ss << "DEX: " << attributes.dexterity << " (" << showMod(attributes.dexterity) << ")\n";
        ss << "CON: " << attributes.constitution << " (" << showMod(attributes.constitution) << ")\n";
        ss << "INT: " << attributes.intelligence << " (" << showMod(attributes.intelligence) << ")\n";
        ss << "WIS: " << attributes.wisdom << " (" << showMod(attributes.wisdom) << ")\n";
        ss << "CHA: " << attributes.charisma << " (" << showMod(attributes.charisma) << ")\n";
        ss << "Weapon: " << (equippedWeapon ? equippedWeapon->getDescription() : "None") << "\n";
        ss << "Armor: " << (equippedArmor ? equippedArmor->getDescription() : "None") << "\n";
        return ss.str();
    }

    std::string getClassName() const {
        switch(characterClass) {
            case CharacterClass::FIGHTER: return "Fighter";
            case CharacterClass::WIZARD: return "Wizard";
            case CharacterClass::ROGUE: return "Rogue";
            case CharacterClass::CLERIC: return "Cleric";
        }
        return "Unknown";
    }

    static std::string showMod(int score) {
        int mod = Attributes::getModifier(score);
        return (mod >= 0 ? "+" : "") + std::to_string(mod);
    }

    static std::string getPrimaryStatName(CharacterClass c) {
        if (c == CharacterClass::WIZARD) return "Intelligence";
        if (c == CharacterClass::ROGUE) return "Dexterity";
        if (c == CharacterClass::CLERIC) return "Wisdom";
        return "Strength";
    }

    std::string getSpecialAbilityName() const {
        if (characterClass == CharacterClass::FIGHTER) return "Action Surge";
        if (characterClass == CharacterClass::WIZARD) return "Magic Missile";
        if (characterClass == CharacterClass::ROGUE) return "Sneak Attack";
        return "Healing Word";
    }

    int hitDie() const {
        if (characterClass == CharacterClass::FIGHTER) return 10;
        if (characterClass == CharacterClass::WIZARD) return 6;
        if (characterClass == CharacterClass::ROGUE) return 8;
        return 8; // cleric
    }

    int weaponDamageDie() const {
        // SRD-flavored starter weapons
        if (characterClass == CharacterClass::FIGHTER) return 8;  // longsword
        if (characterClass == CharacterClass::ROGUE) return 6;    // shortsword
        if (characterClass == CharacterClass::WIZARD) return 6;   // quarterstaff
        return 6; // mace
    }

    int sneakAttackDice() const {
        // 5e: 1d6 at 1st, +1d6 every odd level
        return 1 + (level - 1) / 2;
    }
};

struct RollResult {
    int total;
    int dieRoll;
    bool isCriticalHit;
    bool isCriticalFail;
};

class CombatSystem {
public:
    static int getPrimaryModifier(const Character& c) {
        if (c.characterClass == CharacterClass::WIZARD) return Attributes::getModifier(c.attributes.intelligence);
        if (c.characterClass == CharacterClass::ROGUE) return Attributes::getModifier(c.attributes.dexterity);
        if (c.characterClass == CharacterClass::CLERIC) return Attributes::getModifier(c.attributes.wisdom);
        return Attributes::getModifier(c.attributes.strength);
    }

    static int proficiencyBonusForLevel(int level) {
        // 5e proficiency: 2 at 1-4, 3 at 5-8, ...
        return 2 + (level - 1) / 4;
    }

    static RollResult performAttackRoll(const Character& attacker) {
        int die = rand() % 20 + 1;
        int mod = getPrimaryModifier(attacker);
        int pb = proficiencyBonusForLevel(attacker.level);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.equippedWeapon->bonus : 0;
        return { die + mod + pb + weaponBonus, die, (die == 20), (die == 1) };
    }

    static int calculateDamage(const Character& attacker, bool isCritical, int extraDice = 0, int extraSides = 6) {
        int sides = attacker.weaponDamageDie();
        int damage = (rand() % sides + 1);
        if (isCritical) damage += (rand() % sides + 1); // 5e: double damage dice only
        for (int i = 0; i < extraDice; ++i) {
            damage += (rand() % extraSides + 1);
            if (isCritical) damage += (rand() % extraSides + 1);
        }
        int mod = getPrimaryModifier(attacker);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.equippedWeapon->bonus : 0;
        int total = damage + mod + weaponBonus;
        return total < 1 ? 1 : total;
    }
};

class LootSystem {
public:
    static std::shared_ptr<Item> generateLoot(int roomDepth) {
        int roll = rand() % 100;
        if (roll > 80) {
            int bonus = (roomDepth / 5) + 1;
            return std::make_shared<Item>(Item{"Steel Sword", ItemType::WEAPON, bonus});
        } else if (roll > 60) {
            int bonus = (roomDepth / 6) + 1;
            return std::make_shared<Item>(Item{"Chainmail", ItemType::ARMOR, bonus});
        }
        return nullptr;
    }
};

class SkillSystem {
public:
    static bool performCheck(const Character& c, int attributeScore, int dc) {
        return (rand() % 20 + 1 + Attributes::getModifier(attributeScore)) >= dc;
    }
};

} // namespace dnd

#endif // DND_GAME_LOGIC_H
