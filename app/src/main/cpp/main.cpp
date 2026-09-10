#include <jni.h>
#include <mutex>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivity.h>
#include <unistd.h>
#include <exception>
#include <string>

#include "AndroidOut.h"
#include "Renderer.h"
#include "Game.h"
#include <sstream>

// Persistent game state that survives window destruction
static dnd::Game g_Game;
static struct android_app* g_pApp = nullptr;
static std::mutex g_RendererMutex;

extern "C" {

void handle_cmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            {
                aout << "APP_CMD_INIT_WINDOW" << std::endl;
                std::lock_guard<std::mutex> lock(g_RendererMutex);
                if (!pApp->userData) {
                    // Pass the persistent game reference to the renderer
                    pApp->userData = new Renderer(pApp, g_Game);
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            {
                aout << "APP_CMD_TERM_WINDOW" << std::endl;
                std::lock_guard<std::mutex> lock(g_RendererMutex);
                if (pApp->userData) {
                    auto *pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                    pApp->userData = nullptr;
                    delete pRenderer;
                }
            }
            break;
        default:
            break;
    }
}

bool motion_event_filter_func(const GameActivityMotionEvent *motionEvent) {
    return false; // Allow Android UI to handle touches
}

[[maybe_unused]] void android_main(struct android_app *pApp) {
    g_pApp = pApp;
    pApp->onAppCmd = handle_cmd;
    android_app_set_motion_event_filter(pApp, motion_event_filter_func);

    do {
        bool done = false;
        while (!done) {
            int timeout = 0;
            int events;
            android_poll_source *pSource;
            int result = ALooper_pollOnce(timeout, nullptr, &events,
                                          reinterpret_cast<void**>(&pSource));
            switch (result) {
                case ALOOPER_POLL_TIMEOUT:
                case ALOOPER_POLL_WAKE:
                    done = true;
                    break;
                default:
                    if (pSource) {
                        pSource->process(pApp, pSource);
                    }
            }
        }

        Renderer* pRenderer = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_RendererMutex);
            if (pApp->userData) {
                pRenderer = reinterpret_cast<Renderer *>(pApp->userData);
                pRenderer->handleInput();
                pRenderer->render();
            }
        }

        if (pRenderer) {
            pRenderer->present();
        } else {
            usleep(16000);
        }

    } while (!pApp->destroyRequested);

    g_pApp = nullptr;
}

// --- JNI IMPLEMENTATION ---
// These now use g_Game directly, so they work even if the Renderer is null.

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getPlayerStatus(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getPartyStatus().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getDetailedSheet(JNIEnv *env, jobject thiz, jstring playerName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    std::string nameStr(nativeName);
    env->ReleaseStringUTFChars(playerName, nativeName);

    for (const auto& p : g_Game.getPlayers()) {
        if (p->name == nameStr) {
            return env->NewStringUTF(p->getDetailedSheet().c_str());
        }
    }
    return env->NewStringUTF("Hero not found.");
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getJournal(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getJournal().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getLastEvent(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getLastEvent().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getRoomDescription(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getRoomDescription().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSpecialName(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getSpecialActionName().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getChatHistory(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getChatHistory().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSessionId(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getSessionId().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getShopManifest(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getShopManifest().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isMerchantRoom(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return (jboolean)g_Game.isMerchantRoom();
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isInCombat(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.isInCombat() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isRoomCleared(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.isRoomCleared() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_hasSearchedRoom(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.hasSearchedRoom() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doAdvanceRoom(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.playerAdvanceFromCleared();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doAttack(JNIEnv *env, jobject thiz, jint targetIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.playerAttack(targetIndex);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doHeal(JNIEnv *env, jobject thiz, jint targetIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.playerHeal(targetIndex);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSpecial(JNIEnv *env, jobject thiz, jint targetIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.playerSpecialAction(targetIndex);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doInteract(JNIEnv *env, jobject thiz, jstring playerName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    g_Game.playerInteract(nativeName);
    env->ReleaseStringUTFChars(playerName, nativeName);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doRest(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.playerRest();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doIncreaseStat(JNIEnv *env, jobject thiz, jstring playerName, jint statIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    g_Game.playerIncreaseStat(nativeName, statIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doBuyItem(JNIEnv *env, jobject thiz, jstring playerName, jint itemIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.buyItem(nativeName ? nativeName : "", itemIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getInventoryManifest(JNIEnv *env, jobject thiz, jstring playerName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    std::string out = g_Game.getInventoryManifest(nativeName ? nativeName : "");
    env->ReleaseStringUTFChars(playerName, nativeName);
    return env->NewStringUTF(out.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doEquipItem(JNIEnv *env, jobject thiz, jstring playerName, jint invIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.equipInventoryItem(nativeName ? nativeName : "", invIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUnequipItem(JNIEnv *env, jobject thiz, jstring playerName, jint slot) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.unequipSlot(nativeName ? nativeName : "", slot);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUpgradeInventoryItem(JNIEnv *env, jobject thiz, jstring playerName, jint invIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.upgradeInventoryItem(nativeName ? nativeName : "", invIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doUpgradeEquippedItem(JNIEnv *env, jobject thiz, jstring playerName, jint slot) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.upgradeEquippedItem(nativeName ? nativeName : "", slot);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSellInventoryItem(JNIEnv *env, jobject thiz, jstring playerName, jint invIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.sellInventoryItem(nativeName ? nativeName : "", invIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doSellEquippedItem(JNIEnv *env, jobject thiz, jstring playerName, jint slot) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.sellEquippedItem(nativeName ? nativeName : "", slot);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doTransferInventoryItemToAlly(JNIEnv *env, jobject thiz, jstring playerName, jint invIndex) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.transferInventoryItemToAlly(nativeName ? nativeName : "", invIndex);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_doTransferEquippedItemToAlly(JNIEnv *env, jobject thiz, jstring playerName, jint slot) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    bool ok = g_Game.transferEquippedItemToAlly(nativeName ? nativeName : "", slot);
    env->ReleaseStringUTFChars(playerName, nativeName);
    return ok ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setHost(JNIEnv *env, jobject thiz, jboolean isHost) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.setAsHost(isHost);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_processGameTurn(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.processTurn();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_resetGame(JNIEnv *env, jobject thiz, jint characterClass, jstring playerName, jint soloPlayMode, jint difficulty, jint companionClass, jboolean companionAutoAi) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    g_Game.startNewGame(static_cast<dnd::CharacterClass>(characterClass), nativeName ? nativeName : "Hero",
                        static_cast<int>(soloPlayMode), static_cast<int>(difficulty),
                        static_cast<dnd::CharacterClass>(companionClass),
                        companionAutoAi == JNI_TRUE);
    env->ReleaseStringUTFChars(playerName, nativeName);
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getCompanionName(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getCompanionName().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isCompanionAutoAi(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.isCompanionAutoAi() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setCompanionAutoAi(JNIEnv *env, jobject thiz, jboolean autoAi) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.setCompanionAutoAi(autoAi == JNI_TRUE);
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setCompanionClass(JNIEnv *env, jobject thiz, jint characterClass) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.setCompanionClass(static_cast<dnd::CharacterClass>(characterClass)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_setDifficulty(JNIEnv *env, jobject thiz, jint difficulty) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.setDifficulty(static_cast<int>(difficulty));
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getDifficulty(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return static_cast<jint>(g_Game.getDifficulty());
}

JNIEXPORT jint JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getSoloPlayMode(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return static_cast<jint>(g_Game.getSoloPlayMode());
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isStoryFullyComplete(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.isStoryFullyComplete() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_consumePendingRaidKeyDrop(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.consumePendingRaidKeyDrop() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_beginBossRaidFromCurrent(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.beginBossRaidFromCurrent() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_finishBossRaidKeepParty(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.finishBossRaidKeepParty();
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_recoverFromPartyWipe(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.recoverFromPartyWipe() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_addRemoteAlly(JNIEnv *env, jobject thiz, jint characterClass, jstring playerName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    g_Game.addAlly(nativeName, static_cast<dnd::CharacterClass>(characterClass));
    env->ReleaseStringUTFChars(playerName, nativeName);
}


JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_startDmSession(JNIEnv *env, jobject thiz, jstring dmName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(dmName, nullptr);
    g_Game.startDmSession(nativeName ? nativeName : "Dungeon Master");
    env->ReleaseStringUTFChars(dmName, nativeName);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmBeginDungeon(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.dmBeginDungeon();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmAdvanceRoom(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.dmAdvanceRoom();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmNarrate(JNIEnv *env, jobject thiz, jstring line) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeLine = env->GetStringUTFChars(line, nullptr);
    g_Game.dmNarrate(nativeLine ? nativeLine : "");
    env->ReleaseStringUTFChars(line, nativeLine);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_dmGrantShortRest(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.dmGrantShortRest();
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_prepareClientJoin(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    g_Game.prepareClientJoin();
}

JNIEXPORT jboolean JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_isDmTable(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return g_Game.isDmOnlyTable() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_saveGameState(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.serialize().c_str());
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_loadGameState(JNIEnv *env, jobject thiz, jstring data) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeData = env->GetStringUTFChars(data, nullptr);
    try {
        g_Game.deserialize(nativeData ? nativeData : "");
    } catch (const std::exception& ex) {
        aout << "loadGameState failed: " << ex.what() << std::endl;
    } catch (...) {
        aout << "loadGameState failed: unknown error" << std::endl;
    }
    env->ReleaseStringUTFChars(data, nativeData);
}

JNIEXPORT void JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_sendChatMessage(JNIEnv *env, jobject thiz, jstring sender, jstring message) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeSender = env->GetStringUTFChars(sender, nullptr);
    const char *nativeMsg = env->GetStringUTFChars(message, nullptr);
    g_Game.addChatMessage(nativeSender, nativeMsg);
    env->ReleaseStringUTFChars(sender, nativeSender);
    env->ReleaseStringUTFChars(message, nativeMsg);
}


JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getBattleRoster(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    return env->NewStringUTF(g_Game.getBattleRoster().c_str());
}


JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getXpProgress(JNIEnv *env, jobject thiz, jstring playerName) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    const char *nativeName = env->GetStringUTFChars(playerName, nullptr);
    std::string nameStr(nativeName ? nativeName : "");
    env->ReleaseStringUTFChars(playerName, nativeName);

    for (const auto& p : g_Game.getPlayers()) {
        if (p && p->name == nameStr) {
            // xp,threshold,level,pending
            std::string out = std::to_string(p->xp) + "," +
                std::to_string(p->xpToNextLevel()) + "," +
                std::to_string(p->level) + "," +
                std::to_string(p->pendingStatPoints);
            return env->NewStringUTF(out.c_str());
        }
    }
    return env->NewStringUTF("");
}

JNIEXPORT jstring JNICALL
Java_com_fintrack_dndbeginnerremote_MainActivity_getPartyXpProgress(JNIEnv *env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_RendererMutex);
    std::stringstream ss;
    bool first = true;
    for (const auto& p : g_Game.getPlayers()) {
        if (!p) continue;
        if (!first) ss << ";";
        first = false;
        // name|xp|threshold|level|pending
        ss << p->name << "|" << p->xp << "|" << p->xpToNextLevel()
           << "|" << p->level << "|" << p->pendingStatPoints;
    }
    return env->NewStringUTF(ss.str().c_str());
}

} // extern "C"
