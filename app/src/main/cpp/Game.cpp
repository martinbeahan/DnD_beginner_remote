#include "Game.h"
#include "AndroidOut.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>

namespace dnd {

Game::Game() : currentTurnIndex_(0), turnCounter_(0), roomCount_(0), gameOver_(false), isHost_(false), isMerchantRoom_(false), dmOnlyTable_(false) {
    std::random_device rd;
    rng_.seed(rd());

    std::stringstream ss;
    ss << "DND-" << std::hex << std::uppercase << getRandomInt(0, 0xFFFF);
    sessionId_ = ss.str();

    startNewGame();
}

void Game::startNewGame(CharacterClass selectedClass, const std::string& playerName) {
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    while(!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 1;
    dmOnlyTable_ = false;
    dmName_.clear();
    // Local / solo table is always the turn authority (clients opt out via prepareClientJoin).
    isHost_ = true;

    players_.push_back(std::make_unique<Character>(playerName, selectedClass, "local-player"));
    addAlly("Melf (NPC)", CharacterClass::WIZARD);

    generateRoomDescription();
    spawnRoomContent();
    rollInitiative();

    lastEvent_ = "Adventure Started! Room 1. Roll for Initiative!";
    addChatMessage("System", "A new party has entered the dungeon.");
    addJournalEntry("The party entered the dungeon.");
    dmSay("Welcome, adventurers. I am your Dungeon Master. Steel yourselves — danger waits in the dark.");
    dmSay("Use Attack for a weapon strike, your class Special for a signature move, Potion for healing, and Short Rest to recover.");
}

void Game::startDmSession(const std::string& dmName) {
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    while(!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 0;
    isMerchantRoom_ = false;
    dmOnlyTable_ = true;
    dmName_ = dmName.empty() ? "Dungeon Master" : dmName;
    isHost_ = true;

    roomDescription_ = "A candlelit tavern table waits for heroes. The DM (" + dmName_ + ") prepares the adventure. Share the session ID so friends can Join.";
    lastEvent_ = "DM table open — waiting for players to join.";
    addChatMessage("System", "DM " + dmName_ + " opened an online table.");
    addJournalEntry("The DM lit the candles and opened the table.");
    dmSay("I am " + dmName_ + ", your Dungeon Master. When heroes Join, I will begin the dungeon.");
}

void Game::dmBeginDungeon() {
    if (players_.empty()) {
        lastEvent_ = "No heroes have joined yet.";
        dmSay("Patience — wait for at least one adventurer to Join the session.");
        return;
    }
    roomCount_ = 1;
    isMerchantRoom_ = false;
    gameOver_ = false;
    generateRoomDescription();
    spawnRoomContent();
    rollInitiative();
    lastEvent_ = "The dungeon begins! Room 1 — roll for initiative!";
    addChatMessage("System", "The DM began the dungeon.");
    dmSay("The tavern doors close behind you. Steel yourselves.");
}

void Game::dmAdvanceRoom() {
    if (players_.empty()) {
        lastEvent_ = "No party to advance.";
        return;
    }
    if (gameOver_) {
        lastEvent_ = "Game Over — start a new table.";
        return;
    }
    roomCount_++;
    spawnRoomContent();
    if (!isMerchantRoom_) {
        generateRoomDescription();
        rollInitiative();
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
    lastEvent_ = "DM advanced the party to room " + std::to_string(roomCount_) + ".";
    addChatMessage("System", lastEvent_);
    dmSay("Onward — a new chamber opens before you.");
}

void Game::dmNarrate(const std::string& line) {
    if (line.empty()) return;
    roomDescription_ = line;
    dmSay(line);
    lastEvent_ = "DM narrates…";
}


void Game::prepareClientJoin() {
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    while (!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 0;
    isMerchantRoom_ = false;
    dmOnlyTable_ = false;
    dmName_.clear();
    isHost_ = false;
    currentTurnIndex_ = 0;

    roomDescription_ = "Connecting to the DM table…";
    lastEvent_ = "Waiting for the Dungeon Master to sync the party…";
    addChatMessage("System", "Joining online session…");
}

void Game::dmGrantShortRest() {
    if (gameOver_ || players_.empty()) return;
    // Reuse short-rest recovery without merchant leave logic; force past combat lock.
    bool wasMerchant = isMerchantRoom_;
    isMerchantRoom_ = false;
    playerRest(/*force=*/true);
    isMerchantRoom_ = wasMerchant;
    dmSay("The DM grants a short rest.");
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
    dmSay("Room " + std::to_string(roomCount_) + ": " + roomDescription_ + "What do you do?");
}

void Game::spawnRoomContent() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;

    if (roomCount_ > 1 && roomCount_ % 4 == 0) {
        isMerchantRoom_ = true;
        roomDescription_ = "You find a rare pocket of safety. A weary Merchant awaits.";
        dmSay("A lantern glows ahead. A traveling merchant offers goods — and a chance for a longer rest from the grind.");
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
        if (p->isDead) continue; // fallen heroes stay out of initiative
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
    // Capture identity before we clear; never touch turnOrder_ after players_/enemies_ were freed.
    const int savedIndex = currentTurnIndex_;
    std::string currentUid;
    if (!turnOrder_.empty()
        && currentTurnIndex_ >= 0
        && static_cast<size_t>(currentTurnIndex_) < turnOrder_.size()
        && turnOrder_[static_cast<size_t>(currentTurnIndex_)] != nullptr) {
        currentUid = turnOrder_[static_cast<size_t>(currentTurnIndex_)]->uid;
    }

    turnOrder_.clear();
    for (auto& p : players_) {
        if (!p->isDead) turnOrder_.push_back(p.get());
    }
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
    // After deserialize turnOrder_ was cleared first, so fall back to the saved index.
    if (savedIndex >= 0 && static_cast<size_t>(savedIndex) < turnOrder_.size()) {
        currentTurnIndex_ = savedIndex;
    } else {
        currentTurnIndex_ = 0;
    }
}

Character* Game::findCharacter(const std::string& name) {
    for (auto& p : players_) {
        if (p->name == name) return p.get();
    }
    return nullptr;
}

bool Game::buyItem(const std::string& playerName, int itemIndex) {
    if (gameOver_) return false;
    if (!isMerchantRoom_ || itemIndex < 0 || static_cast<size_t>(itemIndex) >= shopInventory_.size()) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;

    auto item = shopInventory_[static_cast<size_t>(itemIndex)];
    int cost = (item->bonus + 1) * 25;

    if (hero->gold >= cost) {
        hero->gold -= cost;
        if (item->type == ItemType::WEAPON) hero->equippedWeapon = item;
        else if (item->type == ItemType::ARMOR) { hero->equippedArmor = item; hero->calculateAC(); }
        else hero->heal(item->bonus);
        lastEvent_ = hero->name + " purchased " + item->name + "!";
        addJournalEntry(hero->name + " bought " + item->name + " for " + std::to_string(cost) + " gold.");
        return true;
    }
    lastEvent_ = "Not enough gold!";
    return false;
}

std::string Game::getShopManifest() const {
    if (!isMerchantRoom_) return "";
    std::stringstream ss;
    for (size_t i = 0; i < shopInventory_.size(); ++i) {
        ss << i << ":" << shopInventory_[i]->name << " (" << (shopInventory_[i]->bonus + 1) * 25 << "g);";
    }
    return ss.str();
}

void Game::advanceTurn() {
    if (turnOrder_.empty()) { currentTurnIndex_ = 0; return; }
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
}

void Game::removeFromTurnOrder(Character* c) {
    if (!c || turnOrder_.empty()) return;
    Character* cur = nullptr;
    if (currentTurnIndex_ >= 0 && static_cast<size_t>(currentTurnIndex_) < turnOrder_.size()) {
        cur = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    }
    turnOrder_.erase(std::remove(turnOrder_.begin(), turnOrder_.end(), c), turnOrder_.end());
    if (turnOrder_.empty()) {
        currentTurnIndex_ = 0;
        return;
    }
    // Keep the same current actor when possible; otherwise clamp.
    if (cur && cur != c) {
        for (size_t i = 0; i < turnOrder_.size(); ++i) {
            if (turnOrder_[i] == cur) {
                currentTurnIndex_ = static_cast<int>(i);
                return;
            }
        }
    }
    if (currentTurnIndex_ >= static_cast<int>(turnOrder_.size())) {
        currentTurnIndex_ = 0;
    }
}

void Game::checkPartyDefeat() {
    if (players_.empty()) return;
    bool anyLiving = false;
    for (const auto& p : players_) {
        if (!p->isDead) { anyLiving = true; break; }
    }
    if (!anyLiving) {
        gameOver_ = true;
        lastEvent_ = "Game Over — the entire party has fallen.";
        dmSay("Silence falls. No hero remains standing.");
        addChatMessage("System", "Game Over");
    }
}

bool Game::isAllyAi(const Character* c) const {
    if (!c) return false;
    if (c->name.find("(NPC)") != std::string::npos) return true;
    // Solo starter ally uses uid "ally-N"; never treat the local hero as AI.
    if (c->uid.rfind("ally-", 0) == 0) return true;
    return false;
}

void Game::purgeDownedEnemies() {
    // Drop 0-HP foes out of the encounter without touching a dangling turnOrder_ pointer.
    bool removed = false;
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0 || (*it)->isDowned) {
            addJournalEntry("Defeated " + (*it)->name);
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (!removed) return;
    if (enemies_.empty()) {
        int xpGained = 50 + (roomCount_ * 15);
        for (auto& p : players_) {
            if (p->addXp(xpGained)) {
                dmSay(p->name + " levels up! Spend your ability points from the character sheet.");
            }
        }
        dmSay("The party gains " + std::to_string(xpGained) + " XP.");
        roomCount_++;
        spawnRoomContent();
        generateRoomDescription();
        rollInitiative();
    } else if (!turnOrder_.empty()) {
        if (currentTurnIndex_ >= static_cast<int>(turnOrder_.size())) {
            currentTurnIndex_ = 0;
        }
    }
}

void Game::allyTurn() {
    if (turnOrder_.empty()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->isDowned || actor->isDead) {
        advanceTurn();
        return;
    }
    if (enemies_.empty()) {
        advanceTurn();
        return;
    }

    const std::string actorUid = actor->uid;
    const int roomBefore = roomCount_;

    // 1) Heal a downed / critically hurt ally when we can (potion or Cleric Healing Word).
    const int healIdx = pickAllyAiHealTarget();
    if (healIdx >= 0 && actor->resources > 0) {
        if (actor->characterClass == CharacterClass::CLERIC) {
            dmSay(actor->name + " calls a Healing Word for a wounded ally.");
            playerSpecialAction(0); // Cleric special ignores enemy index; heals lowest HP
        } else {
            dmSay(actor->name + " spends a potion on a wounded ally.");
            playerHeal(healIdx);
        }
        if (roomCount_ != roomBefore) return;
        Character* now = getCurrentActor();
        if (now && now->uid == actorUid) advanceTurn();
        return;
    }

    // 2) Pick a living foe — prefer finishing blows / focus fire.
    const int target = pickAllyAiEnemyTarget();
    if (target < 0) {
        purgeDownedEnemies();
        return;
    }

    Character& enemy = *enemies_[static_cast<size_t>(target)];

    // 3) Class special when it clearly beats a basic swing and we have a use left.
    if (actor->resources > 0 && actor->characterClass != CharacterClass::CLERIC
        && allySpecialBeatsBasic(*actor, enemy)) {
        dmSay(actor->name + " uses " + actor->getSpecialAbilityName() + "!");
        playerSpecialAction(target);
        if (roomCount_ != roomBefore) return;
        Character* now = getCurrentActor();
        if (now && now->uid == actorUid) advanceTurn();
        return;
    }

    // 4) Basic weapon attack — only with a validated living target (no playerAttack no-ops).
    playerAttack(target);
    if (roomCount_ != roomBefore) return;
    Character* now = getCurrentActor();
    if (now && now->uid == actorUid) {
        advanceTurn();
    }
}

void Game::processTurn() {
    if (gameOver_ || isMerchantRoom_ || turnOrder_.empty() || !isHost_) return;

    Character* current = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!current) { advanceTurn(); return; }

    // Dead combatants should not remain in the order.
    if (current->isDead) {
        removeFromTurnOrder(current);
        checkPartyDefeat();
        return;
    }

    bool isEnemy = std::find_if(enemies_.begin(), enemies_.end(),
        [&](auto& e){ return e.get() == current; }) != enemies_.end();

    // 0-HP foes still sitting in initiative look like "NPC turns that do nothing".
    if (isEnemy && (current->isDowned || current->currentHp <= 0)) {
        purgeDownedEnemies();
        return;
    }

    if (current->isDowned) {
        if (!isEnemy) {
            // Stable PCs skip death saves until healed or damaged again.
            if (current->isStable) {
                addChatMessage("System", current->name + " remains stable (unconscious).");
                advanceTurn();
                return;
            }
            int roll = getRandomInt(1, 20);
            if (roll == 20) {
                current->heal(1);
                addChatMessage("System", current->name + " stood back up with a Natural 20!");
            } else if (roll >= 10) {
                current->deathSaveSuccesses++;
                if (current->deathSaveSuccesses >= 3) {
                    current->currentHp = 0;
                    current->isStable = true;
                    current->deathSaveSuccesses = 3;
                    addChatMessage("System", current->name + " is now stable.");
                    dmSay(current->name + " stabilizes — still down, but no longer making death saves.");
                } else {
                    addChatMessage("System", current->name + " death save success (" +
                                  std::to_string(current->deathSaveSuccesses) + "/3).");
                }
            } else {
                current->deathSaveFailures += (roll == 1 ? 2 : 1);
                if (current->deathSaveFailures >= 3) {
                    current->isDead = true;
                    current->isDowned = true;
                    current->isStable = false;
                    current->deathSaveFailures = 3;
                    addChatMessage("System", current->name + " has died.");
                    lastEvent_ = current->name + " has fallen permanently.";
                    dmSay(current->name + " breathes their last. The rest of the party fights on.");
                    removeFromTurnOrder(current);
                    checkPartyDefeat();
                    return;
                } else {
                    addChatMessage("System", current->name + " death save failure (" +
                                  std::to_string(current->deathSaveFailures) + "/3).");
                }
            }
        }
        advanceTurn();
        return;
    }

    if (isEnemy) {
        enemyTurn();
        if (!gameOver_ && getCurrentActor() == current) {
            advanceTurn();
        }
        return;
    }

    if (isAllyAi(current)) {
        allyTurn();
        return;
    }
    // Otherwise it's a human-controlled hero — wait for UI input.
}


void Game::dmSay(const std::string& line) {
    addChatMessage("DM", line);
}

int Game::proficiencyBonus() const {
    int level = players_.empty() ? 1 : players_[0]->level;
    return dnd::CombatSystem::proficiencyBonusForLevel(level);
}

bool Game::performWeaponAttack(Character* actor, Character& target, int atkVisIndex, bool targetIsEnemy, int targetIndex, bool sneakAttack) {
    if (!actor) return false;
    pushVisualEvent(targetIsEnemy ? VisualEventType::PLAYER_ATTACK : VisualEventType::ENEMY_ATTACK, atkVisIndex);

    RollResult result = CombatSystem::performAttackRoll(*actor);
    int mod = CombatSystem::getPrimaryModifier(*actor);
    int pb = CombatSystem::proficiencyBonusForLevel(actor->level);
    int gearBonus = actor->equippedWeapon ? actor->equippedWeapon->bonus : 0;

    std::stringstream ss;
    ss << actor->name << " attacks " << target.name << "! d20=" << result.dieRoll
       << " + " << mod << " (ability) + " << pb << " (prof) + " << gearBonus
       << " (magic) = " << result.total << " vs AC " << target.armorClass << ". ";

    bool hit = result.total >= target.armorClass || result.isCriticalHit;
    if (result.isCriticalFail) hit = false;

    if (hit) {
        int extraDice = sneakAttack ? actor->sneakAttackDice() : 0;
        int dmg = CombatSystem::calculateDamage(*actor, result.isCriticalHit, extraDice, 6);
        target.takeDamage(dmg);
        pushVisualEvent(targetIsEnemy ? VisualEventType::ENEMY_DAMAGE : VisualEventType::PLAYER_DAMAGE, targetIndex);
        ss << (result.isCriticalHit ? "CRITICAL HIT! " : "Hit! ") << "Damage " << dmg;
        if (sneakAttack) ss << " (includes Sneak Attack)";
        ss << ".";
        if (result.isCriticalHit) dmSay("The strike lands true — a critical hit!");
    } else {
        ss << "Miss!";
        if (result.isCriticalFail) {
            ss << " (natural 1)";
            dmSay(actor->name + " swings wildly and nearly drops their weapon.");
        }
    }
    lastEvent_ = ss.str();
    addChatMessage("Combat", lastEvent_);
    return hit && target.currentHp <= 0;
}

void Game::resolveEnemyDefeated(Character* actor, int /*targetEnemyIndex*/) {
    // Remove dead enemies and advance dungeon.
    // Strip turnOrder_ entries BEFORE destroying the Character (no dangling pointers).
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0) {
            addJournalEntry("Defeated " + (*it)->name);
            dmSay("The " + (*it)->name + " falls. The dungeon grows quieter… for now.");
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
        } else ++it;
    }

    if (enemies_.empty()) {
        int xpGained = 50 + (roomCount_ * 15);
        for (auto& p : players_) {
            if (p->addXp(xpGained)) {
                dmSay(p->name + " levels up! Spend your ability points from the character sheet.");
            }
        }
        dmSay("The party gains " + std::to_string(xpGained) + " XP.");
        roomCount_++;
        if (actor) {
            auto loot = LootSystem::generateLoot(roomCount_);
            if (loot) {
                if (loot->type == ItemType::WEAPON) actor->equippedWeapon = loot;
                else if (loot->type == ItemType::ARMOR) { actor->equippedArmor = loot; actor->calculateAC(); }
                dmSay(actor->name + " finds " + loot->getDescription() + " among the spoils.");
                addJournalEntry(actor->name + " found loot: " + loot->getDescription());
            }
        }
        spawnRoomContent();
        generateRoomDescription();
        rollInitiative();
    } else {
        rebuildTurnOrder();
        // The attacker already spent their action on the killing blow — move on.
        advanceTurn();
    }
}

void Game::playerAttack(int targetEnemyIndex) {
    if (gameOver_ || enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    if (turnOrder_.empty()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->isDowned || actor->isDead) return;

    // Only party members (hero + NPC allies) use this attack path
    bool isPlayer = false;
    int playerIdx = 0;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (players_[i].get() == actor) { isPlayer = true; playerIdx = static_cast<int>(i); break; }
    }
    if (!isPlayer) return;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];
    bool killed = performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
    if (killed || enemy.currentHp <= 0) {
        resolveEnemyDefeated(actor, targetEnemyIndex);
    } else {
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
    }
}

void Game::playerHeal(int targetPlayerIndex) {
    if (gameOver_) return;
    // Potion of Healing (SRD): 2d4+2. Anyone can drink one by spending a resource (supply).
    if (turnOrder_.empty()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->resources <= 0 || targetPlayerIndex < 0 || static_cast<size_t>(targetPlayerIndex) >= players_.size()) {
        lastEvent_ = "No potions left (need a resource/supply).";
        return;
    }

    actor->resources--;
    Character& target = *players_[static_cast<size_t>(targetPlayerIndex)];
    int amount = getRandomInt(1, 4) + getRandomInt(1, 4) + 2;
    target.heal(amount);
    lastEvent_ = actor->name + " drinks a Potion of Healing on " + target.name + " for " + std::to_string(amount) + " HP.";
    dmSay(target.name + " feels vitality return as the potion takes hold.");
    addChatMessage("Combat", lastEvent_);
    currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
}

void Game::playerSpecialAction(int targetEnemyIndex) {
    if (gameOver_ || turnOrder_.empty()) return;
    Character* actor = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!actor || actor->isDowned) return;

    int playerIdx = 0;
    for (size_t i = 0; i < players_.size(); ++i) if (players_[i].get() == actor) playerIdx = static_cast<int>(i);

    // Cleric Healing Word can target allies without an enemy
    if (actor->characterClass == CharacterClass::CLERIC) {
        if (actor->resources <= 0) { lastEvent_ = "No spell slots left."; return; }
        actor->resources--;
        int targetIdx = targetEnemyIndex; // UI reuses index picker for allies on heal; for special we heal lowest HP ally
        int best = 0;
        for (size_t i = 0; i < players_.size(); ++i) {
            if (players_[i]->currentHp < players_[best]->currentHp) best = static_cast<int>(i);
        }
        Character& ally = *players_[static_cast<size_t>(best)];
        int wis = Attributes::getModifier(actor->attributes.wisdom);
        int amount = getRandomInt(1, 4) + wis;
        if (amount < 1) amount = 1;
        ally.heal(amount);
        lastEvent_ = actor->name + " casts Healing Word on " + ally.name + " for " + std::to_string(amount) + " HP.";
        dmSay("A soft glow knits " + ally.name + "'s wounds — Healing Word.");
        addChatMessage("Combat", lastEvent_);
        currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (enemies_.empty() || targetEnemyIndex < 0 || static_cast<size_t>(targetEnemyIndex) >= enemies_.size()) return;
    if (actor->resources <= 0) { lastEvent_ = "No uses remaining for that feature."; return; }
    actor->resources--;

    Character& enemy = *enemies_[static_cast<size_t>(targetEnemyIndex)];

    if (actor->characterClass == CharacterClass::FIGHTER) {
        dmSay(actor->name + " shouts and surges forward — Action Surge!");
        // Extra attack action: two weapon attacks
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
        if (enemy.currentHp > 0) {
            dmSay("Still standing! " + actor->name + " swings again.");
            performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, false);
        }
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (actor->characterClass == CharacterClass::ROGUE) {
        dmSay(actor->name + " slips into a flank and strikes — Sneak Attack!");
        performWeaponAttack(actor, enemy, playerIdx, true, targetEnemyIndex, true);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }

    if (actor->characterClass == CharacterClass::WIZARD) {
        // Magic Missile: 3 darts of 1d4+1, auto-hit (SRD)
        int total = 0;
        for (int i = 0; i < 3; ++i) total += getRandomInt(1, 4) + 1;
        enemy.takeDamage(total);
        pushVisualEvent(VisualEventType::PLAYER_ATTACK, playerIdx);
        pushVisualEvent(VisualEventType::ENEMY_DAMAGE, targetEnemyIndex);
        lastEvent_ = actor->name + " casts Magic Missile! Three darts deal " + std::to_string(total) + " force damage to " + enemy.name + ".";
        dmSay("Glowing darts streak unerringly to their mark.");
        addChatMessage("Combat", lastEvent_);
        if (enemy.currentHp <= 0) resolveEnemyDefeated(actor, targetEnemyIndex);
        else currentTurnIndex_ = (currentTurnIndex_ + 1) % static_cast<int>(turnOrder_.size());
        return;
    }
}

void Game::playerRest(bool force) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    if (!turnOrder_.empty() && currentTurnIndex_ >= 0
        && static_cast<size_t>(currentTurnIndex_) < turnOrder_.size()
        && turnOrder_[static_cast<size_t>(currentTurnIndex_)]
        && turnOrder_[static_cast<size_t>(currentTurnIndex_)]->isDowned) {
        lastEvent_ = "You're dying — make death saves, you can't rest now.";
        return;
    }
    // Block Short Rest while foes are still in the room (mid-combat).
    // DM grant can pass force=true to allow a story beat.
    if (!force && !isMerchantRoom_ && !enemies_.empty()) {
        lastEvent_ = "Can't Short Rest in the middle of a fight!";
        dmSay("Steel yourselves — rest when the chamber is clear.");
        return;
    }
    if (isMerchantRoom_) {
        roomCount_++;
        spawnRoomContent();
        generateRoomDescription();
        rollInitiative();
        lastEvent_ = "You bid the merchant farewell and press deeper.";
        dmSay("The merchant nods. \"Luck in the dark, friends.\"");
        return;
    }
    // Short Rest (5e-inspired): spend hit dice vibe — recover half missing HP + some features
    for (auto& p : players_) {
        if (p->isDead) continue; // dead heroes stay down
        int missing = p->maxHp - p->currentHp;
        int recover = std::max(p->hitDie() + Attributes::getModifier(p->attributes.constitution), missing / 2);
        if (recover < 1) recover = 1;
        if (recover > missing && missing > 0) recover = missing;
        if (missing == 0) recover = 0;
        p->heal(recover);
        p->resources = std::min(p->maxResources, p->resources + std::max(1, p->maxResources / 2));
        p->isDowned = false;
        p->isStable = false;
        p->deathSaveSuccesses = 0;
        p->deathSaveFailures = 0;
    }
    lastEvent_ = "Short Rest complete. Wounds bind; some power returns.";
    dmSay("You catch your breath in a quiet alcove. This is a Short Rest — a Long Rest will have to wait for safer ground.");
    addJournalEntry("The party took a short rest.");
    addChatMessage("Combat", lastEvent_);
    rollInitiative();
}

void Game::playerInteract(const std::string& playerName) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead || hero->isDowned) return;

    if (hero->performSavingThrow(hero->attributes.intelligence, 12)) {
        hero->gold += 25;
        lastEvent_ = hero->name + " found 25 gold pieces!";
        dmSay("A successful Investigation check reveals a hidden pouch.");
    } else {
        hero->takeDamage(3);
        int idx = 0;
        for(size_t i=0; i<players_.size(); ++i) if(players_[i].get() == hero) idx = static_cast<int>(i);
        pushVisualEvent(VisualEventType::PLAYER_DAMAGE, idx);
        lastEvent_ = "Fail! A trap hit " + hero->name + " for 3 damage!";
        dmSay("A pressure plate clicks — poison darts!");
        if (hero->isDead) {
            removeFromTurnOrder(hero);
            checkPartyDefeat();
            return;
        }
    }
    // Interact spends your turn like Attack/Heal.
    if (!turnOrder_.empty()) {
        advanceTurn();
    }
}

void Game::playerIncreaseStat(const std::string& playerName, int statIndex) {
    if (gameOver_) return;
    Character* hero = findCharacter(playerName);
    if (hero) hero->increaseAttribute(statIndex);
}

int Game::scoreHeroThreat(const Character& hero, bool partyHasDowned) const {
    if (hero.isDead) return -100000;
    int score = 0;
    if (hero.isDowned) {
        // Only chosen when no upright heroes remain — finish unstable first.
        score = 20 + hero.deathSaveFailures * 25;
        if (!hero.isStable) score += 15;
        return score;
    }
    // Living threats: prefer low HP (focus fire / finish) and pressure when someone is already down.
    score = 100;
    score += (hero.maxHp - hero.currentHp) * 4;
    if (hero.maxHp > 0 && hero.currentHp * 3 <= hero.maxHp) score += 35; // critically low
    if (hero.currentHp <= 4) score += 25; // easy finish
    if (partyHasDowned) score += 20; // "downed-adjacent" pressure on remaining fighters
    return score;
}

int Game::pickEnemyAiTarget() const {
    bool partyHasDowned = false;
    for (const auto& p : players_) {
        if (p && !p->isDead && p->isDowned) { partyHasDowned = true; break; }
    }

    int bestUp = -1, bestUpScore = -100000;
    int bestDown = -1, bestDownScore = -100000;
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p || p->isDead) continue;
        int s = scoreHeroThreat(*p, partyHasDowned);
        if (p->isDowned) {
            if (s > bestDownScore) { bestDownScore = s; bestDown = static_cast<int>(i); }
        } else {
            if (s > bestUpScore) { bestUpScore = s; bestUp = static_cast<int>(i); }
        }
    }
    if (bestUp >= 0) return bestUp;
    return bestDown;
}

int Game::pickAllyAiEnemyTarget() const {
    int best = -1;
    int bestScore = -100000;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        const Character* e = enemies_[i].get();
        if (!e || e->currentHp <= 0 || e->isDowned || e->isDead) continue;
        // Prefer finishing weak foes (focus fire).
        int score = 50 + (e->maxHp - e->currentHp) * 5;
        if (e->currentHp <= 6) score += 40;
        if (e->maxHp > 0 && e->currentHp * 3 <= e->maxHp) score += 20;
        // Slight bias toward lower AC (easier to hit with a basic attack).
        score += std::max(0, 16 - e->armorClass);
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

int Game::pickAllyAiHealTarget() const {
    int best = -1;
    int bestScore = -100000;
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p || p->isDead) continue;
        int score = 0;
        if (p->isDowned) {
            score = 200 + p->deathSaveFailures * 30; // revive / stabilize urgency
        } else if (p->maxHp > 0 && p->currentHp * 3 <= p->maxHp) {
            score = 80 + (p->maxHp - p->currentHp); // critically low
        } else if (p->currentHp <= 4 && p->currentHp < p->maxHp) {
            score = 60;
        } else {
            continue; // healthy enough — do not burn a heal
        }
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    return best;
}

bool Game::allySpecialBeatsBasic(const Character& actor, const Character& enemy) const {
    if (actor.resources <= 0) return false;
    // Expected basic hit damage is modest and can miss; specials are stronger or auto-hit.
    const int hp = enemy.currentHp;
    switch (actor.characterClass) {
        case CharacterClass::WIZARD:
            // Magic Missile auto-hits (~10.5 avg) — clearly better than a staff swing, especially to finish or vs high AC.
            return hp <= 12 || enemy.armorClass >= 13 || actor.resources >= 2;
        case CharacterClass::FIGHTER:
            // Action Surge = two attacks; worth it vs chunky foes or to secure a finish.
            return hp > 8 || (hp > 4 && actor.resources >= 2);
        case CharacterClass::ROGUE:
            // Sneak Attack adds dice — almost always better than a plain swing when a use remains.
            return true;
        case CharacterClass::CLERIC:
            // Healing Word is a heal, not an attack special.
            return false;
    }
    return false;
}

void Game::enemyTurn() {
    if (enemies_.empty() || players_.empty()) return;
    Character* enemy = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!enemy || enemy->isDowned || enemy->isDead || enemy->currentHp <= 0) return;

    int enemyIdx = 0;
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (enemies_[i].get() == enemy) enemyIdx = static_cast<int>(i);
    }

    const int targetIdx = pickEnemyAiTarget();
    if (targetIdx < 0) {
        checkPartyDefeat();
        return;
    }

    Character& hero = *players_[static_cast<size_t>(targetIdx)];
    // Light DM color when the AI focuses a wounded or dying target.
    if (!hero.isDead && (hero.isDowned || hero.currentHp * 3 <= hero.maxHp)) {
        dmSay(enemy->name + " smells blood and focuses on " + hero.name + "!");
    }

    // Simple specials: spend a resource when it clearly beats a basic swing.
    const bool canSpecial = enemy->resources > 0;
    const bool finishThreat = hero.isDowned || hero.currentHp * 2 <= hero.maxHp;
    const bool fighterBurst = enemy->characterClass == CharacterClass::FIGHTER
        && hero.currentHp > 6 && !hero.isDowned;
    const bool rogueSneak = enemy->characterClass == CharacterClass::ROGUE
        && (finishThreat || hero.armorClass >= 14);
    const bool useSpecial = canSpecial && (finishThreat || fighterBurst || rogueSneak);

    auto doOneAttack = [&](bool sneak) {
        pushVisualEvent(VisualEventType::ENEMY_ATTACK, enemyIdx);
        RollResult result = CombatSystem::performAttackRoll(*enemy);
        bool hit = (result.total >= hero.armorClass || result.isCriticalHit) && !result.isCriticalFail;
        if (hit) {
            int extra = sneak ? enemy->sneakAttackDice() : 0;
            int dmg = CombatSystem::calculateDamage(*enemy, result.isCriticalHit, extra, 6);
            bool wasDowned = hero.isDowned || hero.isStable;
            hero.takeDamage(dmg, wasDowned && result.isCriticalHit);
            pushVisualEvent(VisualEventType::PLAYER_DAMAGE, targetIdx);
            lastEvent_ = enemy->name + " attacks " + hero.name + "! d20=" + std::to_string(result.dieRoll)
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass)
                + ". " + (result.isCriticalHit ? "CRITICAL HIT! " : "Hit! ")
                + "Damage " + std::to_string(dmg) + ".";
            if (sneak) lastEvent_ += " (Sneak Attack)";
            addChatMessage("Combat", lastEvent_);
            if (hero.isDead) {
                dmSay(hero.name + " is slain!");
                removeFromTurnOrder(&hero);
                checkPartyDefeat();
            } else if (!wasDowned && hero.isDowned) {
                dmSay(hero.name + " drops! Death saving throws will follow on their turns.");
            } else if (wasDowned && hero.isDowned) {
                dmSay(hero.name + " takes a death-save failure from the blow" +
                     (result.isCriticalHit ? " (critical — two failures)!" : "."));
            }
        } else {
            lastEvent_ = enemy->name + " attacks " + hero.name + "! d20=" + std::to_string(result.dieRoll)
                + " = " + std::to_string(result.total) + " vs AC " + std::to_string(hero.armorClass) + ". Miss!";
            addChatMessage("Combat", lastEvent_);
        }
        return hero.isDead;
    };

    if (useSpecial && enemy->characterClass == CharacterClass::FIGHTER) {
        enemy->resources--;
        dmSay(enemy->name + " surges with a brutal flurry!");
        if (!doOneAttack(false) && hero.currentHp > 0 && !hero.isDead) {
            doOneAttack(false);
        }
        return;
    }
    if (useSpecial && enemy->characterClass == CharacterClass::ROGUE) {
        enemy->resources--;
        dmSay(enemy->name + " strikes from an angle — Sneak Attack!");
        doOneAttack(true);
        return;
    }

    doOneAttack(false);
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
    if (gameOver_) ss << "Game Over\n";
    if (dmOnlyTable_ && !dmName_.empty()) ss << "DM: " << dmName_ << "\n";
    for (const auto& p : players_) {
        ss << p->name << " | HP: " << p->currentHp << "/" << p->maxHp;
        if (p->isDead) ss << " [DEAD]";
        else if (p->isStable) ss << " [STABLE]";
        else if (p->isDowned) ss << " [DOWN]";
        ss << "\n";
    }
    if (!enemies_.empty()) {
        ss << "--- Foes ---\n";
        for (const auto& e : enemies_) ss << e->name << " | HP: " << e->currentHp << "/" << e->maxHp << "\n";
    }
    return ss.str();
}


std::string Game::getBattleRoster() const {
    std::stringstream ss;
    for (size_t i = 0; i < players_.size(); ++i) {
        if (i) ss << ";";
        ss << "P," << players_[i]->name << ","
           << static_cast<int>(players_[i]->characterClass) << ","
           << players_[i]->currentHp << "," << players_[i]->maxHp;
    }
    ss << "|";
    for (size_t i = 0; i < enemies_.size(); ++i) {
        if (i) ss << ";";
        ss << "E," << enemies_[i]->name << ","
           << static_cast<int>(enemies_[i]->characterClass) << ","
           << enemies_[i]->currentHp << "," << enemies_[i]->maxHp;
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
    std::string dmSafe = dmName_;
    for (char& c : dmSafe) { if (c == ',' || c == '|' || c == '~') c = '_'; }
    ss << sessionId_ << "," << roomCount_ << "," << (gameOver_ ? 1 : 0) << "," << currentTurnIndex_ << "," << (isMerchantRoom_ ? 1 : 0)
       << "," << (dmOnlyTable_ ? 1 : 0) << "," << dmSafe << "|";
    // Section 1: Descriptions
    ss << roomDescription_ << "~" << lastEvent_ << "|";
    // Section 2: Players
    for (const auto& p : players_) {
        ss << p->name << "," << static_cast<int>(p->characterClass) << "," << p->level << "," << p->xp << "," << p->currentHp << "," << p->maxHp << "," << p->resources << "," << p->maxResources << "," << p->pendingStatPoints << "," << p->attributes.strength << "," << p->attributes.dexterity << "," << p->attributes.constitution << "," << p->attributes.intelligence << "," << p->attributes.wisdom << "," << p->attributes.charisma << "," << p->gold << "," << p->initiative << "," << p->uid << ",";
        if (p->equippedWeapon) ss << p->equippedWeapon->name << ":" << p->equippedWeapon->bonus << ":W"; else ss << "None:0:W";
        ss << ",";
        if (p->equippedArmor) ss << p->equippedArmor->name << ":" << p->equippedArmor->bonus << ":A"; else ss << "None:0:A";
        ss << "," << (p->isDowned ? 1 : 0) << "," << p->deathSaveSuccesses << "," << p->deathSaveFailures
           << "," << (p->isStable ? 1 : 0) << "," << (p->isDead ? 1 : 0);
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
    // Drop turn pointers BEFORE destroying Character unique_ptrs (avoids use-after-free hangs).
    turnOrder_.clear();
    currentTurnIndex_ = 0;

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
            if (std::getline(ss_sub, val, ',')) dmOnlyTable_ = (val == "1");
            else dmOnlyTable_ = false;
            if (std::getline(ss_sub, val, ',')) dmName_ = val;
            else if (!dmOnlyTable_) dmName_.clear();
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
            // Optional trailing death-save fields (backward compatible with older saves).
            std::string down_s, dss_s, dsf_s, stab_s, dead_s;
            if (std::getline(ss_p, down_s, ',')) {
                p->isDowned = (down_s == "1") || (p->currentHp <= 0);
                if (std::getline(ss_p, dss_s, ',')) p->deathSaveSuccesses = std::stoi(dss_s);
                if (std::getline(ss_p, dsf_s, ',')) p->deathSaveFailures = std::stoi(dsf_s);
                if (std::getline(ss_p, stab_s, ',')) p->isStable = (stab_s == "1");
                if (std::getline(ss_p, dead_s, ',')) p->isDead = (dead_s == "1");
            } else {
                p->isDowned = (p->currentHp <= 0);
            }
            if (p->isDead) {
                p->isDowned = true;
                p->isStable = false;
                p->currentHp = 0;
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
