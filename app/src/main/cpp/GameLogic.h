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

enum class ItemRarity {
    COMMON = 0,
    UNCOMMON = 1,
    RARE = 2,
    EPIC = 3,
    LEGENDARY = 4
};

/** Legendary sub-effect (unlocks at upgrade 20+). Persist as int id. */
enum class LegendarySubEffect : int {
    NONE = 0,
    LIFESTEAL = 1,      // heal 1 + upgrade/10 on hit (weapon)
    ON_HIT_HEAL = 2,    // flat +2 HP on hit (weapon)
    FIGHT_SHIELD = 3    // once-per-fight absorb ~5 HP (armor)
};

/** classTag: -1 = any class; else CharacterClass ordinal.
 *  Legendary: legStat0/1/2 = attribute indices (0=STR..5=CHA), -1 unused.
 *  subEffect unlocks visually/mechanically at upgradeLevel >= 20.
 */
struct Item {
    std::string name;
    ItemType type;
    int bonus = 0;
    ItemRarity rarity = ItemRarity::COMMON;
    int classTag = -1;
    int upgradeLevel = 0;
    int legStat0 = -1;
    int legStat1 = -1;
    int legStat2 = -1;
    int subEffect = 0; // LegendarySubEffect

    static const char* rarityName(ItemRarity r) {
        switch (r) {
            case ItemRarity::COMMON: return "Common";
            case ItemRarity::UNCOMMON: return "Uncommon";
            case ItemRarity::RARE: return "Rare";
            case ItemRarity::EPIC: return "Epic";
            case ItemRarity::LEGENDARY: return "Legendary";
        }
        return "Common";
    }

    static const char* attrShort(int idx) {
        switch (idx) {
            case 0: return "STR";
            case 1: return "DEX";
            case 2: return "CON";
            case 3: return "INT";
            case 4: return "WIS";
            case 5: return "CHA";
            default: return "?";
        }
    }

    static const char* subEffectName(int id) {
        switch (static_cast<LegendarySubEffect>(id)) {
            case LegendarySubEffect::LIFESTEAL: return "Lifesteal (on hit)";
            case LegendarySubEffect::ON_HIT_HEAL: return "On-hit heal";
            case LegendarySubEffect::FIGHT_SHIELD: return "Once-per-fight shield";
            default: return "";
        }
    }

    /** Upgrade level cap by rarity — Legendary reaches 20+ for full power. */
    int maxUpgradeLevel() const {
        switch (rarity) {
            case ItemRarity::COMMON: return 5;
            case ItemRarity::UNCOMMON: return 8;
            case ItemRarity::RARE: return 12;
            case ItemRarity::EPIC: return 16;
            case ItemRarity::LEGENDARY: return 25;
        }
        return 5;
    }

    /** How many legendary bonus stats are active at current upgrade. */
    int activeLegStatCount() const {
        if (rarity != ItemRarity::LEGENDARY) return 0;
        if (upgradeLevel >= 20) return 3;
        if (upgradeLevel >= 15) return 2;
        return 1;
    }

    bool subEffectUnlocked() const {
        return rarity == ItemRarity::LEGENDARY && upgradeLevel >= 20 && subEffect > 0;
    }

    /** +1 per active legendary bonus stat matching this attribute index. */
    int legendaryAttrBonus(int attrIndex) const {
        if (rarity != ItemRarity::LEGENDARY) return 0;
        int n = activeLegStatCount();
        int add = 0;
        if (n >= 1 && legStat0 == attrIndex) add++;
        if (n >= 2 && legStat1 == attrIndex) add++;
        if (n >= 3 && legStat2 == attrIndex) add++;
        return add;
    }

    std::string legendaryBonusText() const {
        if (rarity != ItemRarity::LEGENDARY) return "";
        std::stringstream ss;
        int n = activeLegStatCount();
        ss << "Bonus stats:";
        if (n >= 1 && legStat0 >= 0) ss << " +" << attrShort(legStat0);
        if (n >= 2 && legStat1 >= 0) ss << " +" << attrShort(legStat1);
        if (n >= 3 && legStat2 >= 0) ss << " +" << attrShort(legStat2);
        if (n < 3) {
            ss << " (next at +" << (n == 1 ? 15 : 20) << ")";
        }
        if (subEffectUnlocked()) {
            ss << " | " << subEffectName(subEffect);
        } else if (rarity == ItemRarity::LEGENDARY && subEffect > 0) {
            ss << " | " << subEffectName(subEffect) << " (unlocks at +20)";
        }
        return ss.str();
    }

    static const char* classTagName(int tag) {
        switch (tag) {
            case 0: return "Fighter";
            case 1: return "Wizard";
            case 2: return "Rogue";
            case 3: return "Cleric";
            default: return "Any";
        }
    }

    std::string rarityLabel() const { return rarityName(rarity); }
    std::string classLabel() const { return classTagName(classTag); }

    bool canEquip(CharacterClass c) const {
        if (type == ItemType::POTION) return false;
        if (classTag < 0) return true;
        return classTag == static_cast<int>(c);
    }

    int shopCost() const {
        int rarityMult = 1 + static_cast<int>(rarity);
        int base = (bonus + 1) * 20 * rarityMult;
        if (type == ItemType::POTION) return std::max(15, bonus * 3);
        if (classTag >= 0) base += 10; // class-tagged premium
        return std::max(15, base);
    }

    /** Gold to raise this item one upgrade tier (+1 bonus). */
    int upgradeCost() const {
        int rarityBase = 15 * (1 + static_cast<int>(rarity));
        return rarityBase * (upgradeLevel + 1) * (1 + bonus / 2);
    }

    /** Sell-back gold: fair fraction of shop value, scaled by rarity + upgrades (~40–60%). */
    int sellPrice() const {
        int buy = shopCost();
        // Common 40% … Legendary 60% base; +2% per upgrade tier, cap 65%.
        int pct = 40 + static_cast<int>(rarity) * 5 + upgradeLevel * 2;
        if (pct > 65) pct = 65;
        int price = (buy * pct) / 100;
        return std::max(1, price);
    }

    std::string getDescription() const {
        std::stringstream ss;
        ss << name;
        if (bonus != 0) ss << " (+" << bonus << ")";
        if (upgradeLevel > 0) ss << " [+" << upgradeLevel << "]";
        if (rarity == ItemRarity::LEGENDARY) ss << " ★";
        return ss.str();
    }

    /** Persist: name:bonus:W|A|P:rarity:classTag:upgrade:leg0:leg1:leg2:subEffect */
    std::string toToken() const {
        char t = 'W';
        if (type == ItemType::ARMOR) t = 'A';
        else if (type == ItemType::POTION) t = 'P';
        std::stringstream ss;
        ss << name << ":" << bonus << ":" << t << ":"
           << static_cast<int>(rarity) << ":" << classTag << ":" << upgradeLevel
           << ":" << legStat0 << ":" << legStat1 << ":" << legStat2 << ":" << subEffect;
        return ss.str();
    }

    static std::shared_ptr<Item> fromToken(const std::string& tok) {
        if (tok.empty() || tok == "None" || tok.rfind("None:", 0) == 0) return nullptr;
        std::vector<std::string> parts;
        std::string cur;
        for (char c : tok) {
            if (c == ':') { parts.push_back(cur); cur.clear(); }
            else cur.push_back(c);
        }
        parts.push_back(cur);
        if (parts.size() < 3) return nullptr;
        auto item = std::make_shared<Item>();
        item->name = parts[0];
        try { item->bonus = std::stoi(parts[1]); } catch (...) { item->bonus = 0; }
        char t = parts[2].empty() ? 'W' : parts[2][0];
        if (t == 'A') item->type = ItemType::ARMOR;
        else if (t == 'P') item->type = ItemType::POTION;
        else item->type = ItemType::WEAPON;
        if (parts.size() >= 4) {
            try { item->rarity = static_cast<ItemRarity>(std::stoi(parts[3])); } catch (...) {}
        }
        if (parts.size() >= 5) {
            try { item->classTag = std::stoi(parts[4]); } catch (...) { item->classTag = -1; }
        }
        if (parts.size() >= 6) {
            try { item->upgradeLevel = std::stoi(parts[5]); } catch (...) { item->upgradeLevel = 0; }
        }
        if (parts.size() >= 7) {
            try { item->legStat0 = std::stoi(parts[6]); } catch (...) { item->legStat0 = -1; }
        }
        if (parts.size() >= 8) {
            try { item->legStat1 = std::stoi(parts[7]); } catch (...) { item->legStat1 = -1; }
        }
        if (parts.size() >= 9) {
            try { item->legStat2 = std::stoi(parts[8]); } catch (...) { item->legStat2 = -1; }
        }
        if (parts.size() >= 10) {
            try { item->subEffect = std::stoi(parts[9]); } catch (...) { item->subEffect = 0; }
        }
        if (static_cast<int>(item->rarity) < 0 || static_cast<int>(item->rarity) > 4)
            item->rarity = ItemRarity::COMMON;
        return item;
    }

    static void rollLegendaryExtras(Item& i) {
        if (i.rarity != ItemRarity::LEGENDARY) return;
        // Three distinct bonus stats
        int pool[6] = {0,1,2,3,4,5};
        for (int k = 5; k > 0; --k) {
            int j = rand() % (k + 1);
            int tmp = pool[k]; pool[k] = pool[j]; pool[j] = tmp;
        }
        i.legStat0 = pool[0];
        i.legStat1 = pool[1];
        i.legStat2 = pool[2];
        if (i.type == ItemType::ARMOR) {
            i.subEffect = static_cast<int>(LegendarySubEffect::FIGHT_SHIELD);
        } else {
            i.subEffect = (rand() % 2 == 0)
                ? static_cast<int>(LegendarySubEffect::LIFESTEAL)
                : static_cast<int>(LegendarySubEffect::ON_HIT_HEAL);
        }
    }

    static std::shared_ptr<Item> make(const std::string& n, ItemType t, int b,
                                      ItemRarity r = ItemRarity::COMMON, int cls = -1) {
        auto i = std::make_shared<Item>();
        i->name = n; i->type = t; i->bonus = b; i->rarity = r; i->classTag = cls;
        if (r == ItemRarity::LEGENDARY) rollLegendaryExtras(*i);
        return i;
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
    /** Solo NPC companion: true = existing ally AI; false = human chooses actions on their turn. */
    bool aiControlled = false;

    std::shared_ptr<Item> equippedWeapon;
    std::shared_ptr<Item> equippedArmor;
    std::vector<std::shared_ptr<Item>> inventory;

    Character(std::string n, CharacterClass c, std::string id = "local")
        : name(std::move(n)), uid(std::move(id)), characterClass(c), level(1), xp(0) {
        // Base class arrays once; never re-applied on level-up (spent points must stick).
        switch (characterClass) {
            case CharacterClass::FIGHTER: attributes = {16, 12, 14, 8, 10, 10}; break;
            case CharacterClass::WIZARD:  attributes = {8, 14, 12, 16, 10, 10}; break;
            case CharacterClass::ROGUE:   attributes = {10, 16, 12, 12, 10, 14}; break;
            case CharacterClass::CLERIC:  attributes = {14, 10, 14, 10, 16, 12}; break;
        }
        applyStatsForLevel();
        currentHp = maxHp;
        resources = maxResources;
        giveStarterGear();
        calculateAC();
    }

    void giveStarterGear() {
        inventory.clear();
        const int cls = static_cast<int>(characterClass);
        if (characterClass == CharacterClass::FIGHTER) {
            equippedWeapon = Item::make("Longsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Chain Shirt", ItemType::ARMOR, 3, ItemRarity::COMMON, cls);
        } else if (characterClass == CharacterClass::ROGUE) {
            equippedWeapon = Item::make("Shortsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
        } else if (characterClass == CharacterClass::WIZARD) {
            equippedWeapon = Item::make("Quarterstaff", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Traveler Clothes", ItemType::ARMOR, 0, ItemRarity::COMMON, -1);
        } else {
            equippedWeapon = Item::make("Mace", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            equippedArmor = Item::make("Scale Mail", ItemType::ARMOR, 4, ItemRarity::COMMON, cls);
        }
    }

    void calculateAC() {
        int dexMod = Attributes::getModifier(effectiveAttr(1));
        int armorBonus = (equippedArmor && equippedArmor->type == ItemType::ARMOR) ? equippedArmor->bonus : 0;
        armorClass = 10 + dexMod + armorBonus;
    }

    /** Recompute HP/resources from current level + attributes. Does NOT reset attributes. */
    void applyStatsForLevel() {
        switch (characterClass) {
            case CharacterClass::FIGHTER:
                maxResources = 2 + (level / 2); // Action Surge uses
                break;
            case CharacterClass::WIZARD:
                maxResources = 2 + level; // spell slots (simplified)
                break;
            case CharacterClass::ROGUE:
                maxResources = 1 + (level / 3);
                break;
            case CharacterClass::CLERIC:
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
        inventory.clear();
        giveStarterGear();
        calculateAC();
    }

    void addToInventory(const std::shared_ptr<Item>& item) {
        if (item) inventory.push_back(item);
    }

    bool upgradeOwnedItem(std::shared_ptr<Item>& item, std::string& err) {
        if (!item || item->type == ItemType::POTION) {
            err = "Cannot upgrade that.";
            return false;
        }
        if (item->upgradeLevel >= item->maxUpgradeLevel()) {
            err = "Max upgrade (+" + std::to_string(item->maxUpgradeLevel()) + ") for "
                + item->rarityLabel() + ".";
            return false;
        }
        int cost = item->upgradeCost();
        if (gold < cost) {
            err = "Not enough gold! Need " + std::to_string(cost) + "g.";
            return false;
        }
        gold -= cost;
        item->bonus += 1;
        item->upgradeLevel += 1;
        if (item->type == ItemType::ARMOR) calculateAC();
        err.clear();
        return true;
    }

    /** Sum of legendary attr bonuses from equipped weapon+armor. */
    int legendaryAttrBonus(int attrIndex) const {
        int add = 0;
        if (equippedWeapon) add += equippedWeapon->legendaryAttrBonus(attrIndex);
        if (equippedArmor) add += equippedArmor->legendaryAttrBonus(attrIndex);
        return add;
    }

    int effectiveAttr(int which) const {
        int base = 0;
        switch (which) {
            case 0: base = attributes.strength; break;
            case 1: base = attributes.dexterity; break;
            case 2: base = attributes.constitution; break;
            case 3: base = attributes.intelligence; break;
            case 4: base = attributes.wisdom; break;
            case 5: base = attributes.charisma; break;
            default: return 10;
        }
        return base + legendaryAttrBonus(which);
    }

    /** Once-per-fight shield charges (armor Legendary sub-effect). Not persisted mid-fight across saves. */
    mutable bool fightShieldUsed = false;

    std::string getDetailedSheet() const {
        std::stringstream ss;
        ss << "--- " << name << " ---\n";
        ss << "Lvl " << level << " " << getClassName() << " | Gold: " << gold << "\n";
        if (pendingStatPoints > 0) ss << "POINTS TO SPEND: " << pendingStatPoints << "\n";
        ss << "HP: " << currentHp << "/" << maxHp << " | AC: " << armorClass << "\n";
        ss << "XP: " << xp << "/" << xpToNextLevel() << " (to Lvl " << (level + 1) << ")\n";
        ss << "Resources: " << resources << "/" << maxResources << "\n";
        ss << "STR: " << attributes.strength << " (" << showMod(attributes.strength) << ")\n";
        ss << "DEX: " << attributes.dexterity << " (" << showMod(attributes.dexterity) << ")\n";
        ss << "CON: " << attributes.constitution << " (" << showMod(attributes.constitution) << ")\n";
        ss << "INT: " << attributes.intelligence << " (" << showMod(attributes.intelligence) << ")\n";
        ss << "WIS: " << attributes.wisdom << " (" << showMod(attributes.wisdom) << ")\n";
        ss << "CHA: " << attributes.charisma << " (" << showMod(attributes.charisma) << ")\n";
        ss << "Weapon: " << (equippedWeapon ? equippedWeapon->getDescription() + " [" + equippedWeapon->rarityLabel() + "/" + equippedWeapon->classLabel() + "]" : "None") << "\n";
        ss << "Armor: " << (equippedArmor ? equippedArmor->getDescription() + " [" + equippedArmor->rarityLabel() + "/" + equippedArmor->classLabel() + "]" : "None") << "\n";
        ss << "Bag: " << inventory.size() << " item(s)\n";
        return ss.str();
    }

    /** XP required to leave the current level (matches addXp threshold). */
    int xpToNextLevel() const { return level * 100; }

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
        if (c.characterClass == CharacterClass::WIZARD) return Attributes::getModifier(c.effectiveAttr(3));
        if (c.characterClass == CharacterClass::ROGUE) return Attributes::getModifier(c.effectiveAttr(1));
        if (c.characterClass == CharacterClass::CLERIC) return Attributes::getModifier(c.effectiveAttr(4));
        return Attributes::getModifier(c.effectiveAttr(0));
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
    /**
     * luckBonus: 0 normal; bosses add mild luck (see noteBossDefeat).
     * Drop chance and rarity use separate rolls (#46 hotfix): trash stays nerfed;
     * bosses better than trash but not BiS-guaranteed.
     * endgameUnlocked: Acts 1–3 complete — required for Legendary.
     * preferClass / preferClass2: party class tags (-1 = unused).
     */
    static std::shared_ptr<Item> generateLoot(int roomDepth, int luckBonus = 0,
                                              int preferClass = -1, int preferClass2 = -1,
                                              bool endgameUnlocked = false) {
        // Drop gate (shared luck) — ~42% item on luck=0
        int dropRoll = (rand() % 100) + luckBonus;
        if (dropRoll < 58) return nullptr;

        // Rarity: luck only adds luckBonus/3 so bosses stay better than trash, not BiS flood.
        int rarityRoll = (rand() % 100) + (luckBonus / 3);
        ItemRarity rarity = ItemRarity::COMMON;
        // Legendary: endgame + strong luck only (deep boss / raid)
        if (endgameUnlocked && luckBonus >= 28 && rarityRoll >= 104) {
            rarity = ItemRarity::LEGENDARY;
        } else if (endgameUnlocked && luckBonus >= 40 && rarityRoll >= 99) {
            rarity = ItemRarity::LEGENDARY;
        } else if (rarityRoll >= 98) {
            rarity = ItemRarity::EPIC;       // #46: was shared-roll >=95
        } else if (rarityRoll >= 90) {
            rarity = ItemRarity::RARE;      // #46: was >=82
        } else if (rarityRoll >= 70) {
            rarity = ItemRarity::UNCOMMON;  // #46: was >=68
        }
        if (!endgameUnlocked && rarity == ItemRarity::LEGENDARY) {
            rarity = ItemRarity::EPIC;
        }

        int bonus = (roomDepth / 5) + static_cast<int>(rarity);
        if (rarity == ItemRarity::LEGENDARY) bonus += 2;
        if (bonus < 0) bonus = 0;
        bool weapon = (rand() % 2) == 0;

        int party[4];
        int partyN = 0;
        auto pushParty = [&](int c) {
            if (c < 0 || c > 3) return;
            for (int i = 0; i < partyN; ++i) if (party[i] == c) return;
            party[partyN++] = c;
        };
        pushParty(preferClass);
        pushParty(preferClass2);

        int cls = -1;
        int pick = rand() % 100;
        if (partyN > 0) {
            if (pick < 62) cls = party[rand() % partyN];
            else if (pick < 92) cls = -1;
            else {
                int off[4];
                int offN = 0;
                for (int c = 0; c < 4; ++c) {
                    bool in = false;
                    for (int i = 0; i < partyN; ++i) if (party[i] == c) { in = true; break; }
                    if (!in) off[offN++] = c;
                }
                cls = (offN > 0) ? off[rand() % offN] : -1;
            }
        } else {
            if (pick < 40) cls = rand() % 4;
            else cls = -1;
        }

        if (rarity == ItemRarity::LEGENDARY) {
            if (weapon) {
                if (cls == 0) return Item::make("Crownbreaker", ItemType::WEAPON, bonus, rarity, 0);
                if (cls == 1) return Item::make("Starfall Focus", ItemType::WEAPON, bonus, rarity, 1);
                if (cls == 2) return Item::make("Nightwhisper", ItemType::WEAPON, bonus, rarity, 2);
                if (cls == 3) return Item::make("Dawnward Mace", ItemType::WEAPON, bonus, rarity, 3);
                return Item::make("Emberdeep Relic Blade", ItemType::WEAPON, bonus, rarity, -1);
            } else {
                if (cls == 0) return Item::make("Aegis of Millhollow", ItemType::ARMOR, bonus + 3, rarity, 0);
                if (cls == 1) return Item::make("Archsage Mantle", ItemType::ARMOR, bonus + 1, rarity, 1);
                if (cls == 2) return Item::make("Veilwalker Hide", ItemType::ARMOR, bonus + 2, rarity, 2);
                if (cls == 3) return Item::make("Sanctum Plate", ItemType::ARMOR, bonus + 3, rarity, 3);
                return Item::make("Emberdeep Relic Mail", ItemType::ARMOR, bonus + 2, rarity, -1);
            }
        }

        if (weapon) {
            const char* names[] = {"Steel Blade", "Runed Blade", "Shadow Dirk", "War Maul", "Arcane Wand"};
            int ni = rand() % 5;
            if (cls == 0) return Item::make("Champion's Longsword", ItemType::WEAPON, bonus, rarity, 0);
            if (cls == 1) return Item::make("Focus Staff", ItemType::WEAPON, bonus, rarity, 1);
            if (cls == 2) return Item::make("Silent Shortsword", ItemType::WEAPON, bonus, rarity, 2);
            if (cls == 3) return Item::make("Hallowed Mace", ItemType::WEAPON, bonus, rarity, 3);
            return Item::make(names[ni], ItemType::WEAPON, bonus, rarity, -1);
        } else {
            if (cls == 0) return Item::make("Bulwark Mail", ItemType::ARMOR, bonus + 2, rarity, 0);
            if (cls == 1) return Item::make("Scholar Robes", ItemType::ARMOR, bonus, rarity, 1);
            if (cls == 2) return Item::make("Shadow Leathers", ItemType::ARMOR, bonus + 1, rarity, 2);
            if (cls == 3) return Item::make("Temple Vestments", ItemType::ARMOR, bonus + 2, rarity, 3);
            return Item::make("Reinforced Mail", ItemType::ARMOR, bonus + 1, rarity, -1);
        }
    }

    static bool isBossName(const std::string& name) {
        return name.find("Goblin King") != std::string::npos
            || name.find("Skeleton King") != std::string::npos
            || name.find("Ashen Drake") != std::string::npos
            || name.find("Dragon") != std::string::npos
            || name.find("Hollow Crown") != std::string::npos
            || name.find("Ember Hydra") != std::string::npos
            || name.find("Nightfang Matriarch") != std::string::npos
            || name.find("Breach Warden") != std::string::npos;
    }

    static int bossTier(const std::string& name) {
        if (name.find("Hollow Crown") != std::string::npos) return 4;
        if (name.find("Ember Hydra") != std::string::npos) return 5;
        if (name.find("Nightfang Matriarch") != std::string::npos) return 4;
        if (name.find("Ashen Drake") != std::string::npos || name.find("Dragon") != std::string::npos) return 3;
        if (name.find("Breach Warden") != std::string::npos) return 3;
        if (name.find("Skeleton King") != std::string::npos) return 2;
        if (name.find("Goblin King") != std::string::npos) return 1;
        return 0;
    }

    static bool isEndgameBossName(const std::string& name) {
        return name.find("Hollow Crown") != std::string::npos
            || name.find("Ember Hydra") != std::string::npos
            || name.find("Nightfang Matriarch") != std::string::npos;
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
