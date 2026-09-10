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

void Game::startNewGame(CharacterClass selectedClass, const std::string& playerName,
                        int soloPlayMode, int difficulty,
                        CharacterClass companionClass, bool companionAutoAi) {
    turnOrder_.clear();
    players_.clear();
    enemies_.clear();
    chatHistory_.clear();
    journalEntries_.clear();
    shopInventory_.clear();
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossGoldBonus_ = 0;
    bossSeenGk_ = false;
    bossSeenSk_ = false;
    bossSeenDrake_ = false;
    bossSeenHollow_ = false;
    bossSeenHydra_ = false;
    bossSeenNightfang_ = false;
    pendingRaidKeyDrop_ = false;
    while(!visualEvents_.empty()) visualEvents_.pop();

    gameOver_ = false;
    turnCounter_ = 0;
    roomCount_ = 1;
    dmOnlyTable_ = false;
    roomSearchUsed_ = false;
    isMerchantRoom_ = false;
    dmName_.clear();
    // Local / solo table is always the turn authority (clients opt out via prepareClientJoin).
    isHost_ = true;
    if (soloPlayMode == static_cast<int>(SoloPlayMode::CRAWL))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::CRAWL);
    else if (soloPlayMode == static_cast<int>(SoloPlayMode::RAID))
        soloPlayMode_ = static_cast<int>(SoloPlayMode::RAID);
    else
        soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
    difficulty_ = difficulty;
    if (difficulty_ < 0 || difficulty_ > 3) difficulty_ = static_cast<int>(Difficulty::EASY);

    questLanternRecovered_ = false;
    questComplete_ = false;
    questCryptKeyFound_ = false;
    questAct2Complete_ = false;
    questAct2LedgerFound_ = false;
    questAct3Complete_ = false;
    questAct3SealFound_ = false;

    players_.push_back(std::make_unique<Character>(playerName, selectedClass, "local-player"));
    players_.back()->aiControlled = false;
    {
        CharacterClass cc = companionClass;
        if (static_cast<int>(cc) < 0 || static_cast<int>(cc) > 3) cc = CharacterClass::WIZARD;
        addAlly(companionNameForClass(cc), cc);
        if (!players_.empty()) {
            Character* ally = findNpcCompanion();
            if (ally) ally->aiControlled = companionAutoAi;
        }
    }

    if (soloPlayMode_ == static_cast<int>(SoloPlayMode::RAID)) {
        // Boss Raid: endgame-only focused boss fight(s). Caller must gate story-complete + spend a key.
        questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
        questComplete_ = true;
        questAct2Complete_ = true;
        questAct3Complete_ = true; // endgame loot/gates active for this run
        roomCount_ = 20;
        generateRoomDescription();
        // Force an endgame boss immediately
        enemies_.clear();
        isMerchantRoom_ = false;
        roomSearchUsed_ = false;
        {
            int pick = getRandomInt(0, 2);
            if (pick == 0) spawnNamedBoss("Hollow Crown", 4);
            else if (pick == 1) spawnNamedBoss("Ember Hydra", 5);
            else spawnNamedBoss("Nightfang Matriarch", 4);
            roomDescription_ = "Raid arena — a sealed endgame vault. One Raid Key spent. Defeat the raid boss for rich spoils.";
            lastEvent_ = "BOSS RAID! Stand ready!";
            dmSay("Boss Raid begins. Spend was paid in keys — earn gold, XP, and a chance at Legendary gear and another key.");
        }
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        addChatMessage("System", "Boss Raid begins (difficulty: " + std::string(difficultyName(difficulty_)) + ").");
        return;
    }

    if (soloPlayMode_ == static_cast<int>(SoloPlayMode::CRAWL)) {
        // Dungeon Crawl: skip quest script — classic procedural rooms from the start.
        // Early crawl keeps GK/SK/Drake; true endgame bosses require story complete (Continue from post-Act3).
        questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
        generateRoomDescription();
        spawnRoomContent();
        if (!isMerchantRoom_ && !enemies_.empty()) {
            rollInitiative();
        } else {
            turnOrder_.clear();
            currentTurnIndex_ = 0;
        }
        lastEvent_ = "Dungeon Crawl — Room 1. No story script; clear chambers and press Onward.";
        addChatMessage("System", "Dungeon Crawl begins (difficulty: " + std::string(difficultyName(difficulty_)) + ").");
        dmSay("Welcome, adventurers. I am your Dungeon Master.");
        dmSay("Crawl mode: procedural rooms only — finish the story (Acts 1–3) to unlock endgame bosses and Legendary loot.");
        dmSay("Use Attack, Special, Potion, Search once per clear chamber, Short Rest, and Onward.");
        return;
    }

    // Story Solo: Act 1 → Act 2 → Act 3, then endgame procedural.
    questBeat_ = static_cast<int>(SoloQuestBeat::MILLHOLLOW);
    applyStarterPaddingForStory();
    applySoloQuestRoom();
    rollInitiative();

    lastEvent_ = "Ashen Lantern — Millhollow Road. The village is dark.";
    addChatMessage("System", "A new party begins the Ashen Lantern quest (difficulty: " +
                   std::string(difficultyName(difficulty_)) + ").");
    dmSay("Welcome, adventurers. I am your Dungeon Master.");
    dmSay("Use Attack for a weapon strike, your class Special for a signature move, Potion for healing, Search once per clear chamber, and Short Rest when safe.");
    {
        Character* ally = findNpcCompanion();
        const std::string allyName = ally ? ally->name : std::string("your companion");
        dmSay("You carry an extra supply for this quest — drink a Potion when bloodied; " + allyName + " will help when you are low.");
    }
}

void Game::applyStarterPaddingForStory() {
    // Beginner Solo padding: +2 max HP and +1 potion/feature charge (story only).
    for (auto& p : players_) {
        if (!p) continue;
        p->maxHp += 2;
        p->currentHp = p->maxHp;
        p->maxResources += 1;
        p->resources = p->maxResources;
    }
}

void Game::setDifficulty(int d) {
    if (d < 0 || d > 3) d = static_cast<int>(Difficulty::EASY);
    difficulty_ = d;
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
    roomSearchUsed_ = false;
    dmOnlyTable_ = true;
    resetSoloQuestState();
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
    roomSearchUsed_ = false;
    gameOver_ = false;
    resetSoloQuestState(); // Host-as-DM uses procedural rooms, not solo script
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
    roomSearchUsed_ = false;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    const std::string bossToast = lastEvent_;
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
    if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
        lastEvent_ = bossToast;
        addChatMessage("System", lastEvent_);
    } else {
        lastEvent_ = "DM advanced the party to room " + std::to_string(roomCount_) + ".";
        addChatMessage("System", lastEvent_);
        dmSay("Onward — a new chamber opens before you.");
    }
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
    roomSearchUsed_ = false;
    dmOnlyTable_ = false;
    resetSoloQuestState();
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
    // Solo NPC companions default to AI; joined humans (no "(NPC)") wait for their own input.
    players_.back()->aiControlled = (name.find("(NPC)") != std::string::npos);
    rebuildTurnOrder();
}

std::string Game::companionNameForClass(CharacterClass cl) {
    switch (cl) {
        case CharacterClass::FIGHTER: return "Bren (NPC)";
        case CharacterClass::WIZARD: return "Melf (NPC)";
        case CharacterClass::ROGUE: return "Sable (NPC)";
        case CharacterClass::CLERIC: return "Miren (NPC)";
    }
    return "Melf (NPC)";
}

Character* Game::findNpcCompanion() const {
    for (const auto& p : players_) {
        if (p && p->name.find("(NPC)") != std::string::npos) return p.get();
    }
    return nullptr;
}

std::string Game::getCompanionName() const {
    Character* c = findNpcCompanion();
    return c ? c->name : "";
}

bool Game::isCompanionAutoAi() const {
    Character* c = findNpcCompanion();
    if (!c) return true;
    return c->aiControlled;
}

void Game::setCompanionAutoAi(bool autoAi) {
    Character* c = findNpcCompanion();
    if (!c) return;
    c->aiControlled = autoAi;
    lastEvent_ = c->name + (autoAi ? " will act automatically." : " awaits your commands on their turn.");
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
}

bool Game::setCompanionClass(CharacterClass cl) {
    if (gameOver_ || isInCombat()) return false;
    Character* old = findNpcCompanion();
    if (!old) return false;
    if (static_cast<int>(cl) < 0 || static_cast<int>(cl) > 3) return false;
    if (old->characterClass == cl) {
        // Still refresh name if needed
        std::string want = companionNameForClass(cl);
        if (old->name != want) old->name = want;
        return true;
    }
    const int level = old->level;
    const int xp = old->xp;
    const int gold = old->gold;
    auto keptInv = old->inventory;
    auto keptW = old->equippedWeapon;
    auto keptA = old->equippedArmor;
    const int pending = old->pendingStatPoints;
    const int initiative = old->initiative;
    const bool ai = old->aiControlled;
    const bool down = old->isDowned;
    const bool dead = old->isDead;
    const bool stable = old->isStable;
    const int dss = old->deathSaveSuccesses;
    const int dsf = old->deathSaveFailures;
    const std::string uid = old->uid;
    const std::string newName = companionNameForClass(cl);

    for (auto& p : players_) {
        if (p.get() != old) continue;
        auto neu = std::make_unique<Character>(newName, cl, uid);
        neu->level = level;
        neu->xp = xp;
        neu->gold = gold;
        neu->pendingStatPoints = pending;
        neu->initiative = initiative;
        neu->aiControlled = ai;
        neu->inventory = keptInv;
        // Keep gear if still class-legal; else stash to bag.
        neu->equippedWeapon = nullptr;
        neu->equippedArmor = nullptr;
        if (keptW) {
            if (keptW->canEquip(cl)) neu->equippedWeapon = keptW;
            else neu->addToInventory(keptW);
        }
        if (keptA) {
            if (keptA->canEquip(cl)) neu->equippedArmor = keptA;
            else neu->addToInventory(keptA);
        }
        if (!neu->equippedWeapon || !neu->equippedArmor) {
            // Ensure starters fill empty slots without wiping bag.
            const int cls = static_cast<int>(cl);
            if (!neu->equippedWeapon) {
                if (cl == CharacterClass::FIGHTER) neu->equippedWeapon = Item::make("Longsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::ROGUE) neu->equippedWeapon = Item::make("Shortsword", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::WIZARD) neu->equippedWeapon = Item::make("Quarterstaff", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
                else neu->equippedWeapon = Item::make("Mace", ItemType::WEAPON, 0, ItemRarity::COMMON, cls);
            }
            if (!neu->equippedArmor) {
                if (cl == CharacterClass::FIGHTER) neu->equippedArmor = Item::make("Chain Shirt", ItemType::ARMOR, 3, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::ROGUE) neu->equippedArmor = Item::make("Leather Armor", ItemType::ARMOR, 1, ItemRarity::COMMON, cls);
                else if (cl == CharacterClass::WIZARD) neu->equippedArmor = Item::make("Traveler Clothes", ItemType::ARMOR, 0, ItemRarity::COMMON, -1);
                else neu->equippedArmor = Item::make("Scale Mail", ItemType::ARMOR, 4, ItemRarity::COMMON, cls);
            }
        }
        neu->applyStatsForLevel();
        neu->calculateAC();
        neu->currentHp = neu->maxHp;
        neu->resources = neu->maxResources;
        neu->isDowned = down;
        neu->isDead = dead;
        neu->isStable = stable;
        neu->deathSaveSuccesses = dss;
        neu->deathSaveFailures = dsf;
        if (dead || down) neu->currentHp = 0;
        // Story padding: if hero had padding, companion already got it at start via applyStarterPaddingForStory on all.
        p = std::move(neu);
        break;
    }
    rebuildTurnOrder();
    lastEvent_ = newName + " takes the field as a " +
        (cl == CharacterClass::FIGHTER ? "Fighter" :
         cl == CharacterClass::WIZARD ? "Wizard" :
         cl == CharacterClass::ROGUE ? "Rogue" : "Cleric") + ".";
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
    return true;
}

void Game::resetSoloQuestState() {
    questBeat_ = static_cast<int>(SoloQuestBeat::NONE);
    questLanternRecovered_ = false;
    questComplete_ = false;
    questCryptKeyFound_ = false;
    questAct2Complete_ = false;
    questAct2LedgerFound_ = false;
    questAct3Complete_ = false;
    questAct3SealFound_ = false;
    pendingRaidKeyDrop_ = false;
    soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
}

void Game::applySoloQuestRoom() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false;

    const QuestBeatScript* beat = findQuestBeat(questBeat_);
    if (!beat) {
        // Fallback to procedural if beat table missing
        generateRoomDescription();
        spawnRoomContent();
        return;
    }

    roomDescription_ = std::string(beat->roomDescription);
    dmSay(std::string("[") + beat->title + "] " + beat->dmEnter);
    dmSay(roomDescription_);
    addJournalEntry(beat->journalOnEnter);

    spawnSoloQuestEnemies();
    maybeFinishQuestOnResolutionEnter();
    maybeFinishAct2OnSettledEnter();
    maybeFinishAct3OnSealedEnter();

    if (enemies_.empty()) {
        // Soft-open story beats: Search / Rest / Onward available immediately.
        turnOrder_.clear();
        currentTurnIndex_ = 0;
        lastEvent_ = std::string(beat->title) + " — chamber open. Search, Rest, or Onward.";
    } else {
        lastEvent_ = std::string(beat->title) + " — foes ahead!";
    }
}

void Game::spawnSoloQuestEnemies() {
    enemies_.clear();
    const auto beat = static_cast<SoloQuestBeat>(questBeat_);

    // Solo Ashen Lantern: tune early rooms for beginners. attackStatDelta lowers to-hit/damage
    // (and Rogue AC via Dex). resourceCap strips Action Surge / Sneak so early foes don't one-shot.
    // Post-quest procedural spawns are unchanged.
    auto makeFoe = [&](const std::string& name, CharacterClass cl, int bonusHp, int bonusAc,
                       int resourceCap = -1, int attackStatDelta = 0) {
        auto foe = std::make_unique<Character>(name, cl, name + "-" + std::to_string(getRandomInt(0, 1000000)));
        if (attackStatDelta != 0) {
            switch (cl) {
                case CharacterClass::ROGUE:
                    foe->attributes.dexterity = std::max(3, foe->attributes.dexterity + attackStatDelta);
                    break;
                case CharacterClass::FIGHTER:
                    foe->attributes.strength = std::max(3, foe->attributes.strength + attackStatDelta);
                    break;
                case CharacterClass::WIZARD:
                    foe->attributes.intelligence = std::max(3, foe->attributes.intelligence + attackStatDelta);
                    break;
                case CharacterClass::CLERIC:
                    foe->attributes.wisdom = std::max(3, foe->attributes.wisdom + attackStatDelta);
                    break;
            }
            foe->calculateAC();
        }
        foe->maxHp = std::max(4, foe->maxHp + bonusHp);
        foe->currentHp = foe->maxHp;
        foe->armorClass += bonusAc;
        if (resourceCap >= 0) {
            foe->maxResources = resourceCap;
            foe->resources = resourceCap;
        }
        enemies_.push_back(std::move(foe));
    };

    switch (beat) {
        case SoloQuestBeat::MILLHOLLOW:
            // Soft open: one weak scout (was HP-2 full Rogue with Sneak).
            makeFoe("Goblin Scout", CharacterClass::ROGUE, -4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::THORNPATH:
            // Always a single wolf — no random second goblin.
            makeFoe("Wolf", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::CRYPT_DOORS:
            // Puzzle-lite soft open: Search once for the rune-key (no mandatory fight).
            break;
        case SoloQuestBeat::BONE_GALLERY:
            // Medium+: small chance the gallery hosts the Skeleton King (alone). Easy keeps soft pair.
            if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM) && getRandomInt(1, 100) <= 18) {
                spawnNamedBoss("Skeleton King", 2);
                lastEvent_ = "BOSS! Skeleton King — stand ready!";
                dmSay("A crowned horror stirs among the bones — the Skeleton King (original foe).");
            } else {
                makeFoe("Skeleton", CharacterClass::FIGHTER, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
                makeFoe("Skeleton", CharacterClass::FIGHTER, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            }
            break;
        case SoloQuestBeat::LANTERN_VAULT:
            // Climax retained: one Champion, HP/AC one notch down, single surge max.
            makeFoe("Skeleton Champion", CharacterClass::FIGHTER, 12, 1, /*resourceCap=*/1, /*attackStatDelta=*/-2);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The vault guardian stirs — a Skeleton Champion. It can surge once; keep a Potion ready and let " + who + " help.");
            }
            break;
        case SoloQuestBeat::RESOLUTION:
            // Shrine: no fight — resolution beat.
            break;
        case SoloQuestBeat::ACT2_GREEN:
            // Soft open fight: one debt enforcer (bandit-flavored fighter), toned down.
            makeFoe("Debt Enforcer", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT2_WEIR:
            // Medium+: rare Goblin King ambush at the weir (Easy keeps soft river goblins).
            if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM) && getRandomInt(1, 100) <= 16) {
                spawnNamedBoss("Goblin King", 1);
                lastEvent_ = "BOSS! Goblin King — stand ready!";
                dmSay("Scrap-iron crown by the millrace — the Goblin King (original) bars the path!");
            } else {
                makeFoe("River Goblin", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
                makeFoe("River Goblin", CharacterClass::ROGUE, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            }
            break;
        case SoloQuestBeat::ACT2_CELLAR:
            makeFoe("Skeleton", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            makeFoe("Giant Rat", CharacterClass::ROGUE, -2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-3);
            break;
        case SoloQuestBeat::ACT2_LOFT:
            // Soft open: Search for the coerced ledger.
            break;
        case SoloQuestBeat::ACT2_HALL:
            makeFoe("The Collector", CharacterClass::FIGHTER, 14, 1, /*resourceCap=*/1, /*attackStatDelta=*/-1);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The Collector wears a false mill seal. Keep a Potion ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT2_SETTLED:
            break;
        case SoloQuestBeat::ACT3_RUMOR:
            makeFoe("Ash Scavenger", CharacterClass::ROGUE, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_WELL:
            makeFoe("Giant Rat", CharacterClass::ROGUE, 0, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            makeFoe("Root Goblin", CharacterClass::ROGUE, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_ROOTS:
            makeFoe("Skeleton", CharacterClass::FIGHTER, 4, 0, /*resourceCap=*/0, /*attackStatDelta=*/-1);
            makeFoe("Skeleton", CharacterClass::FIGHTER, 2, 0, /*resourceCap=*/0, /*attackStatDelta=*/-2);
            break;
        case SoloQuestBeat::ACT3_RELIC:
            // Soft open: Search for Ember Seal
            break;
        case SoloQuestBeat::ACT3_WARDEN:
            makeFoe("Breach Warden", CharacterClass::FIGHTER, 22, 2, /*resourceCap=*/2, /*attackStatDelta=*/0);
            {
                Character* ally = findNpcCompanion();
                std::string who = ally ? ally->name : "your companion";
                dmSay("The Breach Warden bars the ember wound. Keep potions ready — " + who + " will help.");
            }
            break;
        case SoloQuestBeat::ACT3_SEALED:
            break;
        default:
            break;
    }
}

void Game::grantAshenLantern(Character* actor) {
    if (questLanternRecovered_) return;
    questLanternRecovered_ = true;
    Character* hero = actor;
    if (!hero || hero->isDead) {
        hero = nullptr;
        for (auto& p : players_) {
            if (p && !p->isDead) { hero = p.get(); break; }
        }
    }
    if (hero) {
        // Thematic quest item: modest weapon blessing without replacing class identity harshly.
        hero->equippedWeapon = Item::make("Ashen Lantern", ItemType::WEAPON, 1, ItemRarity::RARE, -1);
        hero->gold += 25;
    }
    dmSay("You seize the Ashen Lantern. Grey glass drinks the dark — Millhollow's hope is in your hands.");
    addJournalEntry("Recovered the Ashen Lantern (questLanternRecovered).");
    addChatMessage("Quest", "Ashen Lantern recovered!");
    lastEvent_ = "Ashen Lantern recovered! Rest if needed, then Onward to the shrine.";
}

void Game::maybeFinishQuestOnResolutionEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::RESOLUTION) return;
    if (!questLanternRecovered_) {
        // Safety: if vault was skipped somehow, still grant on shrine enter.
        grantAshenLantern(players_.empty() ? nullptr : players_[0].get());
    }
    if (!questComplete_) {
        questComplete_ = true;
        dmSay("You set the Ashen Lantern on the shrine. Warm ash-light blooms. Night-things withdraw from Millhollow.");
        dmSay("Act 1 complete. Rest if you need, then Onward — Millhollow's Debt waits on the green.");
        addJournalEntry("Ashen Lantern quest complete — lantern lit at the crypt shrine.");
        addChatMessage("Quest", "Act 1 done — Ashen Lantern. Act 2 awaits.");
    }
}

void Game::maybeFinishAct2OnSettledEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT2_SETTLED) return;
    if (!questAct2LedgerFound_) {
        questAct2LedgerFound_ = true;
        addJournalEntry("Safety: coerced ledger counted as recovered at Debt Settled.");
    }
    if (!questAct2Complete_) {
        questAct2Complete_ = true;
        dmSay("The coerced ledger burns. The false mill seal cracks. Collectors melt into the mist.");
        dmSay("Millhollow's Debt is settled. Rest if you need — then Onward for Act 3: Emberdeep Breach.");
        addJournalEntry("Millhollow's Debt complete — ledger destroyed; false seal broken.");
        addChatMessage("Quest", "Act 2 done — Millhollow's Debt. Act 3 awaits.");
        if (players_.empty() == false && players_[0]) {
            players_[0]->gold += 40;
        }
    }
}

void Game::maybeFinishAct3OnSealedEnter() {
    if (static_cast<SoloQuestBeat>(questBeat_) != SoloQuestBeat::ACT3_SEALED) return;
    if (!questAct3SealFound_) {
        questAct3SealFound_ = true;
        addJournalEntry("Safety: Ember Seal counted as recovered at Breach Sealed.");
    }
    if (!questAct3Complete_) {
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        dmSay("The Ember Seal locks. Ember-light dies. Cool air returns up the well to Millhollow.");
        dmSay("Story complete (Acts 1–3). Endgame crawl bosses, Legendary gear, and Boss Raids unlock.");
        addJournalEntry("Emberdeep Breach complete — story finished; endgame unlocked.");
        addChatMessage("Quest", "Act 3 done — Story complete! Endgame unlocked.");
        if (!players_.empty() && players_[0]) {
            players_[0]->gold += 60;
        }
    }
}

bool Game::endgameContentAllowed() const {
    // Raid runs always treat endgame as open; otherwise require Act 3 complete.
    if (isBossRaid()) return true;
    return questAct3Complete_;
}

bool Game::trySoloQuestSearch(Character* hero) {
    if (!hero || !isSoloQuestScripted()) return false;
    const auto beat = static_cast<SoloQuestBeat>(questBeat_);

    if (beat == SoloQuestBeat::MILLHOLLOW) {
        hero->gold += 8;
        lastEvent_ = hero->name + " finds a soot-stained note: \"Hollowbarrow Crypt. Bring no open flame — the Ashen Lantern alone.\"";
        dmSay("A villager's scrap points to Hollowbarrow Crypt. The Ashen Lantern must burn there — or return to the green.");
        addJournalEntry("Clue: Hollowbarrow Crypt holds the Ashen Lantern; open flame is unwelcome.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::CRYPT_DOORS) {
        questCryptKeyFound_ = true;
        lastEvent_ = hero->name + " pries a cold iron rune-key from the ash-stained lintel.";
        dmSay("The rune-key turns. Crypt wards sigh open — the Bone Gallery waits beyond.");
        addJournalEntry("Found the Ashen Rune-Key at Hollowbarrow's doors.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::LANTERN_VAULT && questLanternRecovered_) {
        hero->gold += 12;
        lastEvent_ = hero->name + " finds spare oil and 12 gold near the plinth.";
        dmSay("A pouch of oil and coin — useful, but the lantern itself is the true prize.");
        addJournalEntry(lastEvent_);
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::RESOLUTION) {
        lastEvent_ = hero->name + " finds soot-spirals on the altar — the shrine remembers every night the lantern burned.";
        dmSay("Nothing more to take. Millhollow's thanks will be quieter than gold. Onward leads to Millhollow's Debt.");
        addJournalEntry("Searched the Ashen Shrine — Act 1 is won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_GREEN) {
        hero->gold += 10;
        lastEvent_ = hero->name + " overhears a collector mutter: \"The loft ledger above the weir seals every name.\"";
        dmSay("A clue: the coerced debt ledger is kept in the mill loft above the weir.");
        addJournalEntry("Clue: coerced ledger is in the mill loft above the weir.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_LOFT) {
        questAct2LedgerFound_ = true;
        hero->gold += 15;
        lastEvent_ = hero->name + " pulls the coerced debt ledger from the shelf — fresh ink, false mill seal stamped on every page.";
        dmSay("You have the ledger. Onward to Collector's Hall — confront the shakedown.");
        addJournalEntry("Recovered the coerced debt ledger (questAct2LedgerFound).");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT2_SETTLED) {
        lastEvent_ = hero->name + " finds only ash in the clay bowl — the debt is already settled.";
        dmSay("Nothing more to take. Onward leads to the old well — Emberdeep Breach.");
        addJournalEntry("Searched after Debt Settled — Act 2 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RUMOR) {
        hero->gold += 12;
        lastEvent_ = hero->name + " hears: \"The Ember Seal sleeps in a niche past the root maze — only it can close the wound.\"";
        dmSay("Clue: recover the Ember Seal, then face whatever guards the breach.");
        addJournalEntry("Clue: Ember Seal lies past the Root Labyrinth.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_RELIC) {
        questAct3SealFound_ = true;
        hero->gold += 20;
        lastEvent_ = hero->name + " lifts the Ember Seal — iron disc, still warm, etched with ash-runes.";
        dmSay("You have the Ember Seal. Onward to the Breach Threshold — the Warden waits.");
        addJournalEntry("Recovered the Ember Seal (questAct3SealFound).");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    if (beat == SoloQuestBeat::ACT3_SEALED) {
        lastEvent_ = hero->name + " finds cooling ash where the breach once roared — the seal holds.";
        dmSay("Nothing more to take. Story is complete — Onward opens endgame rooms.");
        addJournalEntry("Searched after Breach Sealed — Act 3 already won.");
        addChatMessage("Search", lastEvent_);
        return true;
    }
    return false; // use default Search loot/trap
}

void Game::generateRoomDescription() {
    if (isSoloQuestScripted()) {
        // Description already set by applySoloQuestRoom; keep procedural helper for post-quest / DM.
        return;
    }
    if (questAct3Complete_ || questAct2Complete_ || (questComplete_ && questBeat_ == static_cast<int>(SoloQuestBeat::POST_QUEST))) {
        std::vector<std::string> adjectives = {"dark", "damp", "ancient", "dusty", "eerie", "forgotten", "cursed"};
        std::vector<std::string> rooms = {"chamber", "hallway", "library", "crypt", "vault", "shrine", "laboratory"};
        std::stringstream ss;
        const char* tag = questAct3Complete_ ? "[Endgame] "
            : (questAct2Complete_ ? "[After Act 2] " : "[Main quest done] ");
        ss << tag << "You press into a " << adjectives[static_cast<size_t>(getRandomInt(0, static_cast<int>(adjectives.size()) - 1))]
           << " " << rooms[static_cast<size_t>(getRandomInt(0, static_cast<int>(rooms.size()) - 1))] << ". ";
        roomDescription_ = ss.str();
        dmSay("Room " + std::to_string(roomCount_) + ": " + roomDescription_ + "What do you do?");
        return;
    }
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
    roomSearchUsed_ = false;

    if (isSoloQuestScripted()) {
        spawnSoloQuestEnemies();
        return;
    }

    if (roomCount_ > 1 && roomCount_ % 4 == 0) {
        isMerchantRoom_ = true;
        roomDescription_ = "You find a rare pocket of safety. A weary Merchant awaits.";
        dmSay("A lantern glows ahead. A traveling merchant offers goods — and a chance for a longer rest from the grind.");
        restockShop();
        return;
    }

    // Boss chance for deep crawl / post-quest procedural rooms (gated so beginners are not soft-locked).
    maybeSpawnBossEncounter();
    if (!enemies_.empty()) return;

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
    for (auto& p : players_) { if (p) p->fightShieldUsed = false; }

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

void Game::restockShop() {
    shopInventory_.clear();
    int depthBonus = std::max(0, roomCount_ / 4);
    int preferA = -1, preferB = -1;
    partyPreferClasses(preferA, preferB);

    auto isPartyClass = [&](int cls) {
        if (cls < 0) return true; // Any
        return cls == preferA || cls == preferB;
    };
    auto classWeapon = [&](int cls, ItemRarity r, int bonus) {
        if (cls == 0) return Item::make("Fighter's Arming Sword", ItemType::WEAPON, bonus, r, 0);
        if (cls == 1) return Item::make("Wizard's Focus Rod", ItemType::WEAPON, bonus, r, 1);
        if (cls == 2) return Item::make("Rogue's Stiletto", ItemType::WEAPON, bonus, r, 2);
        return Item::make("Cleric's Warhammer", ItemType::WEAPON, bonus, r, 3);
    };
    auto classArmor = [&](int cls, ItemRarity r, int bonus) {
        if (cls == 0) return Item::make("Knight Plate", ItemType::ARMOR, bonus, r, 0);
        if (cls == 1) return Item::make("Scholar Robes", ItemType::ARMOR, bonus, r, 1);
        if (cls == 2) return Item::make("Veil of Shadows", ItemType::ARMOR, bonus, r, 2);
        return Item::make("Reliquary Mail", ItemType::ARMOR, bonus, r, 3);
    };

    // Always: Any commons + potions (usable by everyone).
    shopInventory_.push_back(Item::make("Traveler Blade", ItemType::WEAPON, depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Padded Vest", ItemType::ARMOR, depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Healing Draught", ItemType::POTION, 8 + depthBonus, ItemRarity::COMMON, -1));
    shopInventory_.push_back(Item::make("Greater Potion", ItemType::POTION, 14 + depthBonus, ItemRarity::UNCOMMON, -1));

    // Party stock: commons/uncommons dominate; Rare/Epic gated deeper (#46). No Legendary in shop.
    if (preferA >= 0) {
        shopInventory_.push_back(classWeapon(preferA, ItemRarity::UNCOMMON, depthBonus + 1));
        if (roomCount_ >= 10)
            shopInventory_.push_back(classArmor(preferA, ItemRarity::RARE, depthBonus + 2));
        else if (roomCount_ >= 5)
            shopInventory_.push_back(classArmor(preferA, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    if (preferB >= 0 && preferB != preferA) {
        shopInventory_.push_back(classWeapon(preferB, ItemRarity::UNCOMMON, depthBonus + 1));
        if (roomCount_ >= 12)
            shopInventory_.push_back(classArmor(preferB, ItemRarity::RARE, depthBonus + 2));
        else if (roomCount_ >= 6)
            shopInventory_.push_back(classArmor(preferB, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    int off = -1;
    for (int c = 0; c < 4; ++c) {
        if (!isPartyClass(c)) { off = c; break; }
    }
    if (off >= 0) {
        shopInventory_.push_back(classWeapon(off, ItemRarity::UNCOMMON, depthBonus + 1));
    }
    if (roomCount_ >= 16 && preferA >= 0) {
        shopInventory_.push_back(classWeapon(preferA, ItemRarity::EPIC, depthBonus + 3));
    }
    if (roomCount_ >= 20 && preferB >= 0) {
        shopInventory_.push_back(classArmor(preferB, ItemRarity::EPIC, depthBonus + 4));
    }
}

bool Game::buyItem(const std::string& playerName, int itemIndex) {
    if (gameOver_) return false;
    if (!isMerchantRoom_ || itemIndex < 0 || static_cast<size_t>(itemIndex) >= shopInventory_.size()) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;

    auto item = shopInventory_[static_cast<size_t>(itemIndex)];
    int cost = item->shopCost();

    if (hero->gold >= cost) {
        hero->gold -= cost;
        if (item->type == ItemType::POTION) {
            hero->heal(item->bonus);
            lastEvent_ = hero->name + " purchased and drank " + item->name + "!";
        } else {
            hero->addToInventory(item);
            lastEvent_ = hero->name + " purchased " + item->name + " [" + item->rarityLabel() + "] — open Inventory to equip.";
        }
        addJournalEntry(hero->name + " bought " + item->name + " for " + std::to_string(cost) + " gold.");
        shopInventory_.erase(shopInventory_.begin() + itemIndex);
        return true;
    }
    lastEvent_ = "Not enough gold!";
    return false;
}

std::string Game::getShopManifest() const {
    if (!isMerchantRoom_) return "";
    std::stringstream ss;
    for (size_t i = 0; i < shopInventory_.size(); ++i) {
        const auto& it = shopInventory_[i];
        // idx:name|bonus|type|rarity|class|cost
        ss << i << ":" << it->name << "|" << it->bonus << "|"
           << (it->type == ItemType::WEAPON ? "Weapon" : (it->type == ItemType::ARMOR ? "Armor" : "Potion"))
           << "|" << it->rarityLabel() << "|" << it->classLabel() << "|" << it->shopCost() << ";";
    }
    return ss.str();
}

std::string Game::getInventoryManifest(const std::string& playerName) const {
    Character* hero = nullptr;
    for (const auto& p : players_) {
        if (p && p->name == playerName) { hero = p.get(); break; }
    }
    if (!hero) return "";
    std::stringstream ss;
    auto emit = [&](const std::shared_ptr<Item>& it, const char* slot, int index) {
        if (!it) return;
        // idx:name|bonus|type|rarity|class|slot|upgradeLevel|upgradeCost|sellPrice
        // idx:name|bonus|type|rarity|class|slot|upgradeLevel|upgradeCost|sellPrice|legText
        std::string leg = it->legendaryBonusText();
        for (char& ch : leg) { if (ch == ';' || ch == '|' || ch == ':') ch = ','; }
        ss << index << ":" << it->name << "|" << it->bonus << "|"
           << (it->type == ItemType::WEAPON ? "Weapon" : (it->type == ItemType::ARMOR ? "Armor" : "Potion"))
           << "|" << it->rarityLabel() << "|" << it->classLabel() << "|" << slot
           << "|" << it->upgradeLevel << "|" << it->upgradeCost()
           << "|" << it->sellPrice() << "|" << leg << ";";
    };
    // Equipped first with negative-ish slots encoded as weapon/armor indices in slot field
    emit(hero->equippedWeapon, "weapon", -1);
    emit(hero->equippedArmor, "armor", -2);
    for (size_t i = 0; i < hero->inventory.size(); ++i) {
        emit(hero->inventory[i], "bag", static_cast<int>(i));
    }
    return ss.str();
}

bool Game::equipInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = hero->inventory[static_cast<size_t>(invIndex)];
    if (!item || item->type == ItemType::POTION) {
        lastEvent_ = "Cannot equip that.";
        return false;
    }
    if (!item->canEquip(hero->characterClass)) {
        lastEvent_ = item->classLabel() + " only.";
        return false;
    }
    hero->inventory.erase(hero->inventory.begin() + invIndex);
    if (item->type == ItemType::WEAPON) {
        if (hero->equippedWeapon) hero->addToInventory(hero->equippedWeapon);
        hero->equippedWeapon = item;
        hero->calculateAC(); // Legendary DEX on weapons can affect AC
    } else {
        if (hero->equippedArmor) hero->addToInventory(hero->equippedArmor);
        hero->equippedArmor = item;
        hero->calculateAC();
    }
    lastEvent_ = hero->name + " equips " + item->getDescription() + ".";
    return true;
}

bool Game::unequipSlot(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (slot == 0) {
        if (!hero->equippedWeapon) { lastEvent_ = "No weapon equipped."; return false; }
        hero->addToInventory(hero->equippedWeapon);
        lastEvent_ = hero->name + " unequips " + hero->equippedWeapon->name + ".";
        hero->equippedWeapon = nullptr;
        hero->calculateAC();
        return true;
    }
    if (slot == 1) {
        if (!hero->equippedArmor) { lastEvent_ = "No armor equipped."; return false; }
        hero->addToInventory(hero->equippedArmor);
        lastEvent_ = hero->name + " unequips " + hero->equippedArmor->name + ".";
        hero->equippedArmor = nullptr;
        hero->calculateAC();
        return true;
    }
    lastEvent_ = "Bad slot.";
    return false;
}

bool Game::upgradeInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    std::string err;
    auto& item = hero->inventory[static_cast<size_t>(invIndex)];
    int cost = item ? item->upgradeCost() : 0;
    if (!hero->upgradeOwnedItem(item, err)) {
        lastEvent_ = err;
        return false;
    }
    lastEvent_ = hero->name + " upgrades " + item->name + " to +" + std::to_string(item->bonus)
        + " for " + std::to_string(cost) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::upgradeEquippedItem(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    std::shared_ptr<Item>* ptr = (slot == 0) ? &hero->equippedWeapon : (slot == 1 ? &hero->equippedArmor : nullptr);
    if (!ptr || !*ptr) { lastEvent_ = "Nothing equipped there."; return false; }
    std::string err;
    int cost = (*ptr)->upgradeCost();
    if (!hero->upgradeOwnedItem(*ptr, err)) {
        lastEvent_ = err;
        return false;
    }
    lastEvent_ = hero->name + " upgrades " + (*ptr)->name + " to +" + std::to_string((*ptr)->bonus)
        + " for " + std::to_string(cost) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::sellInventoryItem(const std::string& playerName, int invIndex) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= hero->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = hero->inventory[static_cast<size_t>(invIndex)];
    if (!item) {
        lastEvent_ = "No such item.";
        return false;
    }
    int price = item->sellPrice();
    std::string itemName = item->getDescription();
    hero->inventory.erase(hero->inventory.begin() + invIndex);
    hero->gold += price;
    lastEvent_ = hero->name + " sells " + itemName + " for " + std::to_string(price) + "g.";
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::sellEquippedItem(const std::string& playerName, int slot) {
    if (gameOver_) return false;
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead) return false;
    std::shared_ptr<Item>* ptr = (slot == 0) ? &hero->equippedWeapon : (slot == 1 ? &hero->equippedArmor : nullptr);
    if (!ptr || !*ptr) {
        lastEvent_ = "Nothing equipped there.";
        return false;
    }
    auto item = *ptr;
    int price = item->sellPrice();
    std::string itemName = item->getDescription();
    // Unequip first: clear slot so combat falls back to fists / unarmored AC.
    *ptr = nullptr;
    if (slot == 1) hero->calculateAC();
    hero->gold += price;
    lastEvent_ = hero->name + " sells " + itemName + " for " + std::to_string(price) + "g."
        + (slot == 0 ? " (fists ready)" : "");
    addJournalEntry(hero->name + " sold equipped " + itemName + " for " + std::to_string(price) + "g.");
    return true;
}

void Game::partyPreferClasses(int& outA, int& outB) const {
    outA = -1;
    outB = -1;
    if (!players_.empty() && players_[0]) {
        outA = static_cast<int>(players_[0]->characterClass);
    }
    if (Character* c = findNpcCompanion()) {
        outB = static_cast<int>(c->characterClass);
    }
}

Character* Game::findTransferAlly(const Character* from) const {
    if (!from) return nullptr;
    // Prefer NPC companion when transferring from the hero.
    if (Character* npc = findNpcCompanion()) {
        if (npc != from) return npc;
    }
    for (const auto& p : players_) {
        if (p && p.get() != from) return p.get();
    }
    return nullptr;
}

void Game::deliverItemToAlly(Character* ally, std::shared_ptr<Item> item) {
    if (!ally || !item) return;
    if (item->type == ItemType::POTION) {
        ally->addToInventory(item);
        return;
    }
    if (item->canEquip(ally->characterClass)) {
        if (item->type == ItemType::WEAPON) {
            if (ally->equippedWeapon) ally->addToInventory(ally->equippedWeapon);
            ally->equippedWeapon = item;
            return;
        }
        if (item->type == ItemType::ARMOR) {
            if (ally->equippedArmor) ally->addToInventory(ally->equippedArmor);
            ally->equippedArmor = item;
            ally->calculateAC();
            return;
        }
    }
    ally->addToInventory(item);
}

bool Game::transferInventoryItemToAlly(const std::string& fromName, int invIndex) {
    if (gameOver_) return false;
    Character* from = findCharacter(fromName);
    if (!from || from->isDead) return false;
    Character* ally = findTransferAlly(from);
    if (!ally) {
        lastEvent_ = "No companion to transfer to.";
        return false;
    }
    if (invIndex < 0 || static_cast<size_t>(invIndex) >= from->inventory.size()) {
        lastEvent_ = "No such item.";
        return false;
    }
    auto item = from->inventory[static_cast<size_t>(invIndex)];
    if (!item) {
        lastEvent_ = "No such item.";
        return false;
    }
    from->inventory.erase(from->inventory.begin() + invIndex);
    std::string label = item->getDescription();
    bool equipped = item->canEquip(ally->characterClass) && item->type != ItemType::POTION;
    deliverItemToAlly(ally, item);
    if (equipped) {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + " (equipped).";
    } else {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + ".";
    }
    dmSay(lastEvent_);
    addJournalEntry(lastEvent_);
    return true;
}

bool Game::transferEquippedItemToAlly(const std::string& fromName, int slot) {
    if (gameOver_) return false;
    Character* from = findCharacter(fromName);
    if (!from || from->isDead) return false;
    Character* ally = findTransferAlly(from);
    if (!ally) {
        lastEvent_ = "No companion to transfer to.";
        return false;
    }
    std::shared_ptr<Item>* ptr = (slot == 0) ? &from->equippedWeapon : (slot == 1 ? &from->equippedArmor : nullptr);
    if (!ptr || !*ptr) {
        lastEvent_ = "Nothing equipped there.";
        return false;
    }
    auto item = *ptr;
    *ptr = nullptr;
    if (slot == 1) from->calculateAC();
    std::string label = item->getDescription();
    bool willEquip = item->canEquip(ally->characterClass);
    deliverItemToAlly(ally, item);
    if (willEquip) {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + " (equipped).";
    } else {
        lastEvent_ = from->name + " gives " + label + " to " + ally->name + ".";
    }
    dmSay(lastEvent_);
    addJournalEntry(lastEvent_);
    return true;
}

void Game::markBossSeen(const std::string& name) {
    int tier = LootSystem::bossTier(name);
    if (tier == 1) bossSeenGk_ = true;
    else if (tier == 2) bossSeenSk_ = true;
    else if (name.find("Ashen Drake") != std::string::npos || name.find("Dragon") != std::string::npos)
        bossSeenDrake_ = true;
    else if (name.find("Hollow Crown") != std::string::npos) bossSeenHollow_ = true;
    else if (name.find("Ember Hydra") != std::string::npos) bossSeenHydra_ = true;
    else if (name.find("Nightfang Matriarch") != std::string::npos) bossSeenNightfang_ = true;
    else if (tier >= 3) bossSeenDrake_ = true;
}

void Game::spawnNamedBoss(const std::string& name, int tier) {
    markBossSeen(name);
    CharacterClass cl = CharacterClass::FIGHTER;
    if (tier == 1) cl = CharacterClass::ROGUE;
    else if (tier == 3) cl = CharacterClass::WIZARD;
    else if (tier >= 4) {
        if (name.find("Ember Hydra") != std::string::npos) cl = CharacterClass::WIZARD;
        else if (name.find("Nightfang") != std::string::npos) cl = CharacterClass::ROGUE;
        else cl = CharacterClass::FIGHTER;
    }
    auto foe = std::make_unique<Character>(name, cl, name + "-" + std::to_string(getRandomInt(0, 1000000)));
    int hpBonus = 10 + tier * 8 + roomCount_;
    int acBonus = std::min(6, tier);
    int atkDelta = (tier >= 3) ? 0 : -1;
    if (tier >= 4) {
        hpBonus += 12 + (tier - 3) * 10;
        acBonus += 1;
        atkDelta += 1;
    }
    if (difficulty_ <= static_cast<int>(Difficulty::EASY)) {
        hpBonus = std::max(8, hpBonus - 8);
        acBonus = std::max(0, acBonus - 1);
        atkDelta -= 1;
    } else if (difficulty_ == static_cast<int>(Difficulty::MEDIUM)) {
        hpBonus = std::max(10, hpBonus - 4);
    } else if (difficulty_ >= static_cast<int>(Difficulty::HARD)) {
        hpBonus += 6 + tier * 2;
        acBonus += 1;
    }
    foe->maxHp += hpBonus;
    foe->currentHp = foe->maxHp;
    foe->armorClass += acBonus;
    foe->resources = std::min(foe->maxResources, std::max(1, tier));
    if (cl == CharacterClass::ROGUE) foe->attributes.dexterity = std::max(8, foe->attributes.dexterity + atkDelta);
    else if (cl == CharacterClass::WIZARD) foe->attributes.intelligence = std::max(8, foe->attributes.intelligence + atkDelta);
    else foe->attributes.strength = std::max(8, foe->attributes.strength + atkDelta);
    enemies_.push_back(std::move(foe));
}

bool Game::hasLivingBossEnemy() const {
    for (const auto& e : enemies_) {
        if (e && !e->isDead && LootSystem::isBossName(e->name)) return true;
    }
    return false;
}

void Game::maybeSpawnBossEncounter() {
    // Never soft-lock early story beginners: only procedural crawl / post-quest rooms.
    if (isSoloQuestScripted()) return;

    const bool easy = difficulty_ <= static_cast<int>(Difficulty::EASY);
    const bool endgame = endgameContentAllowed();
    const int gkNeed = easy ? 5 : 4;
    const int skNeed = easy ? 9 : 8;
    const int dragonNeed = easy ? 14 : 12;
    const int endNeed = easy ? 22 : 18;
    const int gkChance = easy ? 32 : 34;
    const int skChance = easy ? 26 : 28;
    const int dragonChance = easy ? 14 : 16;
    const int endChance = easy ? 18 : 22;

    auto spawnGkCourt = [&]() {
        spawnNamedBoss("Goblin King", 1);
        if (difficulty_ >= static_cast<int>(Difficulty::MEDIUM)) {
            auto scout = std::make_unique<Character>("Goblin Scout", CharacterClass::ROGUE, "gk-scout-" + std::to_string(getRandomInt(0, 1000000)));
            scout->maxHp = std::max(4, scout->maxHp - 4);
            scout->currentHp = scout->maxHp;
            enemies_.push_back(std::move(scout));
        }
        roomDescription_ = "Crude banners hang from spikes. The Goblin King bellows a challenge from a scrap-iron throne.";
        lastEvent_ = "BOSS! Goblin King — stand ready!";
        dmSay("The Goblin King! Original boss — richer spoils if you prevail.");
        addJournalEntry("Boss: Goblin King.");
    };

    if (roomCount_ >= 10 && !bossSeenGk_) {
        spawnGkCourt();
        return;
    }

    if (roomCount_ < gkNeed) return;

    int roll = getRandomInt(1, 100);

    // Endgame-only originals — require story Acts 1–3 complete (or Boss Raid).
    if (endgame && roomCount_ >= endNeed && roll <= endChance) {
        int pick = getRandomInt(0, 2);
        auto place = [&](const std::string& nm, int tier, const std::string& desc) {
            spawnNamedBoss(nm, tier);
            roomDescription_ = desc;
            lastEvent_ = "BOSS! " + nm + " — stand ready!";
            dmSay("Endgame boss: " + nm + " (original). Legendary spoils possible.");
            addJournalEntry("Boss: " + nm + ".");
        };
        if (pick == 0) {
            place("Hollow Crown", 4,
                  "A throne of cracked millstone and bone. The Hollow Crown — an endgame tyrant of ash — rises without a face.");
            return;
        }
        if (pick == 1) {
            place("Ember Hydra", 5,
                  "Multiple ember-lit heads weave between pillars. The Ember Hydra hisses steam and bone-fire.");
            return;
        }
        place("Nightfang Matriarch", 4,
              "Webs thick as rope choke the vault. The Nightfang Matriarch — a vast spider-queen — descends.");
        return;
    }

    if (roomCount_ >= dragonNeed && roll <= dragonChance) {
        spawnNamedBoss("Ashen Drake", 3);
        roomDescription_ = "The chamber opens into a scorched hollow. An Ashen Drake coils around a cracked pillar — heat warps the air.";
        lastEvent_ = "BOSS! Ashen Drake — stand ready!";
        dmSay("Original menace: the Ashen Drake (not from any published module). Stand ready — this is a late-game trial.");
        addJournalEntry("Boss: Ashen Drake stirs in the deep.");
        return;
    }
    if (roomCount_ >= skNeed && roll <= skChance) {
        spawnNamedBoss("Skeleton King", 2);
        roomDescription_ = "Bone thrones and rusted crowns litter the floor. The Skeleton King rises, empty eyes fixed on you.";
        lastEvent_ = "BOSS! Skeleton King — stand ready!";
        dmSay("The Skeleton King claims this ossuary. Original foe — fight smart.");
        addJournalEntry("Boss: Skeleton King.");
        return;
    }
    if (roomCount_ >= gkNeed && roll <= gkChance) {
        spawnGkCourt();
    }
}

void Game::noteBossDefeat(const std::string& foeName) {
    int tier = LootSystem::bossTier(foeName);
    if (tier <= 0) return;
    // #46 base luck: 10+tier*8 — bosses better than trash, not BiS flood
    int xp = 40 * tier + roomCount_ * 2;
    int luck = 10 + tier * 8;
    int gold = 20 * tier + getRandomInt(5, 15);
    if (LootSystem::isEndgameBossName(foeName) || isBossRaid()) {
        xp += 80;
        luck += 18; // still modest; Legendary needs endgameUnlocked + rarityRoll
        gold += 40;
    }
    pendingBossXpBonus_ += xp;
    pendingBossLootLuck_ += luck;
    pendingBossGoldBonus_ += gold;
    dmSay("Boss fallen: " + foeName + "! Greater rewards await.");
    maybeGrantRaidKeyFromBoss(foeName);
}

void Game::maybeGrantRaidKeyFromBoss(const std::string& foeName) {
    if (!endgameContentAllowed()) return;
    int tier = LootSystem::bossTier(foeName);
    if (tier < 3 && !LootSystem::isEndgameBossName(foeName) && !isBossRaid()) return;
    int chance = isBossRaid() ? 55 : (LootSystem::isEndgameBossName(foeName) ? 40 : 22);
    if (getRandomInt(1, 100) <= chance) {
        pendingRaidKeyDrop_ = true;
        dmSay("A Raid Key glints among the spoils — check the menu (max 2 held per calendar day).");
        addJournalEntry("Raid Key recovered from " + foeName + ".");
    }
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
    for (const auto& pl : players_) {
        if (!pl->isDead) { anyLiving = true; break; }
    }
    if (!anyLiving) {
        gameOver_ = true;
        const char* dname = difficultyName(difficulty_);
        lastEvent_ = std::string("Game Over — the entire party has fallen. Difficulty: ") + dname + ".";
        if (static_cast<Difficulty>(difficulty_) == Difficulty::NIGHTMARE) {
            dmSay("Silence falls. Nightmare: this run is over — return to the menu and start a new adventure.");
        } else {
            dmSay(std::string("Silence falls. Difficulty ") + dname +
                  ": choose Continue on the wipe dialog to revive (rules depend on difficulty).");
        }
        addChatMessage("System", "Game Over");
    }
}

void Game::revivePartyForDifficulty() {
    const auto d = static_cast<Difficulty>(difficulty_);
    for (auto& pl : players_) {
        if (!pl) continue;
        if (d == Difficulty::HARD) {
            pl->stripGearAndGoldToStarters();
        }
        int hp;
        if (d == Difficulty::EASY) {
            hp = pl->maxHp;
            pl->resources = pl->maxResources;
        } else if (d == Difficulty::MEDIUM) {
            hp = std::max(1, pl->maxHp / 2);
            pl->resources = std::max(0, pl->maxResources / 2);
        } else {
            hp = std::max(1, pl->maxHp / 2);
            pl->resources = std::max(1, pl->maxResources / 2);
        }
        pl->reviveAfterWipe(hp);
    }
}

void Game::rollbackOneRoomOrBeat() {
    enemies_.clear();
    shopInventory_.clear();
    isMerchantRoom_ = false;
    roomSearchUsed_ = false;

    if (isSoloQuestScripted()) {
        int prev = previousScriptedBeat(questBeat_);
        if (prev != questBeat_) {
            questBeat_ = prev;
            if (roomCount_ > 1) roomCount_--;
        } else if (roomCount_ > 1) {
            roomCount_--;
        }
        applySoloQuestRoom();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        return;
    }

    if (roomCount_ > 1) roomCount_--;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
}

bool Game::recoverFromPartyWipe() {
    if (!gameOver_) return false;
    if (static_cast<Difficulty>(difficulty_) == Difficulty::NIGHTMARE) {
        lastEvent_ = "Nightmare — no Continue. Start a new adventure from the menu.";
        return false;
    }
    gameOver_ = false;
    revivePartyForDifficulty();
    rollbackOneRoomOrBeat();
    const char* dname = difficultyName(difficulty_);
    lastEvent_ = std::string("Continue (") + dname + ") — the party rises one chamber back.";
    dmSay(lastEvent_);
    addChatMessage("System", lastEvent_);
    addJournalEntry(std::string("Party wipe Continue on ") + dname + ".");
    return true;
}

bool Game::isAllyAi(const Character* c) const {
    if (!c) return false;
    // Player-controlled companions (and joined humans) wait for UI input.
    return c->aiControlled;
}

void Game::grantKillLoot(Character* actor, const std::string& foeName) {
    int gold = 8 + (roomCount_ * 3) + getRandomInt(0, 7);
    if (LootSystem::isBossName(foeName)) {
        noteBossDefeat(foeName);
        gold += pendingBossGoldBonus_;
        pendingBossGoldBonus_ = 0;
        gold = static_cast<int>(gold * (1.5 + 0.25 * LootSystem::bossTier(foeName)));
    }
    Character* looter = actor;
    if (!looter || looter->isDead) {
        looter = nullptr;
        for (auto& p : players_) {
            if (p && !p->isDead) { looter = p.get(); break; }
        }
    }
    if (!looter) return;
    looter->gold += gold;
    std::string msg = looter->name + " loots " + std::to_string(gold) + " gold from the " + foeName + ".";
    addChatMessage("Combat", msg);
    dmSay(msg);
    addJournalEntry(msg);
    lastEvent_ = msg;
}

void Game::enterClearedRoom(Character* actor) {
    int xpGained = 50 + (roomCount_ * 15) + pendingBossXpBonus_;
    pendingBossXpBonus_ = 0;
    for (auto& p : players_) {
        if (p->addXp(xpGained)) {
            dmSay(p->name + " levels up! Spend your ability points from the character sheet.");
        }
    }
    dmSay("The party gains " + std::to_string(xpGained) + " XP.");
    int luck = pendingBossLootLuck_;
    pendingBossLootLuck_ = 0;
    if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::LANTERN_VAULT) {
        grantAshenLantern(actor);
    } else if (actor && !actor->isDead) {
        int preferA = -1, preferB = -1;
        partyPreferClasses(preferA, preferB);
        const bool eg = endgameContentAllowed();
        auto loot = LootSystem::generateLoot(roomCount_, luck, preferA, preferB, eg);
        if (loot) {
            actor->addToInventory(loot);
            dmSay(actor->name + " finds " + loot->getDescription() + " [" + loot->rarityLabel() + "] — check Inventory.");
            addJournalEntry(actor->name + " found loot: " + loot->getDescription() + " (" + loot->rarityLabel() + ")");
        } else if (luck > 0) {
            // #46 pity: guarantee a drop without luck=80 Epic flood.
            auto pity = LootSystem::generateLoot(roomCount_, 30, preferA, preferB, eg);
            if (!pity) pity = LootSystem::generateLoot(roomCount_, 60, preferA, preferB, eg);
            if (pity) {
                actor->addToInventory(pity);
                dmSay(actor->name + " claims a boss trophy: " + pity->getDescription() + " [" + pity->rarityLabel() + "].");
                addJournalEntry(actor->name + " claimed boss loot: " + pity->getDescription());
            }
        }
    }
    // Stay in this chamber so Short Rest / one Search / Onward are available.
    turnOrder_.clear();
    currentTurnIndex_ = 0;
    roomSearchUsed_ = false;
    roomDescription_ += " The foes lie still. You may Search, take a Short Rest, or press Onward.";
    lastEvent_ = "Room cleared! Search, Rest, or Onward.";
    addChatMessage("Combat", lastEvent_);
    if (questLanternRecovered_ && static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::LANTERN_VAULT) {
        dmSay("The vault is clear. Carry the Ashen Lantern Onward to the shrine — or Search and Rest first.");
    } else {
        dmSay("The chamber is clear. Search for loot, take a Short Rest, or press Onward.");
    }
}

void Game::purgeDownedEnemies() {
    // Drop 0-HP foes out of the encounter without touching a dangling turnOrder_ pointer.
    bool removed = false;
    Character* looter = getCurrentActor();
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0 || (*it)->isDowned) {
            std::string foeName = (*it)->name;
            grantKillLoot(looter, foeName);
            addJournalEntry("Defeated " + foeName);
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (!removed) return;
    if (enemies_.empty()) {
        enterClearedRoom(looter);
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
    if (gameOver_ || isMerchantRoom_ || !isHost_) return;
    // Cleared chamber: wait for Search / Short Rest / Onward — do not AI-spin allies.
    if (enemies_.empty()) return;
    if (turnOrder_.empty()) return;

    Character* current = turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    if (!current) { advanceTurn(); return; }

    // Dead / zero-HP heroes must never take a normal action turn.
    if (!current->isDead && current->currentHp <= 0) current->isDowned = true;
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


void Game::applyLegendaryOnHit(Character* actor, int damageDealt) {
    if (!actor || damageDealt <= 0 || !actor->equippedWeapon) return;
    auto& w = actor->equippedWeapon;
    if (!w->subEffectUnlocked()) return;
    int heal = 0;
    if (w->subEffect == static_cast<int>(LegendarySubEffect::LIFESTEAL)) {
        heal = 1 + (w->upgradeLevel / 10);
    } else if (w->subEffect == static_cast<int>(LegendarySubEffect::ON_HIT_HEAL)) {
        heal = 2;
    }
    if (heal > 0) {
        actor->heal(heal);
        dmSay(actor->name + "'s Legendary weapon restores " + std::to_string(heal) + " HP.");
    }
}

void Game::tryFightShield(Character* defender, int& incomingDamage) {
    if (!defender || incomingDamage <= 0 || !defender->equippedArmor) return;
    auto& a = defender->equippedArmor;
    if (!a->subEffectUnlocked()) return;
    if (a->subEffect != static_cast<int>(LegendarySubEffect::FIGHT_SHIELD)) return;
    if (defender->fightShieldUsed) return;
    defender->fightShieldUsed = true;
    int absorb = 5 + (a->upgradeLevel / 5);
    int blocked = std::min(incomingDamage, absorb);
    incomingDamage -= blocked;
    dmSay(defender->name + "'s Legendary armor projects a once-per-fight shield (" + std::to_string(blocked) + " absorbed).");
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
        if (!targetIsEnemy) {
            tryFightShield(&target, dmg);
        }
        target.takeDamage(dmg);
        if (targetIsEnemy) {
            applyLegendaryOnHit(actor, dmg);
        }
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
    // Remove dead enemies; pause in a cleared chamber (do not auto-spawn next room).
    // Strip turnOrder_ entries BEFORE destroying the Character (no dangling pointers).
    for (auto it = enemies_.begin(); it != enemies_.end(); ) {
        if ((*it)->currentHp <= 0) {
            std::string foeName = (*it)->name;
            grantKillLoot(actor, foeName);
            addJournalEntry("Defeated " + foeName);
            dmSay("The " + foeName + " falls. The dungeon grows quieter… for now.");
            removeFromTurnOrder(it->get());
            it = enemies_.erase(it);
        } else ++it;
    }

    if (enemies_.empty()) {
        enterClearedRoom(actor);
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
    if (!actor || actor->isDowned || actor->isDead || actor->currentHp <= 0) {
        lastEvent_ = "You're down — you can't use a potion until you're back up.";
        return;
    }
    if (actor->resources <= 0 || targetPlayerIndex < 0 || static_cast<size_t>(targetPlayerIndex) >= players_.size()) {
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
    if (!actor || actor->isDowned || actor->isDead || actor->currentHp <= 0) return;

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
        const bool bossRoom = hasLivingBossEnemy();
        const std::string bossToast = lastEvent_;
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
            lastEvent_ = bossToast;
        } else {
            lastEvent_ = "You bid the merchant farewell and press deeper.";
            dmSay("The merchant nods. \"Luck in the dark, friends.\"");
        }
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
    if (enemies_.empty()) {
        // Remain in the cleared chamber so Search / Onward stay available.
        turnOrder_.clear();
        currentTurnIndex_ = 0;
        return;
    }
    rollInitiative();
}

void Game::playerInteract(const std::string& playerName) {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    Character* hero = findCharacter(playerName);
    if (!hero || hero->isDead || hero->isDowned) return;

    // Search only when the chamber is clear (not mid-fight).
    if (!enemies_.empty()) {
        lastEvent_ = "Too dangerous to Search while foes remain!";
        dmSay("Clear the chamber first — then Search.");
        return;
    }
    if (roomSearchUsed_) {
        lastEvent_ = "You've already searched this room.";
        dmSay("Nothing more turns up here — press Onward when ready.");
        return;
    }
    // One meaningful Search per room (success or fail locks further Search).
    roomSearchUsed_ = true;

    if (trySoloQuestSearch(hero)) {
        // Story Search handled (clue / rune-key / shrine).
    } else if (hero->performSavingThrow(hero->attributes.intelligence, 12)) {
        int found = 15 + roomCount_ * 2 + getRandomInt(0, 10);
        hero->gold += found;
        lastEvent_ = hero->name + " searched and found " + std::to_string(found) + " gold pieces!";
        addChatMessage("Combat", lastEvent_);
        dmSay("A successful Investigation check reveals a hidden pouch.");
        addJournalEntry(lastEvent_);
    } else {
        hero->takeDamage(3);
        int idx = 0;
        for(size_t i=0; i<players_.size(); ++i) if(players_[i].get() == hero) idx = static_cast<int>(i);
        pushVisualEvent(VisualEventType::PLAYER_DAMAGE, idx);
        lastEvent_ = "Fail! A trap hit " + hero->name + " for 3 damage!";
        addChatMessage("Combat", lastEvent_);
        dmSay("A pressure plate clicks — poison darts!");
        if (hero->isDead) {
            removeFromTurnOrder(hero);
            checkPartyDefeat();
            return;
        }
    }
    // Interact spends your turn like Attack/Heal when initiative is active.
    if (!turnOrder_.empty()) {
        advanceTurn();
    }
}

void Game::playerAdvanceFromCleared() {
    if (gameOver_) { lastEvent_ = "Game Over — start a new adventure."; return; }
    if (isMerchantRoom_) {
        lastEvent_ = "Use Leave to depart the merchant.";
        return;
    }
    if (!enemies_.empty()) {
        lastEvent_ = "Can't press Onward — foes remain!";
        dmSay("Steel yourselves — finish the fight first.");
        return;
    }

    // Solo story: Act 1 → Act 2 → Act 3 → procedural endgame.
    if (isSoloQuestScripted()) {
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::CRYPT_DOORS && !questCryptKeyFound_) {
            dmSay("The doors yield grudgingly — you force them without the rune-key. Dust and bone-scent spill out.");
            addJournalEntry("Forced Hollowbarrow's doors without the Ashen Rune-Key.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT2_LOFT && !questAct2LedgerFound_) {
            dmSay("You leave without the ledger — the Collector will still have copies. Steel yourselves.");
            addJournalEntry("Left Ledger Loft without recovering the coerced ledger.");
        }
        if (static_cast<SoloQuestBeat>(questBeat_) == SoloQuestBeat::ACT3_RELIC && !questAct3SealFound_) {
            dmSay("You leave without the Ember Seal — the breach will not close cleanly.");
            addJournalEntry("Left Ember Seal Niche without the seal.");
        }

        if (questBeat_ >= static_cast<int>(SoloQuestBeat::MILLHOLLOW)
            && questBeat_ < static_cast<int>(SoloQuestBeat::RESOLUTION)) {
            questBeat_++;
            roomCount_++;
            roomSearchUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        if (questBeat_ == static_cast<int>(SoloQuestBeat::RESOLUTION)) {
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT2_GREEN);
            roomCount_++;
            roomSearchUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 2: Millhollow's Debt begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 2 begins — Millhollow's Debt on the green.");
            return;
        }
        if (questBeat_ >= static_cast<int>(SoloQuestBeat::ACT2_GREEN)
            && questBeat_ < static_cast<int>(SoloQuestBeat::ACT2_SETTLED)) {
            questBeat_++;
            roomCount_++;
            roomSearchUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        if (questBeat_ == static_cast<int>(SoloQuestBeat::ACT2_SETTLED)) {
            questAct2Complete_ = true;
            questComplete_ = true;
            questBeat_ = static_cast<int>(SoloQuestBeat::ACT3_RUMOR);
            roomCount_++;
            roomSearchUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = "Onward — Act 3: Emberdeep Breach begins.";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("Act 3 begins — Emberdeep Breach at the old well.");
            return;
        }
        if (questBeat_ >= static_cast<int>(SoloQuestBeat::ACT3_RUMOR)
            && questBeat_ < static_cast<int>(SoloQuestBeat::ACT3_SEALED)) {
            questBeat_++;
            roomCount_++;
            roomSearchUsed_ = false;
            applySoloQuestRoom();
            if (!enemies_.empty()) rollInitiative();
            lastEvent_ = std::string("Onward — ") + soloQuestBeatName(questBeat_) + ".";
            addChatMessage("Quest", lastEvent_);
            addJournalEntry("The party advanced to " + std::string(soloQuestBeatName(questBeat_)) + ".");
            return;
        }
        // ACT3_SEALED → post-story endgame procedural
        questBeat_ = static_cast<int>(SoloQuestBeat::POST_QUEST);
        questAct3Complete_ = true;
        questAct2Complete_ = true;
        questComplete_ = true;
        roomCount_++;
        roomSearchUsed_ = false;
        spawnRoomContent();
        const bool bossRoom = hasLivingBossEnemy();
        const std::string bossToast = lastEvent_;
        if (!isMerchantRoom_) {
            if (!bossRoom) generateRoomDescription();
            if (!enemies_.empty()) rollInitiative();
            else { turnOrder_.clear(); currentTurnIndex_ = 0; }
        } else {
            turnOrder_.clear();
            currentTurnIndex_ = 0;
        }
        if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
            lastEvent_ = bossToast;
            addChatMessage("Quest", lastEvent_);
        } else {
            lastEvent_ = "Story complete — endgame rooms await. Room " + std::to_string(roomCount_) + ".";
            addChatMessage("Quest", "Acts 1–3 complete — endgame unlocked.");
            dmSay("Millhollow sleeps. Endgame crawl bosses, Legendary gear, and Boss Raids await the brave.");
        }
        addJournalEntry("Post-story endgame exploration begins (Acts 1–3 done).");
        return;
    }

    // Legacy / mid-save: finished Act 2 into procedural before Act 3 shipped → start Act 3.
    if (!isBossRaid() && !isSoloCrawl()
        && questAct2Complete_ && !questAct3Complete_
        && !isSoloQuestScripted()) {
        questBeat_ = static_cast<int>(SoloQuestBeat::ACT3_RUMOR);
        roomCount_++;
        roomSearchUsed_ = false;
        applySoloQuestRoom();
        if (!enemies_.empty()) rollInitiative();
        lastEvent_ = "Onward — Act 3: Emberdeep Breach begins (story continues).";
        addChatMessage("Quest", lastEvent_);
        addJournalEntry("Act 3 begins from post–Act 2 save.");
        return;
    }

    roomCount_++;
    roomSearchUsed_ = false;
    spawnRoomContent();
    const bool bossRoom = hasLivingBossEnemy();
    const std::string bossToast = lastEvent_;
    if (!isMerchantRoom_) {
        if (!bossRoom) generateRoomDescription();
        if (!enemies_.empty()) rollInitiative();
        else { turnOrder_.clear(); currentTurnIndex_ = 0; }
    } else {
        turnOrder_.clear();
        currentTurnIndex_ = 0;
    }
    if (bossRoom && bossToast.rfind("BOSS!", 0) == 0) {
        lastEvent_ = bossToast;
        addChatMessage("Combat", lastEvent_);
        addJournalEntry("The party advanced to room " + std::to_string(roomCount_) + " (boss).");
    } else {
        lastEvent_ = "You press deeper into the dungeon… Room " + std::to_string(roomCount_) + ".";
        addChatMessage("Combat", lastEvent_);
        dmSay("Onward — a new chamber opens before you.");
        addJournalEntry("The party advanced to room " + std::to_string(roomCount_) + ".");
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
    // During Ashen Lantern (beats 1–5), bias heals earlier so beginners aren't left bloodied.
    const bool earlySoloHeal = isSoloQuestScripted()
        && questBeat_ <= static_cast<int>(SoloQuestBeat::LANTERN_VAULT);
    for (size_t i = 0; i < players_.size(); ++i) {
        const Character* p = players_[i].get();
        if (!p || p->isDead) continue;
        int score = 0;
        if (p->isDowned) {
            score = 200 + p->deathSaveFailures * 30; // revive / stabilize urgency
        } else if (earlySoloHeal && p->maxHp > 0 && p->currentHp * 2 <= p->maxHp) {
            score = 95 + (p->maxHp - p->currentHp); // bloodied — heal sooner on solo quest
        } else if (p->maxHp > 0 && p->currentHp * 3 <= p->maxHp) {
            score = 80 + (p->maxHp - p->currentHp); // critically low
        } else if (p->currentHp <= 4 && p->currentHp < p->maxHp) {
            score = 60;
        } else if (earlySoloHeal && p->currentHp <= 6 && p->currentHp < p->maxHp) {
            score = 55; // soft early-quest cushion
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
            tryFightShield(&hero, dmg);
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

    const bool exploreBeat = isMerchantRoom_ || (enemies_.empty() && roomCount_ >= 1 && !dmOnlyTable_);
    ss << "Room " << roomCount_ << " | Turn: " << (current ? current->name : (exploreBeat ? "Safe" : "None")) << "\n";
    if (isBossRaid()) ss << "Mode: Boss Raid\n";
    else if (isSoloCrawl()) ss << "Mode: Dungeon Crawl\n";
    else if (questAct3Complete_) ss << "Story complete: Acts 1–3 (endgame unlocked)\n";
    else if (isAct3Beat(questBeat_)) ss << "Quest: Emberdeep Breach — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questAct2Complete_ && !isSoloQuestScripted()) ss << "Acts 1–2 done — Act 3 pending or onward\n";
    else if (isAct2Beat(questBeat_)) ss << "Quest: Millhollow's Debt — " << soloQuestBeatName(questBeat_) << "\n";
    else if (questComplete_ && !isSoloQuestScripted()) ss << "Quest complete: Ashen Lantern recovered\n";
    else if (isAct1Beat(questBeat_)) ss << "Quest: Ashen Lantern — " << soloQuestBeatName(questBeat_) << "\n";
    ss << "Difficulty: " << difficultyName(difficulty_) << "\n";
    if (gameOver_) ss << "Game Over\n";
    if (dmOnlyTable_ && !dmName_.empty()) ss << "DM: " << dmName_ << "\n";
    if (Character* companion = findNpcCompanion()) {
        ss << "Companion: " << companion->name
           << (companion->aiControlled ? " [Auto]" : " [Player]") << "\n";
    }
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
       << "," << (dmOnlyTable_ ? 1 : 0) << "," << dmSafe << "," << (roomSearchUsed_ ? 1 : 0)
       << "," << questBeat_ << "," << (questLanternRecovered_ ? 1 : 0) << "," << (questComplete_ ? 1 : 0)
       << "," << (questCryptKeyFound_ ? 1 : 0)
       << "," << (questAct2Complete_ ? 1 : 0) << "," << (questAct2LedgerFound_ ? 1 : 0)
       << "," << soloPlayMode_ << "," << difficulty_
       << "," << (bossSeenGk_ ? 1 : 0) << "," << (bossSeenSk_ ? 1 : 0) << "," << (bossSeenDrake_ ? 1 : 0)
       << "," << (questAct3Complete_ ? 1 : 0) << "," << (questAct3SealFound_ ? 1 : 0)
       << "," << (bossSeenHollow_ ? 1 : 0) << "," << (bossSeenHydra_ ? 1 : 0) << "," << (bossSeenNightfang_ ? 1 : 0) << "|";
    // Section 1: Descriptions
    ss << roomDescription_ << "~" << lastEvent_ << "|";
    // Section 2: Players
    for (const auto& p : players_) {
        ss << p->name << "," << static_cast<int>(p->characterClass) << "," << p->level << "," << p->xp << "," << p->currentHp << "," << p->maxHp << "," << p->resources << "," << p->maxResources << "," << p->pendingStatPoints << "," << p->attributes.strength << "," << p->attributes.dexterity << "," << p->attributes.constitution << "," << p->attributes.intelligence << "," << p->attributes.wisdom << "," << p->attributes.charisma << "," << p->gold << "," << p->initiative << "," << p->uid << ",";
        if (p->equippedWeapon) ss << p->equippedWeapon->toToken(); else ss << "None:0:W:0:-1:0";
        ss << ",";
        if (p->equippedArmor) ss << p->equippedArmor->toToken(); else ss << "None:0:A:0:-1:0";
        ss << "," << (p->isDowned ? 1 : 0) << "," << p->deathSaveSuccesses << "," << p->deathSaveFailures
           << "," << (p->isStable ? 1 : 0) << "," << (p->isDead ? 1 : 0)
           << "," << (p->aiControlled ? 1 : 0);
        // Inventory bag (hero required; companion included): tokens joined by ^
        ss << ",";
        for (size_t ii = 0; ii < p->inventory.size(); ++ii) {
            if (ii) ss << "^";
            if (p->inventory[ii]) ss << p->inventory[ii]->toToken();
        }
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
    pendingBossXpBonus_ = 0;
    pendingBossLootLuck_ = 0;
    pendingBossGoldBonus_ = 0;
    bossSeenGk_ = false;
    bossSeenSk_ = false;
    bossSeenDrake_ = false;
    bossSeenHollow_ = false;
    bossSeenHydra_ = false;
    bossSeenNightfang_ = false;
    pendingRaidKeyDrop_ = false;
    questAct3Complete_ = false;
    questAct3SealFound_ = false;
    shopInventory_.clear();

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
            if (std::getline(ss_sub, val, ',')) roomSearchUsed_ = (val == "1");
            else roomSearchUsed_ = false;
            // Optional quest fields (Ashen Lantern) — backward compatible with older saves.
            if (std::getline(ss_sub, val, ',')) questBeat_ = std::stoi(val);
            else questBeat_ = 0;
            if (std::getline(ss_sub, val, ',')) questLanternRecovered_ = (val == "1");
            else questLanternRecovered_ = false;
            if (std::getline(ss_sub, val, ',')) questComplete_ = (val == "1");
            else questComplete_ = false;
            if (std::getline(ss_sub, val, ',')) questCryptKeyFound_ = (val == "1");
            else questCryptKeyFound_ = false;
            if (std::getline(ss_sub, val, ',')) questAct2Complete_ = (val == "1");
            else questAct2Complete_ = false;
            if (std::getline(ss_sub, val, ',')) questAct2LedgerFound_ = (val == "1");
            else questAct2LedgerFound_ = false;
            if (std::getline(ss_sub, val, ',')) soloPlayMode_ = std::stoi(val);
            else soloPlayMode_ = static_cast<int>(SoloPlayMode::STORY);
            if (std::getline(ss_sub, val, ',')) difficulty_ = std::stoi(val);
            else difficulty_ = static_cast<int>(Difficulty::EASY);
            if (difficulty_ < 0 || difficulty_ > 3) difficulty_ = static_cast<int>(Difficulty::EASY);
            if (std::getline(ss_sub, val, ',')) bossSeenGk_ = (val == "1");
            else bossSeenGk_ = false;
            if (std::getline(ss_sub, val, ',')) bossSeenSk_ = (val == "1");
            else bossSeenSk_ = false;
            if (std::getline(ss_sub, val, ',')) bossSeenDrake_ = (val == "1");
            else bossSeenDrake_ = false;
            // Act 3 / endgame (backward compatible)
            if (std::getline(ss_sub, val, ',')) questAct3Complete_ = (val == "1");
            else questAct3Complete_ = false;
            if (std::getline(ss_sub, val, ',')) questAct3SealFound_ = (val == "1");
            else questAct3SealFound_ = false;
            if (std::getline(ss_sub, val, ',')) bossSeenHollow_ = (val == "1");
            else bossSeenHollow_ = false;
            if (std::getline(ss_sub, val, ',')) bossSeenHydra_ = (val == "1");
            else bossSeenHydra_ = false;
            if (std::getline(ss_sub, val, ',')) bossSeenNightfang_ = (val == "1");
            else bossSeenNightfang_ = false;
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
                auto w = Item::fromToken(item_w);
                if (w) p->equippedWeapon = w;
            }
            if (std::getline(ss_p, item_a, ',')) {
                auto a = Item::fromToken(item_a);
                if (a) p->equippedArmor = a;
            }
            // Optional trailing death-save fields (backward compatible with older saves).
            std::string down_s, dss_s, dsf_s, stab_s, dead_s;
            if (std::getline(ss_p, down_s, ',')) {
                p->isDowned = (down_s == "1") || (p->currentHp <= 0);
                if (std::getline(ss_p, dss_s, ',')) p->deathSaveSuccesses = std::stoi(dss_s);
                if (std::getline(ss_p, dsf_s, ',')) p->deathSaveFailures = std::stoi(dsf_s);
                if (std::getline(ss_p, stab_s, ',')) p->isStable = (stab_s == "1");
                if (std::getline(ss_p, dead_s, ',')) p->isDead = (dead_s == "1");
                std::string ai_s;
                if (std::getline(ss_p, ai_s, ',')) {
                    p->aiControlled = (ai_s == "1");
                } else {
                    // Legacy saves: NPC-named allies were always AI-controlled.
                    p->aiControlled = (p->name.find("(NPC)") != std::string::npos);
                }
                // Optional inventory bag (v2.2+): ^-joined tokens
                std::string inv_s;
                if (std::getline(ss_p, inv_s, ',')) {
                    p->inventory.clear();
                    if (!inv_s.empty()) {
                        std::stringstream invss(inv_s);
                        std::string tok;
                        while (std::getline(invss, tok, '^')) {
                            auto it = Item::fromToken(tok);
                            if (it) p->inventory.push_back(it);
                        }
                    }
                }
            } else {
                p->isDowned = (p->currentHp <= 0);
                p->aiControlled = (p->name.find("(NPC)") != std::string::npos);
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
