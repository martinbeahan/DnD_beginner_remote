#include "Game.h"
#include "AndroidOut.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

namespace dnd {

Game::Game() : currentTurnIndex_(0), turnCounter_(0), roomCount_(0), gameOver_(false), isHost_(false), isMerchantRoom_(false) {
    std::random_device rd;
    rng_.seed(rd());

    std::stringstream ss;
    ss << "DND-" << std::hex << std::uppercase << getRandomInt(0, 0xFFFF);
    sessionId_ = ss.str();

    startNewGame();
}

void Game::startNewGame(CharacterClass selectedClass, const std::string& playerName) {
    players_.clear();
    enemies_.clear();
    turnOrder_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    while(!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 1;

    players_.push_back(std::make_unique<Character>(playerName, selectedClass, "local-player"));
    addAlly("Melf (NPC)", CharacterClass::WIZARD);

    generateRoomDescription();
    spawnRoomContent();
    rollInitiative();

    lastEvent_ = "Adventure Started! Room 1. Roll for Initiative!";
    addChatMessage("System", "A new party has entered the dungeon.");
    addJournalEntry("The party entered the dungeon.");
}

void Game::addAlly(const std::string& name, CharacterClass cl) {
    for (const auto& p : players_) {
        if (p->name == name) return;
    }
    std::string uid = "ally-" + std::to_string(players_.size());
    players_.push_back(std::make_unique<Character>(name, cl, uid));
    rebuildTurnOrder();
}

void Game::generateRoomDescription() {
    std::vector<std::string> adjectives = {"dark", "damp", "ancient", "dusty", "eerie", "forgotten", "cursed"};
    std::vector<std::string> rooms = {"chamber", "hallway", "library", "crypt", "vault", "shrine", "laboratory"};
    std::stringstream ss;
    ss << "You stand in a " << adjectives[static_cast<size_t>(getRandomInt(0, static_cast<int>(adjectives.size()) - 1))]
       << " " << rooms[static_cast<size_t>(getRandomInt(0, static_cast<int>(rooms.size()) - 1))] << ". ";
    roomDescription_ = ss.str();
}

void Game::spawnRoomContent() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;

    if (roomCount_ > 1 && roomCount_ % 4 == 0) {
        isMerchantRoom_ = true;
        roomDescription_ = "You find a rare pocket of safety. A weary Merchant awaits.";
        shopInventory_.push_back(std::make_shared<Item>(Item{"Steel Blade", ItemType::WEAPON, (roomCount_ / 4)}));
        shopInventory_.push_back(std::make_shared<Item>(Item{"Platemail", ItemType::ARMOR, (roomCount_ / 4)}));
        shopInventory_.push_back(std::make_shared<Item>(Item{"Greater Potion", ItemType::POTION, 10}));
        return;
    }

    auto goblin = std::make_unique<Character>("Goblin", CharacterClass::ROGUE, "goblin-" + std::to_string(getRandomInt(0, 1000000)));
    goblin->maxHp += (roomCount_ * 2);
    goblin->currentHp = goblin->maxHp;
    enemies_.push_back(std::move(goblin));

    if (roomCount_ > 5) {
        auto skeleton = std::make_unique<Character>("Skeleton", CharacterClass::FIGHTER, "skeleton-" + std::to_string(getRandomInt(0, 1000000)));
        skeleton->maxHp += roomCount_;
        skeleton->currentHp = skeleton->maxHp;
        skeleton->armorClass += (roomCount_ / 10);
        enemies_.push_back(std::move(skeleton));
    }
}

void Game::rollInitiative() {
    turnOrder_.clear();
    if (isMerchantRoom_) return;

    for (auto& p : players_) {
        p->initiative = (getRandomInt(1, 20)) + Attributes::getModifier(p->attributes.dexterity);
        turnOrder_.push_back(p.get());
    }
    for (auto& e : enemies_) {
        e->initiative = (getRandomInt(1, 20)) + Attributes::getModifier(e->attributes.dexterity);
        turnOrder_.push_back(e.get());
    }
    std::sort(turnOrder_.begin(), turnOrder_.end(), [](Character* a, Character* b) {
        return a->initiative > b->initiative;
    });
    currentTurnIndex_ = 0;
}

void Game::rebuildTurnOrder() {
    std::string currentUid = (turnOrder_.empty() || currentTurnIndex_ < 0 || static_cast<size_t>(currentTurnIndex_) >= turnOrder_.size()) ? "" : turnOrder_[static_cast<size_t>(currentTurnIndex_)]->uid;

    turnOrder_.clear();
    for (auto& p : players_) turnOrder_.push_back(p.get());
    for (auto& e : enemies_) turnOrder_.push_back(e.get());

    std::sort(turnOrder_.begin(), turnOrder_.end(), [](Character* a, Character* b) {
        return a->initiative > b->initiative;
    });

    if (!currentUid.empty()) {
        for (size_t i = 0; i < turnOrder_.size(); ++i) {
            if (turnOrder_[i]->uid == currentUid) {
                currentTurnIndex_ = static_cast<int>(i);
                return;
            }
        }
    }
    currentTurnIndex_ = 0;
}

Character* Game::findCharacter(const std::string& name) {
    for (auto& p : players_) {
        if (p->name == name) return p.get();
    }
    return nullptr;
}

void Game::buyItem(const std::string& playerName, int itemIndex) {
    if (!isMerchantRoom_ || itemIndex < 0 || static_cast<size_t>(itemIndex) >= shopInventory_.size()) return;
    Character* hero = findCharacter(playerName);
    if (!hero) return;

    auto item = shopInventory_[static_cast<size_t>(itemIndex)];
    int cost = (item->bonus + 1) * 25;

    if (hero->gold >= cost) {
        hero->gold -= cost;
        if (item->type == ItemType::WEAPON) hero->equippedWeapon = item;
        else if (item->type == ItemType::ARMOR) { hero->equippedArmor = item; hero->calculateAC(); }
        else hero->heal(item->bonus);
        lastEvent_ = hero->name + " purchased " + item->name + "!";
        addJournalEntry(hero->name + " bought " + item->name + " for " + std::to_string(cost) + " gold.");
    } else {
        lastEvent_ = "Not enough gold!";
    }
}

std::string Game::getShopManifest() const {
    if (!isMerchantRoom_) return "";
    std::stringstream ss;
    for (size_t i = 0; i < shopInventory_.size(); ++i) {
        ss << i << ":" << shopInventory_[i]->name << " (" << (shopInventory_[i]->bonus + 1) * 25 << "g);";
    }
    return ss.str();
}

void Game::processTurn() {
    if (gameOver_ || isMerchantRoom_ || turnOrder_.empty() || !isHost_) return;

    Character* current = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (current->isDowned) {
        bool isEnemy = false;
        for (auto& e : enemies_) if (e.get() == current) isEnemy = true;
        if (!isEnemy) {
            int roll = getRandomInt(1, 20);
            if (roll == 20) {
                current->heal(1);
                addChatMessage("System", current->name + " stood back up with a Natural 20!");
            }
            else if (roll >= 10) {
                current->deathSaveSuccesses++;
                if (current->deathSaveSuccesses >= 3) {
                    current->currentHp = 0;
                    addChatMessage("System", current->name + " is now stable.");
                }
            } else {
                current->deathSaveFailures += (roll == 1 ? 2 : 1);
                if (current->deathSaveFailures >= 3) {
                    addChatMessage("System", current->name + " has died.");
                    gameOver_ = true;
                }
            }
        }
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    bool isEnemy = std::find_if(enemies_.begin(), enemies_.end(), [&](auto& e){ return e.get() == current; }) != enemies_.end();
    if (isEnemy) {
        enemyTurn();
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
    } else if (current->name.find("(NPC)") != std::string::npos) {
        playerAttack(0);
    }
}

void Game::playerAttack(int targetEnemyIndex) {
    if (gameOver_ || enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->isDowned) return;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];
    int playerIdx = 0;
    for(size_t i=0; i<players_.size(); ++i) if(players_[i].get() == actor) playerIdx = static_cast<int>(i);
    pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);

    RollResult result = CombatSystem::performAttackRoll(*actor);
    int mod = CombatSystem::getPrimaryModifier(*actor);
    int gearBonus = actor->equippedWeapon ? actor->equippedWeapon->bonus : 0;

    std::stringstream ss;
    ss << actor->name << " attacks! Roll: " << result.dieRoll << " + " << mod << "(stat) + " << gearBonus << "(gear) = " << result.total << ". ";

    if (result.total >= enemy.armorClass) {
        int dmg = CombatSystem::calculateDamage(*actor, result.isCriticalHit);
        enemy.takeDamage(dmg);
        pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
        ss << "HIT! Dealt " << dmg << " damage!";
        if (result.isCriticalHit) addChatMessage("Combat", actor->name + " land a CRITICAL hit!");
    } else {
        ss << "MISS! (Enemy AC is " << enemy.armorClass << ")";
        if (result.isCriticalFail) addChatMessage("Combat", actor->name + " fumbled horribly!");
    }
    lastEvent_ = ss.str();

    if (enemy.currentHp <= 0) {
        addJournalEntry("Defeated " + enemy.name);
        enemies_.erase(enemies_.begin() + targetEnemyIndex);
        if (enemies_.empty()) {
            int xpGained = 40 + (roomCount_ * 10);
            for (auto& p : players_) p->addXp(xpGained);
            roomCount_++;
            auto loot = LootSystem::generateLoot(roomCount_);
            if (loot) {
                if (loot->type == ItemType::WEAPON) actor->equippedWeapon = loot;
                else if (loot->type == ItemType::ARMOR) { actor->equippedArmor = loot; actor->calculateAC(); }
                addJournalEntry(actor->name + " found loot: " + loot->getDescription());
            }
            spawnRoomContent();
            generateRoomDescription();
            rollInitiative();
        } else {
            rebuildTurnOrder();
        }
    } else {
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
    }
}

void Game::playerHeal(int targetPlayerIndex) {
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->resources <= 0 || targetPlayerIndex < 0 || static_cast<size_t>(targetPlayerIndex) >= players_.size()) return;

    actor->resources--;
    int amount = getRandomInt(4, 11);
    players_[static_cast<size_t>(targetPlayerIndex)]->heal(amount);
    lastEvent_ = actor->name + " heals " + players_[static_cast<size_t>(targetPlayerIndex)]->name + " for " + std::to_string(amount) + " HP.";
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
}

void Game::playerSpecialAction(int targetEnemyIndex) {
    if (gameOver_ || turnOrder_.empty()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->resources <= 0 || enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;

    actor->resources--;
    std::stringstream ss;
    ss << actor->name << " uses " << actor->getSpecialAbilityName() << "! ";

    if (actor->characterClass == CharacterClass::WIZARD) {
        for (auto& e : enemies_) { e->takeDamage(getRandomInt(4, 13)); pushVisualEvent(VisualEventType::ENEMY_DAMAGE, 0); }
        ss << "Fire engulfs the room!";
    } else {
        int dmg = CombatSystem::calculateDamage(*actor, true) + 8;
        enemies_[static_cast<size_t>(targetEnemyIndex)]->takeDamage(dmg);
        pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
        ss << "A massive strike deals " << dmg << " damage!";
    }
    lastEvent_ = ss.str();

    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0) it = enemies_.erase(it);
        else ++it;
    }

    if (enemies_.empty()) {
        roomCount_++;
        spawnRoomContent();
        generateRoomDescription();
        rollInitiative();
    } else {
        rebuildTurnOrder();
    }
}

void Game::playerRest() {
    if (isMerchantRoom_) {
        roomCount_++; spawnRoomContent(); generateRoomDescription(); rollInitiative();
        lastEvent_ = "You bid the merchant farewell.";
        return;
    }
    for (auto& p : players_) { p->resources = p->maxResources; p->heal(p->maxHp / 2); p->isDowned = false; }
    lastEvent_ = "Long Rest complete! Health and Power restored.";
    addJournalEntry("The party took a long rest.");
    rollInitiative();
}

void Game::playerInteract(const std::string& playerName) {
    Character* hero = findCharacter(playerName);
    if (!hero) return;

    if (hero->performSavingThrow(hero->attributes.intelligence, 12)) {
        hero->gold += 25;
        lastEvent_ = hero->name + " found 25 gold pieces!";
    } else {
        hero->takeDamage(3);
        int idx = 0;
        for(size_t i=0; i<players_.size(); ++i) if(players_[i].get() == hero) idx = static_cast<int>(i);
        pushVisualEvent(VisualEventType::PLAYER_DAMAGE, idx);
        lastEvent_ = "Fail! A trap hit " + hero->name + " for 3 damage!";
    }
}

void Game::playerIncreaseStat(const std::string& playerName, int statIndex) {
    Character* hero = findCharacter(playerName);
    if (hero) hero->increaseAttribute(statIndex);
}

void Game::enemyTurn() {
    if (enemies_.empty() || players_.empty()) return;
    Character* enemy = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    int enemyIdx = 0;
    for(size_t i=0; i<enemies_.size(); ++i) if(enemies_[i].get() == enemy) enemyIdx = static_cast<int>(i);

    std::vector<int> validTargets;
    for(size_t i=0; i<players_.size(); ++i) if(!players_[i]->isDowned) validTargets.push_back(static_cast<int>(i));

    int targetIdx = validTargets.empty() ? 0 : validTargets[static_cast<size_t>(getRandomInt(0, static_cast<int>(validTargets.size()) - 1))];
    Character& hero = *players_[static_cast<size_t>(targetIdx)];

    pushVisualEvent(VisualEventType::ENEMY_ATTACK, enemyIdx);
    RollResult result = CombatSystem::performAttackRoll(*enemy);
    if (result.total >= hero.armorClass) {
        int dmg = CombatSystem::calculateDamage(*enemy, result.isCriticalHit);
        hero.takeDamage(dmg);
        pushVisualEvent(VisualEventType::PLAYER_DAMAGE, targetIdx);
        lastEvent_ = enemy->name + " attacks " + hero.name + " for " + std::to_string(dmg) + " damage!";
    } else lastEvent_ = enemy->name + " misses " + hero.name + "!";
}

void Game::addChatMessage(const std::string& sender, const std::string& message) {
    if (sender == "System" || sender == "Combat") {
        chatHistory_.push_back("[" + sender + "]: " + message);
    } else {
        chatHistory_.push_back(message);
    }
    if (chatHistory_.size() > 30) chatHistory_.pop_front();
}

std::string Game::getChatHistory() const {
    std::stringstream ss;
    for (const auto& msg : chatHistory_) ss << msg << "\n";
    return ss.str();
}

void Game::addJournalEntry(const std::string& entry) {
    journalEntries_.push_back(entry);
    if (journalEntries_.size() > 30) journalEntries_.pop_front();
}

std::string Game::getJournal() const {
    std::stringstream ss;
    ss << "--- ADVENTURE LOG ---\n";
    for (const auto& entry : journalEntries_) ss << "- " << entry << "\n";
    return ss.str();
}

std::string Game::getPartyStatus() const {
    std::stringstream ss;
    Character* current = (turnOrder_.empty() || currentTurnIndex_ < 0 || static_cast<size_t>(currentTurnIndex_) >= turnOrder_.size()) ? nullptr : turnOrder_[static_cast<size_t>(currentTurnIndex_)];

    ss << "Room " << roomCount_ << " | Turn: " << (current ? current->name : (isMerchantRoom_ ? "Safe" : "None")) << "\n";
    for (const auto& p : players_) {
        ss << p->name << " | HP: " << p->currentHp << "/" << p->maxHp << (p->isDowned ? " [DOWN]" : "") << "\n";
    }
    if (!enemies_.empty()) {
        ss << "--- Foes ---\n";
        for (const auto& e : enemies_) ss << e->name << " | HP: " << e->currentHp << "/" << e->maxHp << "\n";
    }
    return ss.str();
}

std::string Game::getSpecialActionName() const {
    Character* actor = (turnOrder_.empty() || currentTurnIndex_ < 0 || static_cast<size_t>(currentTurnIndex_) >= turnOrder_.size()) ? nullptr : turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (actor) return actor->getSpecialAbilityName();
    return players_.empty() ? "Special" : players_[0]->getSpecialAbilityName();
}

std::string Game::serialize() {
    std::stringstream ss;
    // Section 0: Header
    ss << sessionId_ << "," << roomCount_ << "," << (gameOver_ ? 1 : 0) << "," << currentTurnIndex_ << "," << (isMerchantRoom_ ? 1 : 0) << "|";
    // Section 1: Descriptions
    ss << roomDescription_ << "~" << lastEvent_ << "|";
    // Section 2: Players
    for (const auto& p : players_) {
        ss << p->name << "," << static_cast<int>(p->characterClass) << "," << p->level << "," << p->xp << "," << p->currentHp << "," << p->maxHp << "," << p->resources << "," << p->maxResources << "," << p->pendingStatPoints << "," << p->attributes.strength << "," << p->attributes.dexterity << "," << p->attributes.constitution << "," << p->attributes.intelligence << "," << p->attributes.wisdom << "," << p->attributes.charisma << "," << p->gold << "," << p->initiative << "," << p->uid << ",";
        if (p->equippedWeapon) ss << p->equippedWeapon->name << ":" << p->equippedWeapon->bonus << ":W"; else ss << "None:0:W";
        ss << ",";
        if (p->equippedArmor) ss << p->equippedArmor->name << ":" << p->equippedArmor->bonus << ":A"; else ss << "None:0:A";
        ss << ";";
    }
    ss << "|";
    // Section 3: Enemies
    for (const auto& e : enemies_) {
        ss << e->name << "," << static_cast<int>(e->characterClass) << "," << e->level << "," << e->xp << "," << e->currentHp << "," << e->maxHp << "," << e->resources << "," << e->maxResources << "," << e->pendingStatPoints << "," << e->attributes.strength << "," << e->attributes.dexterity << "," << e->attributes.constitution << "," << e->attributes.intelligence << "," << e->attributes.wisdom << "," << e->attributes.charisma << "," << e->gold << "," << e->initiative << "," << e->uid << ";";
    }
    ss << "|";
    // Section 4: Chat History
    for (const auto& msg : chatHistory_) {
        ss << msg << "~";
    }
    ss << "|";
    // Section 5: Journal
    for (const auto& entry : journalEntries_) {
        ss << entry << "~";
    }
    return ss.str();
}

void Game::deserialize(const std::string& data) {
    if (data.empty()) return;
    std::stringstream ss(data);
    std::string section;

    // Section 0: Header
    if (std::getline(ss, section, '|')) {
        std::stringstream ss_sub(section);
        std::string val;
        if (std::getline(ss_sub, sessionId_, ',')) {
            if (std::getline(ss_sub, val, ',')) roomCount_ = std::stoi(val);
            if (std::getline(ss_sub, val, ',')) gameOver_ = (val == "1");
            if (std::getline(ss_sub, val, ',')) currentTurnIndex_ = std::stoi(val);
            if (std::getline(ss_sub, val, ',')) isMerchantRoom_ = (val == "1");
        }
    }

    // Section 1: Descriptions
    if (std::getline(ss, section, '|')) {
        size_t pos = section.find('~');
        if (pos != std::string::npos) {
            roomDescription_ = section.substr(0, pos);
            lastEvent_ = section.substr(pos + 1);
        }
    }

    // Section 2: Players
    players_.clear();
    if (std::getline(ss, section, '|')) {
        std::stringstream ss_players(section);
        std::string player_str;
        while (std::getline(ss_players, player_str, ';')) {
            if (player_str.empty()) continue;
            std::stringstream ss_p(player_str);
            std::string n, cl_s, lvl_s, xp_s, chp_s, mhp_s, res_s, mres_s, psp_s, str_s, dex_s, con_s, int_s, wis_s, cha_s, gold_s, ini_s, uid;
            std::getline(ss_p, n, ',');
            std::getline(ss_p, cl_s, ',');
            std::getline(ss_p, lvl_s, ',');
            std::getline(ss_p, xp_s, ',');
            std::getline(ss_p, chp_s, ',');
            std::getline(ss_p, mhp_s, ',');
            std::getline(ss_p, res_s, ',');
            std::getline(ss_p, mres_s, ',');
            std::getline(ss_p, psp_s, ',');
            std::getline(ss_p, str_s, ',');
            std::getline(ss_p, dex_s, ',');
            std::getline(ss_p, con_s, ',');
            std::getline(ss_p, int_s, ',');
            std::getline(ss_p, wis_s, ',');
            std::getline(ss_p, cha_s, ',');
            std::getline(ss_p, gold_s, ',');
            std::getline(ss_p, ini_s, ',');
            std::getline(ss_p, uid, ',');

            auto p = std::make_unique<Character>(n, static_cast<CharacterClass>(std::stoi(cl_s)), uid);
            p->level = std::stoi(lvl_s);
            p->xp = std::stoi(xp_s);
            p->currentHp = std::stoi(chp_s);
            p->maxHp = std::stoi(mhp_s);
            p->resources = std::stoi(res_s);
            p->maxResources = std::stoi(mres_s);
            p->pendingStatPoints = std::stoi(psp_s);
            p->attributes = {std::stoi(str_s), std::stoi(dex_s), std::stoi(con_s), std::stoi(int_s), std::stoi(wis_s), std::stoi(cha_s)};
            p->gold = std::stoi(gold_s);
            p->initiative = std::stoi(ini_s);
            p->isDowned = (p->currentHp <= 0);

            std::string item_w, item_a;
            if (std::getline(ss_p, item_w, ',')) {
                size_t p1 = item_w.find(':');
                size_t p2 = item_w.find(':', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    std::string name = item_w.substr(0, p1);
                    int bonus = std::stoi(item_w.substr(p1 + 1, p2 - p1 - 1));
                    if (name != "None") p->equippedWeapon = std::make_shared<Item>(Item{name, ItemType::WEAPON, bonus});
                }
            }
            if (std::getline(ss_p, item_a, ',')) {
                size_t p1 = item_a.find(':');
                size_t p2 = item_a.find(':', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    std::string name = item_a.substr(0, p1);
                    int bonus = std::stoi(item_a.substr(p1 + 1, p2 - p1 - 1));
                    if (name != "None") p->equippedArmor = std::make_shared<Item>(Item{name, ItemType::ARMOR, bonus});
                }
            }
            p->calculateAC();
            players_.push_back(std::move(p));
        }
    }

    // Section 3: Enemies
    enemies_.clear();
    if (std::getline(ss, section, '|')) {
        std::stringstream ss_enemies(section);
        std::string enemy_str;
        while (std::getline(ss_enemies, enemy_str, ';')) {
            if (enemy_str.empty()) continue;
            std::stringstream ss_e(enemy_str);
            std::string n, cl_s, lvl_s, xp_s, chp_s, mhp_s, res_s, mres_s, psp_s, str_s, dex_s, con_s, int_s, wis_s, cha_s, gold_s, ini_s, uid;
            std::getline(ss_e, n, ',');
            std::getline(ss_e, cl_s, ',');
            std::getline(ss_e, lvl_s, ',');
            std::getline(ss_e, xp_s, ',');
            std::getline(ss_e, chp_s, ',');
            std::getline(ss_e, mhp_s, ',');
            std::getline(ss_e, res_s, ',');
            std::getline(ss_e, mres_s, ',');
            std::getline(ss_e, psp_s, ',');
            std::getline(ss_e, str_s, ',');
            std::getline(ss_e, dex_s, ',');
            std::getline(ss_e, con_s, ',');
            std::getline(ss_e, int_s, ',');
            std::getline(ss_e, wis_s, ',');
            std::getline(ss_e, cha_s, ',');
            std::getline(ss_e, gold_s, ',');
            std::getline(ss_e, ini_s, ',');
            std::getline(ss_e, uid, ',');

            auto e = std::make_unique<Character>(n, static_cast<CharacterClass>(std::stoi(cl_s)), uid);
            e->level = std::stoi(lvl_s);
            e->xp = std::stoi(xp_s);
            e->currentHp = std::stoi(chp_s);
            e->maxHp = std::stoi(mhp_s);
            e->resources = std::stoi(res_s);
            e->maxResources = std::stoi(mres_s);
            e->pendingStatPoints = std::stoi(psp_s);
            e->attributes = {std::stoi(str_s), std::stoi(dex_s), std::stoi(con_s), std::stoi(int_s), std::stoi(wis_s), std::stoi(cha_s)};
            e->gold = std::stoi(gold_s);
            e->initiative = std::stoi(ini_s);
            e->calculateAC();
            enemies_.push_back(std::move(e));
        }
    }

    // Section 4: Chat
    chatHistory_.clear();
    if (std::getline(ss, section, '|')) {
        std::stringstream ss_chat(section);
        std::string msg;
        while (std::getline(ss_chat, msg, '~')) {
            if (!msg.empty()) chatHistory_.push_back(msg);
        }
    }

    // Section 5: Journal
    journalEntries_.clear();
    if (std::getline(ss, section, '|')) {
        std::stringstream ss_journal(section);
        std::string entry;
        while (std::getline(ss_journal, entry, '~')) {
            if (!entry.empty()) journalEntries_.push_back(entry);
        }
    }

    rebuildTurnOrder();
}

} // namespace dnd
