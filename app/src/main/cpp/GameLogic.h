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
    int deathSaveSuccesses = 0;
    int deathSaveFailures = 0;

    std::shared_ptr<Item> equippedWeapon;
    std::shared_ptr<Item> equippedArmor;

    Character(std::string n, CharacterClass c, std::string id = "local")
        : name(std::move(n)), uid(std::move(id)), characterClass(c), level(1), xp(0) {
        applyStatsForLevel();
        currentHp = maxHp;
        resources = maxResources;
        calculateAC();
        equippedWeapon = std::make_shared<Item>(Item{"Rusty Sword", ItemType::WEAPON, 0});
        equippedArmor = std::make_shared<Item>(Item{"Tattered Rags", ItemType::ARMOR, 0});
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
                maxHp = 10 + (level * 6);
                maxResources = 2 + (level / 2);
                break;
            case CharacterClass::WIZARD:
                attributes = {8, 14, 12, 16, 10, 10};
                maxHp = 6 + (level * 4);
                maxResources = 2 + level;
                break;
            case CharacterClass::ROGUE:
                attributes = {10, 16, 12, 12, 10, 14};
                maxHp = 8 + (level * 5);
                maxResources = 1 + (level / 3);
                break;
            case CharacterClass::CLERIC:
                attributes = {14, 10, 14, 10, 16, 12};
                maxHp = 8 + (level * 5);
                maxResources = 2 + level;
                break;
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

    void takeDamage(int amount) {
        currentHp = std::max(0, currentHp - amount);
        if (currentHp == 0) {
            isDowned = true;
            deathSaveSuccesses = 0;
            deathSaveFailures = 0;
        }
    }

    void heal(int amount) {
        isDowned = false;
        currentHp = std::min(maxHp, currentHp + amount);
    }

    std::string getDetailedSheet() const {
        std::stringstream ss;
        ss << "--- " << name << " ---\n";
        ss << "Lvl " << level << " " << getClassName() << " | Gold: " << gold << "\n";
        if (pendingStatPoints > 0) ss << "POINTS TO SPEND: " << pendingStatPoints << "\n";
        ss << "HP: " << currentHp << "/" << maxHp << " | AC: " << armorClass << "\n";
        ss << "STR: " << attributes.strength << " (" << showMod(attributes.strength) << ")\n";
        ss << "DEX: " << attributes.dexterity << " (" << showMod(attributes.dexterity) << ")\n";
        ss << "INT: " << attributes.intelligence << " (" << showMod(attributes.intelligence) << ")\n";
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
        if (characterClass == CharacterClass::WIZARD) return "Fireball";
        if (characterClass == CharacterClass::ROGUE) return "Sneak Attack";
        return "Healing Word";
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

    static RollResult performAttackRoll(const Character& attacker) {
        int die = rand() % 20 + 1;
        int mod = getPrimaryModifier(attacker);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.equippedWeapon->bonus : 0;
        return { die + mod + weaponBonus, die, (die == 20), (die == 1) };
    }

    static int calculateDamage(const Character& attacker, bool isCritical) {
        int die = (attacker.characterClass == CharacterClass::FIGHTER) ? 10 : 8;
        int damage = rand() % die + 1;
        if (isCritical) damage += rand() % die + 1;
        int mod = getPrimaryModifier(attacker);
        int weaponBonus = (attacker.equippedWeapon && attacker.equippedWeapon->type == ItemType::WEAPON) ? attacker.equippedWeapon->bonus : 0;
        return std::max(1, damage + mod + weaponBonus);
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
