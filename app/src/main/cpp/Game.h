#ifndef DND_GAME_H
#define DND_GAME_H

#include "GameLogic.h"
#include "QuestScript.h"
#include <vector>
#include <string>
#include <queue>
#include <deque>
#include <memory>
#include <random>

namespace dnd {

enum class VisualEventType {
    PLAYER_ATTACK,
    PLAYER_DAMAGE,
    ENEMY_ATTACK,
    ENEMY_DAMAGE
};

struct VisualEvent {
    VisualEventType type;
    int targetIndex;
};

class Game {
public:
    Game();
    void startNewGame(CharacterClass selectedClass = CharacterClass::FIGHTER, const std::string& playerName = "Hero");
    void startDmSession(const std::string& dmName);
    void addAlly(const std::string& name, CharacterClass cl);
    void dmBeginDungeon();
    void dmAdvanceRoom();
    void dmNarrate(const std::string& line);
    void dmGrantShortRest();
    void prepareClientJoin();
    std::string getDmName() const { return dmName_; }
    bool isDmOnlyTable() const { return dmOnlyTable_; }

    // Player Actions
    void playerAttack(int targetEnemyIndex);
    void playerHeal(int targetPlayerIndex);
    void playerSpecialAction(int targetEnemyIndex);
    void playerInteract(const std::string& playerName);
    void playerRest(bool force = false);
    void playerIncreaseStat(const std::string& playerName, int statIndex);
    /** Cleared-room advance (Onward) — not merchant Leave. */
    void playerAdvanceFromCleared();

    // Merchant Actions
    bool buyItem(const std::string& playerName, int itemIndex);
    std::string getShopManifest() const;

    // Turn Management
    void processTurn();
    void spawnRoomContent();
    void generateRoomDescription();
    void rollInitiative();
    void rebuildTurnOrder();

    // Authority (Multiplayer)
    void setAsHost(bool host) { isHost_ = host; }
    bool getIsHost() const { return isHost_; }

    // Social/Chat/Journal
    void addChatMessage(const std::string& sender, const std::string& message);
    std::string getChatHistory() const;
    void addJournalEntry(const std::string& entry);
    std::string getJournal() const;

    // Networking/Persistence
    std::string serialize();
    void deserialize(const std::string& data);

    std::string getLastEvent() const { return lastEvent_; }
    std::string getRoomDescription() const { return roomDescription_; }
    std::string getSessionId() const { return sessionId_; }
    std::string getPartyStatus() const;
    std::string getBattleRoster() const;
    std::string getSpecialActionName() const;

    bool isGameOver() const { return gameOver_; }
    bool isMerchantRoom() const { return isMerchantRoom_; }
    bool isInCombat() const { return !isMerchantRoom_ && !enemies_.empty(); }
    /** No living foes, still in a dungeon chamber (Search/Rest/Onward beat). */
    bool isRoomCleared() const {
        return !gameOver_ && !isMerchantRoom_ && !dmOnlyTable_ && roomCount_ >= 1 && enemies_.empty();
    }
    bool hasSearchedRoom() const { return roomSearchUsed_; }
    int getRoomCount() const { return roomCount_; }
    int getQuestBeat() const { return questBeat_; }
    bool isQuestLanternRecovered() const { return questLanternRecovered_; }
    bool isQuestComplete() const { return questComplete_; }
    /** Solo scripted Ashen Lantern beats (1–6); false for Host-as-DM / online lobby. */
    bool isSoloQuestScripted() const {
        return questBeat_ >= static_cast<int>(SoloQuestBeat::MILLHOLLOW)
            && questBeat_ <= static_cast<int>(SoloQuestBeat::RESOLUTION);
    }

    const std::vector<std::unique_ptr<Character>>& getPlayers() const { return players_; }
    const std::vector<std::unique_ptr<Character>>& getEnemies() const { return enemies_; }

    Character* getCurrentActor() const {
        if (turnOrder_.empty() || currentTurnIndex_ < 0 || static_cast<size_t>(currentTurnIndex_) >= turnOrder_.size()) return nullptr;
        return turnOrder_[static_cast<size_t>(currentTurnIndex_)];
    }

    bool hasVisualEvent() const { return !visualEvents_.empty(); }
    VisualEvent popVisualEvent() {
        VisualEvent e = visualEvents_.front();
        visualEvents_.pop();
        return e;
    }

    // Random helper
    int getRandomInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng_);
    }

private:
    std::vector<std::unique_ptr<Character>> players_;
    std::vector<std::unique_ptr<Character>> enemies_;
    std::vector<Character*> turnOrder_;
    int currentTurnIndex_;

    std::string lastEvent_;
    std::string roomDescription_;
    std::string sessionId_;

    std::deque<std::string> chatHistory_;
    std::deque<std::string> journalEntries_;
    std::queue<VisualEvent> visualEvents_;

    std::vector<std::shared_ptr<Item>> shopInventory_;

    int turnCounter_;
    int roomCount_;
    bool gameOver_;
    bool isHost_ = false;
    bool isMerchantRoom_ = false;
    bool dmOnlyTable_ = false;
    bool roomSearchUsed_ = false; // one Search attempt per chamber (anti-exploit)
    // Solo Ashen Lantern quest (original; SRD-compatible monsters only). Online DM path leaves beat at NONE.
    int questBeat_ = 0;
    bool questLanternRecovered_ = false;
    bool questComplete_ = false;
    bool questCryptKeyFound_ = false;
    std::string dmName_;

    std::mt19937 rng_;

    void enemyTurn();
    void allyTurn();
    bool isAllyAi(const Character* c) const;
    // Lightweight combat AI helpers (solo + host-side only).
    int scoreHeroThreat(const Character& hero, bool partyHasDowned) const;
    int pickEnemyAiTarget() const;
    int pickAllyAiEnemyTarget() const;
    int pickAllyAiHealTarget() const;
    bool allySpecialBeatsBasic(const Character& actor, const Character& enemy) const;
    void removeFromTurnOrder(Character* c);
    void checkPartyDefeat();
    void advanceTurn();
    void purgeDownedEnemies();
    void enterClearedRoom(Character* actor);
    void grantKillLoot(Character* actor, const std::string& foeName);
    void dmSay(const std::string& line);
    void resetSoloQuestState();
    void applySoloQuestRoom();
    void spawnSoloQuestEnemies();
    void grantAshenLantern(Character* actor);
    void maybeFinishQuestOnResolutionEnter();
    bool trySoloQuestSearch(Character* hero);
    int proficiencyBonus() const;
    void resolveEnemyDefeated(Character* actor, int targetEnemyIndex);
    bool performWeaponAttack(Character* actor, Character& target, int attackerVisualIndex, bool targetIsEnemy, int targetIndex, bool sneakAttack);
    void pushVisualEvent(VisualEventType type, int index) { visualEvents_.push({type, index}); }
    Character* findCharacter(const std::string& name);
};

} // namespace dnd

#endif // DND_GAME_H
