package com.fintrack.dndbeginnerremote

import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import android.graphics.Color
import android.view.Gravity
import android.widget.ImageView
import android.widget.ProgressBar
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.animation.AlphaAnimation
import android.view.animation.Animation
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.google.firebase.FirebaseApp

class MainActivity : AppCompatActivity() {
    private val TAG = "DnDMain"

    private lateinit var statusText: TextView
    private lateinit var turnBanner: TextView
    private lateinit var logText: TextView
    private lateinit var roomDescText: TextView
    private lateinit var chatInput: EditText
    private lateinit var btnSendChat: Button
    private lateinit var btnAttack: Button
    private lateinit var btnSpecial: Button
    private lateinit var btnHeal: Button
    private lateinit var btnInteract: Button
    private lateinit var btnRest: Button
    private lateinit var btnReset: Button
    private lateinit var btnSheet: Button
    private lateinit var btnJournal: Button
    private lateinit var btnHelp: Button
    private lateinit var btnInGameSettings: Button
    private lateinit var partyColumn: LinearLayout
    private lateinit var enemyColumn: LinearLayout
    
    private var multiplayer: MultiplayerManager? = null
    private var lastBattleRoster = ""
    private var lastAnimEvent = ""
    private var selectedEnemyName: String? = null
    private var selectedAllyName: String? = null
    private var foeTargetingMode = false
    private var allyTargetingMode = false
    private var lastSelectionRoomKey = ""
    private val combatFeed = ArrayDeque<String>()
    private var diceOverlayRoot: View? = null
    private var diceTitle: TextView? = null
    private var diceD20: TextView? = null
    private var diceDamage: TextView? = null
    private var diceSummary: TextView? = null
    private var diceHideRunnable: Runnable? = null
    private lateinit var mainMenuRoot: View
    private lateinit var btnMenuContinue: Button
    private lateinit var btnMenuSolo: Button
    private lateinit var btnMenuHost: Button
    private lateinit var btnMenuJoin: Button
    private lateinit var btnMenuSettings: Button
    private lateinit var btnMenuQuit: Button
    private lateinit var mainMenuSubtitle: TextView
    private lateinit var settingsRoot: View
    private lateinit var chkBeginnerTips: CheckBox
    private lateinit var chkMusic: CheckBox
    private lateinit var chkSfx: CheckBox
    private lateinit var chkDmVoice: CheckBox
    private lateinit var btnSettingsTutorial: Button
    private lateinit var btnSettingsAbandon: Button
    private lateinit var btnSettingsClose: Button
    private lateinit var settingsAboutText: TextView
    // Audio prefs + GameAudio (BGM / SFX / optional on-device TTS).
    private var musicEnabled = true
    private var sfxEnabled = true
    private var dmVoiceEnabled = false
    private var gameAudio: GameAudio? = null
    private var lastCombatMusicState: Boolean? = null
    private val handler = Handler(Looper.getMainLooper())
    private var uiLoopStarted = false
    private val uiTick = object : Runnable {
        override fun run() {
            if (!sessionActive || !nativeReady) {
                handler.postDelayed(this, 500)
                return
            }
            try {
                processGameTurn()
                updateUi()
            } catch (e: Exception) {
                Log.e(TAG, "ui tick failed", e)
            }
            handler.postDelayed(this, 300)
        }
    }
    private var lastProcessedEvent = ""
    private var lastRoomDesc = ""
    private var lastChatHistory = ""
    private var isUpdatingFromRemote = false
    private var localPlayerName = "Hero"
    private var localClassId = 0
    private var lastCoachTip = ""
    private var lastCoachTurnKey = ""
    private var soloCoachEnabled = true
    /** True after Continue / New Game / Join — avoids wiping save in onPause before that. */
    private var sessionActive = false
    /** True when this device is the online DM (authoritative). */
    private var isOnlineHost = false
    /** True when joined someone else's session. */
    private var isOnlineClient = false
    private var onlineSessionId = ""
    private var remoteDmName = ""

    companion object {
        private const val NATIVE_LIB = "dndbeginnerremote"
        /** False if the C++ library failed to load (common when NDK/CMake build failed). */
        @JvmStatic var nativeReady: Boolean = false
            private set

        init {
            try {
                System.loadLibrary(NATIVE_LIB)
                nativeReady = true
            } catch (e: UnsatisfiedLinkError) {
                nativeReady = false
                Log.e("DnDMain", "Failed to load native library '$NATIVE_LIB'. Rebuild with NDK installed.", e)
            }
        }
    }

    // JNI Methods
    external fun getPlayerStatus(): String
    external fun getBattleRoster(): String
    external fun getDetailedSheet(playerName: String): String
    external fun getJournal(): String
    external fun getLastEvent(): String
    external fun getRoomDescription(): String
    external fun getSpecialName(): String
    external fun getChatHistory(): String
    external fun getSessionId(): String
    external fun getShopManifest(): String
    external fun isMerchantRoom(): Boolean
    external fun doAttack(targetIndex: Int)
    external fun doHeal(targetIndex: Int)
    external fun doSpecial(targetIndex: Int)
    external fun doInteract(playerName: String)
    external fun doRest()
    external fun doIncreaseStat(playerName: String, statIndex: Int)
    external fun doBuyItem(playerName: String, itemIndex: Int): Boolean
    external fun isInCombat(): Boolean
    external fun isRoomCleared(): Boolean
    external fun hasSearchedRoom(): Boolean
    external fun doAdvanceRoom()
    external fun setHost(isHost: Boolean)
    external fun addRemoteAlly(characterClass: Int, playerName: String)
    external fun sendChatMessage(sender: String, message: String)
    external fun processGameTurn()
    external fun resetGame(characterClass: Int, playerName: String)
    external fun startDmSession(dmName: String)
    external fun dmBeginDungeon()
    external fun dmAdvanceRoom()
    external fun dmNarrate(line: String)
    external fun dmGrantShortRest()
    external fun isDmTable(): Boolean
    external fun prepareClientJoin()
    external fun saveGameState(): String
    external fun loadGameState(data: String)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(TAG, "onCreate started")

        
        try {
            FirebaseApp.initializeApp(this)
        } catch (e: Exception) {
            Log.e(TAG, "Firebase init failed: ${e.message}")
        }

        setContentView(R.layout.activity_main)

        if (!nativeReady) {
            Toast.makeText(
                this,
                "Native game library failed to load. In Android Studio: Tools → SDK Manager → SDK Tools → enable NDK and CMake, then Build → Rebuild Project.",
                Toast.LENGTH_LONG
            ).show()
        }
        
        statusText = findViewById(R.id.playerStatusText)
        turnBanner = findViewById(R.id.turnBanner)
        logText = findViewById(R.id.combatLogText)
        roomDescText = findViewById(R.id.roomDescText)
        chatInput = findViewById(R.id.chatInput)
        btnSendChat = findViewById(R.id.btnSendChat)
        btnAttack = findViewById(R.id.btnAttack)
        btnSpecial = findViewById(R.id.btnSpecial)
        btnHeal = findViewById(R.id.btnHeal)
        btnInteract = findViewById(R.id.btnInteract)
        btnRest = findViewById(R.id.btnRest)
        btnReset = findViewById(R.id.btnReset)
        btnSheet = findViewById(R.id.btnSheet)
        btnJournal = findViewById(R.id.btnJournal)
        btnHelp = findViewById(R.id.btnHelp)
        btnInGameSettings = findViewById(R.id.btnInGameSettings)
        partyColumn = findViewById(R.id.partyColumn)
        enemyColumn = findViewById(R.id.enemyColumn)
        wireDiceOverlay()
        wireMainMenuAndSettings()

        btnAttack.setOnClickListener { 
            Log.d(TAG, "Attack clicked")
            if (isMerchantRoom()) {
                showShopDialog()
                return@setOnClickListener
            }
            val cleared = try { isRoomCleared() } catch (_: Exception) { false }
            if (cleared) {
                performOrQueue("advance", 0) { doAdvanceRoom() }
                return@setOnClickListener
            }
            resolveTargetedAction(needEnemy = true, actionType = "attack") { i ->
                doAttack(i); animateAttack(true, targetEnemyIndex = i); scheduleDiceFromLastEvent()
            }
        }
        
        btnSpecial.setOnClickListener {
            Log.d(TAG, "Special clicked")
            if (isMerchantRoom()) {
                val blurb = try { getRoomDescription() } catch (_: Exception) { "A merchant greets you." }
                Toast.makeText(this, blurb, Toast.LENGTH_LONG).show()
                return@setOnClickListener
            }
            // Cleric Healing Word picks ally internally; still allow foe targeting for other classes.
            val special = try { getSpecialName() } catch (_: Exception) { "" }
            if (special.contains("Healing", ignoreCase = true)) {
                performOrQueue("special", 0) {
                    doSpecial(0); scheduleDiceFromLastEvent()
                }
            } else {
                resolveTargetedAction(needEnemy = true, actionType = "special") { i ->
                    doSpecial(i); animateAttack(true, targetEnemyIndex = i); scheduleDiceFromLastEvent()
                }
            }
        }
        btnHeal.setOnClickListener {
            Log.d(TAG, "Heal clicked")
            resolveTargetedAction(needEnemy = false, actionType = "heal") { i ->
                doHeal(i); scheduleDiceFromLastEvent()
            }
        }
        btnInteract.setOnClickListener {
            Log.d(TAG, "Interact clicked")
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            if (inCombat) {
                Toast.makeText(this, "Clear the room before Searching.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val searched = try { hasSearchedRoom() } catch (_: Exception) { false }
            if (searched) {
                Toast.makeText(this, "Already searched this room.", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            performOrQueue("interact", 0) { doInteract(localPlayerName) }
        }
        btnRest.setOnClickListener {
            Log.d(TAG, "Rest clicked")
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            if (inCombat && !isMerchantRoom()) {
                Toast.makeText(this, "Can't Short Rest in the middle of a fight!", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            performOrQueue("rest", 0) { doRest() }
        }

        btnSendChat.setOnClickListener {
            val msg = chatInput.text.toString()
            if (msg.isNotEmpty()) {
                val formattedMsg = "[$localPlayerName]: $msg"
                sendChatMessage(localPlayerName, formattedMsg)
                multiplayer?.sendChat(formattedMsg)
                chatInput.text.clear()
            }
        }

        btnSheet.setOnClickListener {
            if (isOnlineHost && (try { isDmTable() } catch (_: Exception) { true })) showDmToolsDialog()
            else showCharacterSheet()
        }
        btnJournal.setOnClickListener { showJournal() }
        btnHelp.setOnClickListener { showHelpMenu() }
        btnInGameSettings.setOnClickListener {
            try { gameAudio?.playUiClick() } catch (_: Exception) {}
            showSettingsOverlay()
        }
        btnReset.setOnClickListener { resetToStartMenu() }
        btnReset.text = "Menu"

        findViewById<View>(R.id.topBar).setOnLongClickListener {
            val sid = getSessionId()
            val clip = android.content.ClipData.newPlainText("DND_SESSION", sid)
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
            Toast.makeText(this, "Session ID $sid Copied!", Toast.LENGTH_SHORT).show()
            true
        }

        soloCoachEnabled = prefs().getBoolean("solo_coach_enabled", true)
        musicEnabled = prefs().getBoolean("pref_music_enabled", true)
        sfxEnabled = prefs().getBoolean("pref_sfx_enabled", true)
        dmVoiceEnabled = prefs().getBoolean("pref_dm_voice_enabled", false)
        gameAudio = GameAudio(this).also {
            it.start(musicEnabled, sfxEnabled, dmVoiceEnabled)
        }

        // Only wipe when a prior *active* session exited uncleanly (crash_guard left true).
        // Do NOT arm crash_guard merely because the start dialog is shown.
        if (prefs().getBoolean("crash_guard", false)) {
            Log.w(TAG, "Previous session did not exit cleanly — clearing save_state")
            prefs().edit()
                .remove("save_state")
                .putBoolean("crash_guard", false)
                .remove("was_online_host")
                .remove("was_online_client")
                .remove("online_session_id")
                .apply()
            Toast.makeText(this, "Cleared a broken save from a previous crash.", Toast.LENGTH_LONG).show()
        }

        showStartDialog()
        startUiUpdateLoop()
    }

    override fun onDestroy() {
        handler.removeCallbacks(uiTick)
        uiLoopStarted = false
        gameAudio?.release()
        gameAudio = null
        super.onDestroy()
    }

    override fun onPause() {
        super.onPause()
        gameAudio?.onPause()
        // Flush save before process death so progress survives relaunch.
        // Skip until a session is active so the start dialog can't overwrite a good save with an empty game.
        if (!nativeReady || !sessionActive) return
        try {
            val data = saveGameState()
            if (data.isNotBlank() && data.contains("|") && !saveIsGameOver(data)) {
                prefs().edit()
                    .putString("save_state", data)
                    .putString("hero_name", localPlayerName)
                    .putInt("hero_class", localClassId)
                    .putBoolean("crash_guard", false) // good save written — not an unclean exit
                    .putBoolean("was_online_host", isOnlineHost)
                    .putBoolean("was_online_client", isOnlineClient)
                    .putString("online_session_id", onlineSessionId)
                    .commit()
            }
        } catch (e: Exception) {
            Log.e(TAG, "onPause save failed", e)
        }
    }

    override fun onResume() {
        super.onResume()
        gameAudio?.onResume()
    }

    /** Detach Firebase listeners and clear online flags before mode switches / start menu. */
    private fun detachOnlineSession() {
        try { multiplayer?.detach() } catch (e: Exception) { Log.w(TAG, "detach failed", e) }
        multiplayer = null
        isOnlineHost = false
        isOnlineClient = false
        onlineSessionId = ""
        remoteDmName = ""
        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
        joinTimeoutRunnable = null
    }

    private var joinTimeoutRunnable: Runnable? = null

    private fun resetToStartMenu() {
        sessionActive = false
        detachOnlineSession()
        prefs().edit().putBoolean("crash_guard", false).apply()
        showStartDialog()
    }

    private fun activateSession() {
        sessionActive = true
        hideMainMenu()
        hideSettingsOverlay()
        // Arm crash guard only while a real session is running.
        prefs().edit().putBoolean("crash_guard", true).apply()
    }

    private fun currentActorName(): String {
        return try {
            val status = getPlayerStatus()
            val turnLine = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: return ""
            turnLine.substringAfter("Turn:").trim().substringBefore("|").trim()
        } catch (_: Exception) {
            ""
        }
    }

    /** Host-side: reject actions that aren't from the current actor. */
    private fun validateActorOrReject(player: String): Boolean {
        val actor = currentActorName()
        if (actor.isBlank() || actor.equals("Safe", ignoreCase = true) || actor.equals("None", ignoreCase = true)) {
            return true // merchant / free moment
        }
        if (actor.equals(player, ignoreCase = true)) return true
        Toast.makeText(this, "Not $player's turn (it's $actor).", Toast.LENGTH_SHORT).show()
        try { sendChatMessage("System", "Rejected $player's action — current turn is $actor.") } catch (_: Exception) {}
        return false
    }

    private fun performOrQueue(actionType: String, targetIndex: Int, localApply: () -> Unit) {
        try {
            val st = getPlayerStatus()
            if (st.contains("Game Over", ignoreCase = true)) {
                Toast.makeText(this, "Game Over — start a new adventure.", Toast.LENGTH_SHORT).show()
                btnReset.visibility = View.VISIBLE
                clearSave("blocked action after death")
                return
            }
            val localDown = st.lineSequence().any {
                it.contains(localPlayerName) && (
                    it.contains("[DOWN]", ignoreCase = true) ||
                    it.contains("[DEAD]", ignoreCase = true) ||
                    it.contains("[STABLE]", ignoreCase = true)
                )
            }
            // Downed/dead heroes never attack/search/heal; party Rest/Onward after a clear may continue.
            val clearedNow = try { isRoomCleared() } catch (_: Exception) { false }
            if (localDown) {
                val partyContinue = clearedNow && actionType in setOf("rest", "advance")
                if (!partyContinue && actionType in setOf("attack", "special", "heal", "interact", "rest", "advance")) {
                    Toast.makeText(this, "You're down — wait for a heal or Game Over.", Toast.LENGTH_SHORT).show()
                    return
                }
            }
        } catch (_: Exception) {}
        if (isOnlineClient) {
            val mp = multiplayer
            if (mp == null || !mp.isAvailable) {
                Toast.makeText(this, "Not connected to the DM session.", Toast.LENGTH_SHORT).show()
                return
            }
            mp.pushAction(
                mapOf(
                    "type" to actionType,
                    "targetIndex" to targetIndex,
                    "statIndex" to targetIndex,
                    "playerName" to localPlayerName,
                    "classId" to localClassId
                )
            )
            Toast.makeText(this, "Sent to the DM…", Toast.LENGTH_SHORT).show()
            return
        }
        localApply()
        syncAndSave()
    }

    private fun livingUnits(isEnemy: Boolean): List<Pair<Int, BattleUnit>> {
        val (party, foes) = parseBattleRoster(try { getBattleRoster() } catch (_: Exception) { "" })
        val list = if (isEnemy) foes else party
        return list.mapIndexedNotNull { idx, u ->
            // Foes: only living. Allies: include 0-HP downed so Potion/heal can target them.
            if (isEnemy && u.hp <= 0) null else idx to u
        }
    }

    private fun resolveSelectedIndex(needEnemy: Boolean): Int? {
        if (needEnemy) {
            val name = selectedEnemyName ?: return null
            return livingUnits(true).firstOrNull { it.second.name == name }?.first
        }
        val name = selectedAllyName
        if (name != null) {
            livingUnits(false).firstOrNull { it.second.name == name }?.first?.let { return it }
        }
        // Potion defaults to self
        return livingUnits(false).firstOrNull { it.second.name.equals(localPlayerName, true) }?.first
            ?: livingUnits(false).firstOrNull()?.first
    }

    private fun clearCombatSelection(reason: String? = null) {
        selectedEnemyName = null
        selectedAllyName = null
        foeTargetingMode = false
        allyTargetingMode = false
        if (reason != null) Log.d(TAG, "Cleared selection: $reason")
        applySelectionHighlights()
        stopTargetPulse()
    }

    private fun resolveTargetedAction(needEnemy: Boolean, actionType: String, action: (Int) -> Unit) {
        val idx = resolveSelectedIndex(needEnemy)
        if (idx != null) {
            Log.d(TAG, "Using selected index $idx for $actionType")
            if (needEnemy) foeTargetingMode = false else allyTargetingMode = false
            stopTargetPulse()
            performOrQueue(actionType, idx) {
                action(idx)
                // Clear selection after the action resolves
                if (needEnemy) selectedEnemyName = null else selectedAllyName = null
                applySelectionHighlights()
            }
            return
        }
        if (needEnemy) {
            val living = livingUnits(true)
            if (living.isEmpty()) {
                Toast.makeText(this, "No living foes.", Toast.LENGTH_SHORT).show()
                return
            }
            foeTargetingMode = true
            allyTargetingMode = false
            startTargetPulse(enemyColumn)
            Toast.makeText(this, "Tap a foe", Toast.LENGTH_SHORT).show()
            applySelectionHighlights()
        } else {
            val living = livingUnits(false)
            if (living.isEmpty()) {
                Toast.makeText(this, "No living allies.", Toast.LENGTH_SHORT).show()
                return
            }
            allyTargetingMode = true
            foeTargetingMode = false
            startTargetPulse(partyColumn)
            Toast.makeText(this, "Tap an ally (or yourself)", Toast.LENGTH_SHORT).show()
            applySelectionHighlights()
        }
    }

    /** Legacy AlertDialog picker retained only as fallback (unused by Attack/Special/Potion). */
    private fun handleActionWithTarget(
        title: String,
        isEnemy: Boolean = true,
        actionType: String,
        action: (Int) -> Unit
    ) {
        resolveTargetedAction(needEnemy = isEnemy, actionType = actionType, action = action)
    }

    private fun firebaseReady(): Boolean {
        return try {
            if (com.google.firebase.FirebaseApp.getApps(this).isEmpty()) {
                FirebaseApp.initializeApp(this)
            }
            val probe = MultiplayerManager("PROBE")
            val ok = probe.isAvailable
            probe.detach()
            ok
        } catch (e: Exception) {
            Log.w(TAG, "firebaseReady failed", e)
            false
        }
    }

    private fun showFirebaseRequiredDialog(then: (() -> Unit)? = null) {
        AlertDialog.Builder(this)
            .setTitle("Firebase needed for online play")
            .setMessage(
                "Host / Join need Firebase Realtime Database.\n\n" +
                    "1. Place app/google-services.json from the Firebase console (required locally)\n" +
                    "2. The Google Services plugin is already enabled in app/build.gradle.kts\n" +
                    "3. Enable Realtime Database (see FIREBASE_SETUP.md)\n" +
                    "4. Clean + Rebuild\n\n" +
                    "Host: enter your DM name only — friends Join and pick a class.\n" +
                    "Solo adventure works without Firebase."
            )
            .setPositiveButton("OK") { _, _ -> then?.invoke() }
            .setCancelable(false)
            .show()
    }

    private fun setupMultiplayer(id: String, asHost: Boolean) {
        Log.d(TAG, "Setting up multiplayer session=$id asHost=$asHost")
        multiplayer?.detach()
        val mp = MultiplayerManager(id)
        multiplayer = mp
        onlineSessionId = id
        isOnlineHost = asHost
        isOnlineClient = !asHost

        if (!mp.isAvailable) {
            Toast.makeText(this, "Firebase unavailable — online session not connected.", Toast.LENGTH_LONG).show()
            isOnlineHost = false
            isOnlineClient = false
            multiplayer = null
            return
        }

        if (asHost) {
            mp.publishMeta(localPlayerName, role = "dm")
            mp.listenForLobby { name, classId ->
                runOnUiThread {
                    try {
                        admitPlayer(name, classId)
                        syncAndSave()
                    } catch (e: Exception) {
                        Log.e(TAG, "lobby admit failed", e)
                    }
                }
            }
            mp.listenForActions { actionId, action ->
                runOnUiThread {
                    try {
                        applyHostAction(action)
                        mp.ackAction(actionId)
                        syncAndSave()
                    } catch (e: Exception) {
                        Log.e(TAG, "host action failed", e)
                    }
                }
            }
        } else {
            mp.listenForMeta { name ->
                runOnUiThread {
                    remoteDmName = name
                    updateUi()
                }
            }
        }

        mp.listenForUpdates { data ->
            runOnUiThread {
                if (data.isBlank() || isUpdatingFromRemote) return@runOnUiThread
                // Host ignores echoes of its own publishes
                if (isOnlineHost) {
                    try {
                        if (data == saveGameState()) return@runOnUiThread
                    } catch (_: Exception) {}
                }
                try {
                    isUpdatingFromRemote = true
                    loadGameState(data)
                    lastBattleRoster = ""
                    lastRoomDesc = ""
                    lastChatHistory = ""
                    // Successful sync from DM — cancel join wait.
                    if (isOnlineClient) {
                        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
                        joinTimeoutRunnable = null
                        prefs().edit()
                            .putBoolean("was_online_client", true)
                            .putBoolean("was_online_host", false)
                            .putString("online_session_id", onlineSessionId)
                            .putBoolean("crash_guard", false)
                            .apply()
                    }
                    refreshBattleArena()
                    updateUi()
                } catch (e: Exception) {
                    Log.e(TAG, "remote state apply failed", e)
                } finally {
                    isUpdatingFromRemote = false
                }
            }
        }

        mp.listenForChat { msg ->
            runOnUiThread {
                try {
                    if (!getChatHistory().contains(msg)) sendChatMessage("Remote", msg)
                } catch (e: Exception) {
                    Log.w(TAG, "remote chat failed", e)
                }
            }
        }
    }

    private fun partyHasPlayer(name: String): Boolean {
        return try {
            val partySide = getBattleRoster().substringBefore('|')
            partySide.split(';').any { entry ->
                entry.split(',').getOrNull(1)?.equals(name, ignoreCase = true) == true
            }
        } catch (_: Exception) {
            getPlayerStatus().lineSequence().any {
                it.startsWith("$name |") || it.startsWith("$name|")
            }
        }
    }

    private fun admitPlayer(player: String, classId: Int) {
        if (partyHasPlayer(player)) return
        addRemoteAlly(classId, player)
        try { sendChatMessage("DM", "$player has joined the party.") } catch (_: Exception) {}
        Toast.makeText(this, "$player joined the table.", Toast.LENGTH_SHORT).show()
        lastBattleRoster = ""
        refreshBattleArena()
    }

    private fun applyHostAction(action: Map<String, Any>) {
        val type = action["type"]?.toString() ?: return
        val player = action["playerName"]?.toString() ?: "Hero"
        val target = (action["targetIndex"] as? Number)?.toInt()
            ?: action["targetIndex"]?.toString()?.toIntOrNull()
            ?: 0
        val classId = (action["classId"] as? Number)?.toInt()
            ?: action["classId"]?.toString()?.toIntOrNull()
            ?: 0
        val statIndex = (action["statIndex"] as? Number)?.toInt()
            ?: action["statIndex"]?.toString()?.toIntOrNull()
            ?: target

        when (type) {
            "join" -> admitPlayer(player, classId)
            "attack" -> {
                if (!validateActorOrReject(player)) return
                doAttack(target)
                animateAttack(true, targetEnemyIndex = target)
                scheduleDiceFromLastEvent()
            }
            "special" -> {
                if (!validateActorOrReject(player)) return
                doSpecial(target)
                animateAttack(true, targetEnemyIndex = target)
                scheduleDiceFromLastEvent()
            }
            "heal" -> {
                if (!validateActorOrReject(player)) return
                doHeal(target)
            }
            "rest" -> {
                if (!validateActorOrReject(player)) return
                val inCombat = try { isInCombat() } catch (_: Exception) { false }
                if (inCombat) {
                    Toast.makeText(this, "Can't Short Rest in combat.", Toast.LENGTH_SHORT).show()
                    return
                }
                doRest()
            }
            "interact" -> {
                if (!validateActorOrReject(player)) return
                val inCombat = try { isInCombat() } catch (_: Exception) { false }
                if (inCombat) {
                    Toast.makeText(this, "Clear the room before Searching.", Toast.LENGTH_SHORT).show()
                    return
                }
                doInteract(player)
            }
            "advance" -> {
                if (!validateActorOrReject(player)) return
                doAdvanceRoom()
            }
            "buy" -> doBuyItem(player, target)
            "levelup", "increaseStat" -> doIncreaseStat(player, statIndex)
            else -> Log.w(TAG, "Unknown action type: $type")
        }
    }

    private fun syncAndSave() {
        if (!nativeReady) return
        try {
            val data = saveGameState()
            if (saveIsGameOver(data)) {
                clearSave("defeat during sync")
            } else {
                prefs().edit()
                    .putString("save_state", data)
                    .putString("hero_name", localPlayerName)
                    .putInt("hero_class", localClassId)
                    .putBoolean("crash_guard", false)
                    .putBoolean("was_online_host", isOnlineHost)
                    .putBoolean("was_online_client", isOnlineClient)
                    .putString("online_session_id", onlineSessionId)
                    .apply()
            }
            // Only the DM/host publishes authoritative state
            if (!isUpdatingFromRemote && isOnlineHost) {
                try { multiplayer?.updateState(data) } catch (e: Exception) {
                    Log.w(TAG, "multiplayer update failed", e)
                }
            }
            lastBattleRoster = ""
            refreshBattleArena()
        } catch (e: Exception) {
            Log.e(TAG, "syncAndSave failed", e)
        }
    }

    private fun savedState(): String? {
        val data = prefs().getString("save_state", null)
        return data?.takeIf { it.isNotBlank() && it.contains("|") }
    }

    /** Save header: sessionId,roomCount,gameOver,turn,merchant|... */
    private fun saveIsGameOver(data: String): Boolean {
        val bits = data.substringBefore('|').split(',')
        return bits.size >= 3 && bits[2].trim() == "1"
    }

    private fun clearSave(reason: String? = null) {
        prefs().edit().remove("save_state").apply()
        if (reason != null) Log.i(TAG, "Cleared save: $reason")
    }

    private fun livableSavedState(): String? {
        val data = savedState() ?: return null
        if (saveIsGameOver(data)) {
            clearSave("game over")
            return null
        }
        return data
    }

    private fun heroNameFromSave(data: String): String {
        val stored = prefs().getString("hero_name", null)?.trim().orEmpty()
        if (stored.isNotEmpty()) return stored
        val parts = data.split('|')
        if (parts.size < 3) return "Hero"
        val name = parts[2].substringBefore(';').substringBefore(',').trim()
        return name.ifEmpty { "Hero" }
    }

    private fun continueSavedGame() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready — can't load save.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        detachOnlineSession()
        val data = livableSavedState() ?: run {
            Toast.makeText(this, "No living adventure to continue — start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        val wasHost = prefs().getBoolean("was_online_host", false)
        val wasClient = prefs().getBoolean("was_online_client", false)
        val savedSid = prefs().getString("online_session_id", "")?.trim().orEmpty()
        try {
            loadGameState(data)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load save", e)
            Toast.makeText(this, "Save looked broken — start a new game.", Toast.LENGTH_LONG).show()
            prefs().edit().remove("save_state").apply()
            showStartDialog()
            return
        }
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        val status = try { getPlayerStatus() } catch (_: Exception) { "" }
        if (!status.contains(localPlayerName) && !status.contains("| HP:") && !status.contains("DM:")) {
            Toast.makeText(this, "Save looked empty — starting fresh is safer.", Toast.LENGTH_LONG).show()
            prefs().edit().remove("save_state").apply()
            showStartDialog()
            return
        }
        if (status.contains("Game Over", ignoreCase = true) || saveIsGameOver(data)) {
            clearSave("continued into game over")
            Toast.makeText(this, "That adventure already ended. Start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        // Successful deserialize + live status — do not treat as unclean exit.
        prefs().edit().putBoolean("crash_guard", false).apply()

        val resumeOnline = savedSid.isNotBlank() && (wasHost || wasClient) && firebaseReady()
        if (resumeOnline && wasHost) {
            setHost(true)
            activateSession()
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            setupMultiplayer(savedSid, asHost = true)
            if (!isOnlineHost || multiplayer?.isAvailable != true) {
                // Fall back to local authority with the loaded state.
                setHost(true)
                isOnlineHost = false
                isOnlineClient = false
                multiplayer = null
                onlineSessionId = ""
                Toast.makeText(this, "Welcome back (offline) — could not resume Host session.", Toast.LENGTH_LONG).show()
            } else {
                syncAndSave()
                Toast.makeText(this, "Welcome back, DM — resumed session $savedSid", Toast.LENGTH_SHORT).show()
            }
        } else if (resumeOnline && wasClient) {
            setHost(false)
            activateSession()
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            setupMultiplayer(savedSid, asHost = false)
            if (!isOnlineClient || multiplayer?.isAvailable != true) {
                setHost(true) // keep playing the snapshot solo
                isOnlineHost = false
                isOnlineClient = false
                multiplayer = null
                onlineSessionId = ""
                Toast.makeText(this, "Welcome back (offline) — DM session unavailable.", Toast.LENGTH_LONG).show()
            } else {
                scheduleJoinTimeout(savedSid, resuming = true)
                Toast.makeText(this, "Rejoining $savedSid…", Toast.LENGTH_SHORT).show()
            }
        } else {
            setHost(true)
            isOnlineHost = false
            isOnlineClient = false
            multiplayer = null
            onlineSessionId = ""
            activateSession()
            // activateSession re-arms crash_guard; clear again after confirmed good load.
            prefs().edit().putBoolean("crash_guard", false).apply()
            btnReset.visibility = View.GONE
            Toast.makeText(this, "Welcome back, $localPlayerName (solo)", Toast.LENGTH_SHORT).show()
        }
        refreshBattleArena()
        updateUi()
    }


    private fun showStartDialog() {
        Log.d(TAG, "Showing main menu")
        // Mode switch / Reset: drop any live Firebase listeners first.
        sessionActive = false
        detachOnlineSession()
        hideSettingsOverlay()
        refreshMainMenuButtons()
        mainMenuRoot.visibility = View.VISIBLE
        mainMenuRoot.bringToFront()
        btnReset.visibility = View.GONE
    }

    private fun hideMainMenu() {
        if (::mainMenuRoot.isInitialized) {
            mainMenuRoot.visibility = View.GONE
        }
    }

    private fun refreshMainMenuButtons() {
        if (!::btnMenuContinue.isInitialized) return
        val saved = livableSavedState()
        if (saved != null) {
            val wasHost = prefs().getBoolean("was_online_host", false)
            val wasClient = prefs().getBoolean("was_online_client", false)
            val sid = prefs().getString("online_session_id", "")?.trim().orEmpty()
            val tag = when {
                wasHost && sid.isNotBlank() -> "Host $sid"
                wasClient && sid.isNotBlank() -> "Join $sid"
                else -> "solo"
            }
            btnMenuContinue.visibility = View.VISIBLE
            btnMenuContinue.text = "Continue ($tag) — ${heroNameFromSave(saved)}"
            mainMenuSubtitle.text = "Welcome back, adventurer"
        } else {
            btnMenuContinue.visibility = View.GONE
            mainMenuSubtitle.text = "Choose your path"
        }
    }

    private fun wireMainMenuAndSettings() {
        mainMenuRoot = findViewById(R.id.mainMenuRoot)
        btnMenuContinue = findViewById(R.id.btnMenuContinue)
        btnMenuSolo = findViewById(R.id.btnMenuSolo)
        btnMenuHost = findViewById(R.id.btnMenuHost)
        btnMenuJoin = findViewById(R.id.btnMenuJoin)
        btnMenuSettings = findViewById(R.id.btnMenuSettings)
        btnMenuQuit = findViewById(R.id.btnMenuQuit)
        mainMenuSubtitle = findViewById(R.id.mainMenuSubtitle)

        settingsRoot = findViewById(R.id.settingsRoot)
        chkBeginnerTips = findViewById(R.id.chkBeginnerTips)
        chkMusic = findViewById(R.id.chkMusic)
        chkSfx = findViewById(R.id.chkSfx)
        chkDmVoice = findViewById(R.id.chkDmVoice)
        btnSettingsTutorial = findViewById(R.id.btnSettingsTutorial)
        btnSettingsAbandon = findViewById(R.id.btnSettingsAbandon)
        btnSettingsClose = findViewById(R.id.btnSettingsClose)
        settingsAboutText = findViewById(R.id.settingsAboutText)

        btnMenuContinue.setOnClickListener {
            // Menu stays until continueSavedGame succeeds (activateSession) or
            // failure paths re-show via showStartDialog().
            continueSavedGame()
        }
        btnMenuSolo.setOnClickListener {
            askHeroName { name ->
                localPlayerName = name
                showClassSelection(mode = "solo")
            }
        }
        btnMenuHost.setOnClickListener {
            if (!firebaseReady()) {
                showFirebaseRequiredDialog { showStartDialog() }
                return@setOnClickListener
            }
            askHeroName(hint = "Your DM name") { name ->
                localPlayerName = name
                startHostingAsDm()
            }
        }
        btnMenuJoin.setOnClickListener {
            if (!firebaseReady()) {
                showFirebaseRequiredDialog { showStartDialog() }
                return@setOnClickListener
            }
            askHeroName { name ->
                localPlayerName = name
                showJoinDialog()
            }
        }
        btnMenuSettings.setOnClickListener {
            gameAudio?.playUiClick()
            showSettingsOverlay()
        }
        btnMenuQuit.setOnClickListener { moveTaskToBack(true) }

        chkBeginnerTips.setOnCheckedChangeListener { _, checked ->
            soloCoachEnabled = checked
            prefs().edit().putBoolean("solo_coach_enabled", checked).apply()
        }
        chkMusic.setOnCheckedChangeListener { _, checked ->
            musicEnabled = checked
            prefs().edit().putBoolean("pref_music_enabled", checked).apply()
            gameAudio?.setMusicEnabled(checked)
        }
        chkSfx.setOnCheckedChangeListener { _, checked ->
            sfxEnabled = checked
            prefs().edit().putBoolean("pref_sfx_enabled", checked).apply()
            gameAudio?.setSfxEnabled(checked)
            if (checked) gameAudio?.playUiClick()
        }
        chkDmVoice.setOnCheckedChangeListener { _, checked ->
            dmVoiceEnabled = checked
            prefs().edit().putBoolean("pref_dm_voice_enabled", checked).apply()
            gameAudio?.setDmVoiceEnabled(checked)
        }
        btnSettingsTutorial.setOnClickListener {
            hideSettingsOverlay()
            showTutorial(0) {}
        }
        btnSettingsAbandon.setOnClickListener { confirmAbandonAdventure() }
        btnSettingsClose.setOnClickListener { hideSettingsOverlay() }
    }

    private fun showSettingsOverlay() {
        if (!::settingsRoot.isInitialized) return
        chkBeginnerTips.setOnCheckedChangeListener(null)
        chkMusic.setOnCheckedChangeListener(null)
        chkSfx.setOnCheckedChangeListener(null)
        chkDmVoice.setOnCheckedChangeListener(null)
        chkBeginnerTips.isChecked = soloCoachEnabled
        chkMusic.isChecked = musicEnabled
        chkSfx.isChecked = sfxEnabled
        chkDmVoice.isChecked = dmVoiceEnabled
        chkBeginnerTips.setOnCheckedChangeListener { _, checked ->
            soloCoachEnabled = checked
            prefs().edit().putBoolean("solo_coach_enabled", checked).apply()
        }
        chkMusic.setOnCheckedChangeListener { _, checked ->
            musicEnabled = checked
            prefs().edit().putBoolean("pref_music_enabled", checked).apply()
            gameAudio?.setMusicEnabled(checked)
        }
        chkSfx.setOnCheckedChangeListener { _, checked ->
            sfxEnabled = checked
            prefs().edit().putBoolean("pref_sfx_enabled", checked).apply()
            gameAudio?.setSfxEnabled(checked)
            if (checked) gameAudio?.playUiClick()
        }
        chkDmVoice.setOnCheckedChangeListener { _, checked ->
            dmVoiceEnabled = checked
            prefs().edit().putBoolean("pref_dm_voice_enabled", checked).apply()
            gameAudio?.setDmVoiceEnabled(checked)
        }
        btnSettingsAbandon.visibility = if (sessionActive) View.VISIBLE else View.GONE
        val verCode = try {
            packageManager.getPackageInfo(packageName, 0).longVersionCode.toInt()
        } catch (_: Exception) {
            19
        }
        val verName = try {
            packageManager.getPackageInfo(packageName, 0).versionName ?: "1.3"
        } catch (_: Exception) {
            "1.3"
        }
        settingsAboutText.text =
            getString(R.string.app_name) + "\nversion " + verName + " (" + verCode + ")" +
                "\n\nCompatible with 5e SRD (SRD 5.1, CC-BY 4.0). Not an official D&D product. " +
                "See ATTRIBUTION.md.\n\nAudio (CC0): Ironchest Dungeon Loops (explore + tension BGM); " +
                "StarNinjas sword/clash SFX; Darsycho monster snarl; bart interface beep. " +
                "DM voice uses on-device Text-to-Speech (no third-party voices)."
        settingsRoot.visibility = View.VISIBLE
        settingsRoot.bringToFront()
    }

    private fun hideSettingsOverlay() {
        if (::settingsRoot.isInitialized) {
            settingsRoot.visibility = View.GONE
        }
    }

    private fun confirmAbandonAdventure() {
        AlertDialog.Builder(this)
            .setTitle("Leave adventure?")
            .setMessage("This clears your current session flags and save, then returns to the main menu.")
            .setPositiveButton("Abandon") { _, _ -> abandonAdventure() }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun abandonAdventure() {
        hideSettingsOverlay()
        sessionActive = false
        detachOnlineSession()
        prefs().edit()
            .remove("save_state")
            .putBoolean("crash_guard", false)
            .remove("was_online_host")
            .remove("was_online_client")
            .remove("online_session_id")
            .apply()
        Toast.makeText(this, "Adventure abandoned.", Toast.LENGTH_SHORT).show()
        showStartDialog()
    }

    private fun askHeroName(hint: String = "Your hero name", onName: (String) -> Unit) {
        val input = EditText(this).apply {
            this.hint = hint
            val prior = prefs().getString("hero_name", null)
            if (!prior.isNullOrBlank()) setText(prior)
            setSelection(text.length)
        }
        AlertDialog.Builder(this)
            .setTitle(hint)
            .setView(input)
            .setCancelable(false)
            .setPositiveButton("Next") { _, _ ->
                onName(input.text.toString().ifBlank { "Hero" })
            }
            .setNegativeButton("Back") { _, _ -> showStartDialog() }
            .show()
    }

    private fun startHostingAsDm() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not ready.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        detachOnlineSession()
        try {
            startDmSession(localPlayerName)
            setHost(true)
        } catch (e: Exception) {
            Log.e(TAG, "startDmSession failed", e)
            Toast.makeText(this, "Could not open DM table.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        activateSession()
        btnReset.visibility = View.GONE
        val sidNow = try { getSessionId() } catch (_: Exception) { "" }
        setupMultiplayer(sidNow, asHost = true)
        if (!isOnlineHost || multiplayer?.isAvailable != true) {
            Toast.makeText(this, "Could not start online session — check Firebase.", Toast.LENGTH_LONG).show()
            detachOnlineSession()
            sessionActive = false
            showFirebaseRequiredDialog { showStartDialog() }
            return
        }
        syncAndSave()
        updateUi()
        showSessionShareDialog()
        showDmToolsDialog(firstOpen = true)
    }

    private fun showClassSelection(mode: String, sid: String = "") {
        val classes = arrayOf("Fighter", "Wizard", "Rogue", "Cleric")
        val title = when (mode) {
            "join" -> "Choose your class"
            else -> "Choose your class"
        }
        AlertDialog.Builder(this).setTitle(title).setItems(classes) { _, which ->
            Log.d(TAG, "Class $which selected. mode=$mode")
            localClassId = which
            when (mode) {
                "solo" -> {
                    detachOnlineSession()
                    resetGame(which, localPlayerName)
                    setHost(true)
                    maybeOfferTutorialThenCoach()
                    activateSession()
                    btnReset.visibility = View.GONE
                    syncAndSave()
                    updateUi()
                }
                "join" -> {
                    detachOnlineSession()
                    setHost(false)
                    try { prepareClientJoin() } catch (e: Exception) {
                        Log.e(TAG, "prepareClientJoin failed", e)
                    }
                    activateSession()
                    btnReset.visibility = View.GONE
                    lastBattleRoster = ""
                    setupMultiplayer(sid, asHost = false)
                    if (multiplayer?.isAvailable != true) {
                        Toast.makeText(this, "Could not connect — check Firebase.", Toast.LENGTH_LONG).show()
                        resetToStartMenu()
                        return@setItems
                    }
                    multiplayer?.publishLobbyJoin(localPlayerName, which)
                    multiplayer?.pushAction(
                        mapOf(
                            "type" to "join",
                            "playerName" to localPlayerName,
                            "classId" to which,
                            "targetIndex" to 0
                        )
                    )
                    scheduleJoinTimeout(sid, resuming = false)
                    updateUi()
                    Toast.makeText(
                        this,
                        "Joining $sid — waiting for the DM to admit you…",
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }.setNegativeButton("Back") { _, _ -> showStartDialog() }.show()
    }

    private fun showJoinDialog() {
        val input = EditText(this).apply { hint = "Session ID (e.g. DND-A1B2)" }
        AlertDialog.Builder(this).setTitle("Join DM session").setView(input)
            .setPositiveButton("Join") { _, _ ->
                val sid = input.text.toString().trim().uppercase()
                if (sid.isNotEmpty()) showClassSelection(mode = "join", sid = sid)
                else showStartDialog()
            }
            .setNegativeButton("Back") { _, _ -> showStartDialog() }
            .show()
    }


    private fun showDmToolsDialog(firstOpen: Boolean = false) {
        val options = arrayOf(
            "Begin dungeon (when heroes joined)",
            "Advance to next room",
            "Narrate / set scene",
            "Grant short rest",
            "Show session ID again",
            "Close"
        )
        val title = if (firstOpen) "DM tools — you control the story" else "DM tools"
        AlertDialog.Builder(this)
            .setTitle(title)
            .setItems(options) { _, which ->
                when (which) {
                    0 -> {
                        dmBeginDungeon(); syncAndSave(); updateUi()
                        Toast.makeText(this, "Dungeon begun.", Toast.LENGTH_SHORT).show()
                    }
                    1 -> {
                        dmAdvanceRoom(); syncAndSave(); updateUi()
                    }
                    2 -> {
                        val input = EditText(this).apply { hint = "What do the heroes see?" }
                        AlertDialog.Builder(this)
                            .setTitle("Narrate")
                            .setView(input)
                            .setPositiveButton("Speak") { _, _ ->
                                val line = input.text.toString().trim()
                                if (line.isNotEmpty()) {
                                    dmNarrate(line)
                                    syncAndSave()
                                    updateUi()
                                }
                            }
                            .setNegativeButton("Cancel", null)
                            .show()
                    }
                    3 -> {
                        dmGrantShortRest(); syncAndSave(); updateUi()
                    }
                    4 -> showSessionShareDialog()
                }
            }
            .show()
    }

    private fun showSessionShareDialog() {
        val sid = onlineSessionId.ifBlank {
            try { getSessionId() } catch (_: Exception) { "" }
        }
        if (sid.isBlank()) return
        val clip = android.content.ClipData.newPlainText("DND_SESSION", sid)
        (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
        AlertDialog.Builder(this)
            .setTitle("You are the DM")
            .setMessage(
                "Session ID:\n\n$sid\n\n" +
                    "You are the DM — not a hero on the battle map.\n" +
                    "Friends: Join session, enter this ID, pick a class.\n" +
                    "Use the DM button for Begin dungeon / Advance / Narrate.\n\n" +
                    "(Copied to clipboard.)"
            )
            .setPositiveButton("Got it", null)
            .show()
    }

    private fun showCharacterSheet() {
        if (!nativeReady) return
        val sheet = try { getDetailedSheet(localPlayerName) } catch (_: Exception) { "Hero not found." }
        if (sheet.contains("not found", ignoreCase = true)) {
            Toast.makeText(this, sheet, Toast.LENGTH_SHORT).show()
            return
        }
        val view = layoutInflater.inflate(R.layout.dialog_character_sheet, null)
        val portrait = view.findViewById<ImageView>(R.id.sheetPortrait)
        val nameTv = view.findViewById<TextView>(R.id.sheetName)
        val classTv = view.findViewById<TextView>(R.id.sheetClass)
        val goldTv = view.findViewById<TextView>(R.id.sheetGold)
        val hpLabel = view.findViewById<TextView>(R.id.sheetHpLabel)
        val hpBar = view.findViewById<ProgressBar>(R.id.sheetHpBar)
        val resourcesTv = view.findViewById<TextView>(R.id.sheetResources)
        val equippedTv = view.findViewById<TextView>(R.id.sheetEquipped)
        val pointsHint = view.findViewById<TextView>(R.id.sheetPointsHint)
        val levelUpBtn = view.findViewById<Button>(R.id.sheetLevelUp)

        nameTv.text = localPlayerName
        val lvlLine = sheet.lineSequence().firstOrNull { it.startsWith("Lvl ") }.orEmpty()
        classTv.text = lvlLine.substringBefore("|").trim().ifBlank { "Hero" }
        val gold = Regex("""Gold:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: "?"
        goldTv.text = "Gold: $gold"
        val hp = Regex("""HP:\s*(\d+)/(\d+)""").find(sheet)
        val cur = hp?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
        val max = hp?.groupValues?.getOrNull(2)?.toIntOrNull() ?: 1
        val ac = Regex("""AC:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: "?"
        hpLabel.text = "HP $cur/$max · AC $ac"
        hpBar.max = max.coerceAtLeast(1)
        hpBar.progress = cur.coerceIn(0, hpBar.max)
        hpBar.progressDrawable = getDrawable(R.drawable.bg_hp_bar)?.mutate()
        val res = Regex("""Resources:\s*(\d+)/(\d+)""").find(sheet)
        resourcesTv.text = if (res != null) {
            "Resources: ${res.groupValues[1]}/${res.groupValues[2]}"
        } else "Resources: —"
        fun stat(key: String): String {
            val m = Regex("""$key:\s*(\d+)\s*\(([^)]+)\)""").find(sheet)
            return if (m != null) "$key ${m.groupValues[1]} (${m.groupValues[2]})" else "$key —"
        }
        view.findViewById<TextView>(R.id.sheetStatStr).text = stat("STR")
        view.findViewById<TextView>(R.id.sheetStatDex).text = stat("DEX")
        view.findViewById<TextView>(R.id.sheetStatCon).text = stat("CON")
        view.findViewById<TextView>(R.id.sheetStatInt).text = stat("INT")
        view.findViewById<TextView>(R.id.sheetStatWis).text = stat("WIS")
        view.findViewById<TextView>(R.id.sheetStatCha).text = stat("CHA")
        val weapon = sheet.lineSequence().firstOrNull { it.startsWith("Weapon:") }?.removePrefix("Weapon:")?.trim()
        val armor = sheet.lineSequence().firstOrNull { it.startsWith("Armor:") }?.removePrefix("Armor:")?.trim()
        equippedTv.text = buildString {
            append("Weapon: ${weapon ?: "None"}")
            append("\nArmor: ${armor ?: "None"}")
        }
        portrait.setImageResource(spriteFor(BattleUnit(true, localPlayerName, localClassId, cur, max)))
        val canLevel = sheet.contains("POINTS TO SPEND")
        if (canLevel) {
            val pts = Regex("""POINTS TO SPEND:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1) ?: ""
            pointsHint.visibility = View.VISIBLE
            pointsHint.text = "You have $pts attribute point(s) to spend."
            levelUpBtn.visibility = View.VISIBLE
        } else {
            pointsHint.visibility = View.GONE
            levelUpBtn.visibility = View.GONE
        }
        val dialog = AlertDialog.Builder(this)
            .setTitle("Hero Sheet")
            .setView(view)
            .setPositiveButton("Close", null)
            .create()
        levelUpBtn.setOnClickListener {
            dialog.dismiss()
            showStatUpgradeDialog()
        }
        dialog.show()
    }

    private fun showStatUpgradeDialog() {
        val stats = arrayOf("Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma")
        AlertDialog.Builder(this).setTitle("Spend Point").setItems(stats) { _, which ->
            performOrQueue("levelup", which) {
                doIncreaseStat(localPlayerName, which)
            }
            // Clients already toast "Sent to the DM…"; host/solo get sync via performOrQueue.
            if (!isOnlineClient) updateUi()
        }.show()
    }


    private fun parsePlayerGold(): Int {
        return try {
            val sheet = getDetailedSheet(localPlayerName)
            Regex("""Gold:\s*(\d+)""").find(sheet)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
        } catch (_: Exception) { 0 }
    }

    private fun appendCombatFeed(line: String) {
        val cleaned = line.trim()
        if (cleaned.isEmpty()) return
        if (combatFeed.isNotEmpty() && combatFeed.last() == cleaned) return
        combatFeed.addLast(cleaned)
        while (combatFeed.size > 6) combatFeed.removeFirst()
        logText.text = combatFeed.joinToString("\n")
        val scroll = findViewById<android.widget.ScrollView?>(R.id.combatLogScroll)
        scroll?.post { scroll.fullScroll(View.FOCUS_DOWN) }
    }

    private fun showShopDialog() {
        if (!nativeReady) {
            Toast.makeText(this, "Native library not loaded — cannot open shop.", Toast.LENGTH_SHORT).show()
            return
        }
        val manifest = getShopManifest()
        if (manifest.isBlank()) {
            Toast.makeText(this, "The merchant has nothing for sale.", Toast.LENGTH_SHORT).show()
            return
        }
        // C++ format: "0:Steel Blade (25g);1:Platemail (25g);..."
        val entries = manifest.split(';').map { it.trim() }.filter { it.isNotEmpty() }
        if (entries.isEmpty()) {
            Toast.makeText(this, "The merchant has nothing for sale.", Toast.LENGTH_SHORT).show()
            return
        }
        val view = layoutInflater.inflate(R.layout.dialog_shop, null)
        val goldTv = view.findViewById<TextView>(R.id.shopGoldText)
        val list = view.findViewById<LinearLayout>(R.id.shopItemList)
        fun refreshGold() {
            goldTv.text = "Your gold: ${parsePlayerGold()}g"
        }
        refreshGold()
        val dialog = AlertDialog.Builder(this)
            .setTitle("Merchant")
            .setView(view)
            .setNegativeButton("Leave", null)
            .create()
        for (entry in entries) {
            val index = entry.substringBefore(':').toIntOrNull() ?: continue
            val label = entry.substringAfter(':', entry)
            val cost = Regex("""\((\d+)g\)""").find(label)?.groupValues?.getOrNull(1)?.toIntOrNull() ?: 0
            val name = label.replace(Regex("""\s*\(\d+g\)\s*$"""), "").trim()
            val row = layoutInflater.inflate(R.layout.item_shop_row, list, false)
            row.findViewById<TextView>(R.id.shopItemName).text = name
            row.findViewById<TextView>(R.id.shopItemCost).text = "${cost}g"
            val buy = row.findViewById<Button>(R.id.shopBuyBtn)
            fun updateBuyEnabled() {
                buy.isEnabled = parsePlayerGold() >= cost
            }
            updateBuyEnabled()
            buy.setOnClickListener {
                if (isOnlineClient) {
                    performOrQueue("buy", index) { }
                    dialog.dismiss()
                    return@setOnClickListener
                }
                val ok = try { doBuyItem(localPlayerName, index) } catch (_: Exception) { false }
                val ev = try { getLastEvent() } catch (_: Exception) { "" }
                if (ok) {
                    syncAndSave()
                    appendCombatFeed(ev.ifBlank { "Bought $name." })
                    refreshGold()
                    updateBuyEnabled()
                    // refresh all buy buttons
                    for (i in 0 until list.childCount) {
                        val r = list.getChildAt(i)
                        val b = r.findViewById<Button>(R.id.shopBuyBtn) ?: continue
                        val cTxt = r.findViewById<TextView>(R.id.shopItemCost)?.text?.toString().orEmpty()
                        val c = cTxt.removeSuffix("g").toIntOrNull() ?: 0
                        b.isEnabled = parsePlayerGold() >= c
                    }
                    updateUi()
                } else {
                    appendCombatFeed(ev.ifBlank { "Could not buy that." })
                    Toast.makeText(this, ev.ifBlank { "Could not buy that." }, Toast.LENGTH_SHORT).show()
                    refreshGold()
                }
            }
            list.addView(row)
        }
        dialog.show()
    }

    private fun showJournal() {
        if (!nativeReady) return
        val journal = try { getJournal() } catch (_: Exception) { "" }
        val chatLog = try { getChatHistory() } catch (_: Exception) { "" }
        val recent = chatLog.lineSequence()
            .filter { it.isNotBlank() }
            .toList()
            .takeLast(12)
            .joinToString("\n")
        val body = buildString {
            if (recent.isNotBlank()) {
                append("Recent\n")
                append(recent)
                append("\n\n")
            }
            append("Journal\n")
            append(journal.ifBlank { "(empty)" })
        }
        AlertDialog.Builder(this)
            .setTitle("Adventure Log")
            .setMessage(body)
            .setPositiveButton("Close", null)
            .show()
    }

    private fun startUiUpdateLoop() {
        if (uiLoopStarted) return
        uiLoopStarted = true
        handler.post(uiTick)
    }


    private fun updateUi() {
        if (!nativeReady) return
        val status = getPlayerStatus()
        val isShop = isMerchantRoom()

        // Compact HUD — full sheet stays behind Sheet
        val turnLine = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: "Turn: —"
        val roomLine = status.lineSequence().firstOrNull {
            it.startsWith("Room") || it.contains("Room ")
        } ?: "Room ?"
        val whose = turnLine.substringAfter("Turn:", "—").trim()
        val myTurnBanner = whose.equals(localPlayerName, ignoreCase = true) ||
            whose.equals("You", ignoreCase = true)
        turnBanner.text = when {
            isOnlineHost && status.contains("Game Over", ignoreCase = true) -> "DM · defeat…"
            isOnlineHost && status.contains("waiting for players", ignoreCase = true) -> "DM · waiting for heroes"
            isOnlineHost -> "DM · directing the table"
            isOnlineClient && remoteDmName.isNotBlank() -> "DM: $remoteDmName · $whose"
            status.contains("Quest complete", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                "Main quest done — Ashen Lantern · Search, Rest, or Onward"
            status.contains("Quest complete", ignoreCase = true) -> "Main quest done — Ashen Lantern"
            status.contains("Quest: Ashen Lantern", ignoreCase = true) &&
                try { isRoomCleared() } catch (_: Exception) { false } ->
                status.lineSequence().firstOrNull { it.startsWith("Quest:") }?.removePrefix("Quest:")?.trim()
                    ?.let { "Quest · $it · clear" } ?: "Room clear — Search, Rest, or Onward"
            isShop -> "Safe haven — merchant"
            try { isRoomCleared() } catch (_: Exception) { false } -> "Room clear — Search, Rest, or Onward"
            status.contains("Game Over", ignoreCase = true) -> "Defeat…"
            myTurnBanner -> "Your move, $localPlayerName"
            else -> "$whose acts…"
        }
        val roomBit = roomLine.substringBefore("|").trim().ifBlank { roomLine }
        statusText.text = when {
            isOnlineHost && onlineSessionId.isNotBlank() -> "$roomBit · ID $onlineSessionId"
            isOnlineClient && onlineSessionId.isNotBlank() -> "$roomBit · joined $onlineSessionId"
            else -> roomBit
        }

        val specialName = try { getSpecialName() } catch (_: Exception) { "Special" }
        // Two-line labels so text stays visible on narrow / large-font screens
        val roomCleared = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
        val searchedRoom = try { hasSearchedRoom() } catch (_: Exception) { false }
        btnSpecial.text = if (isShop) "Info" else "✦\n$specialName"
        btnAttack.text = when {
            isShop -> "🛒\nShop"
            roomCleared -> "🚪\nOnward"
            else -> "⚔\nAttack"
        }
        btnHeal.text = "⚗\nPotion"
        btnRest.text = if (isShop) "Leave" else "🌙\nRest"
        btnInteract.visibility = if (isShop) View.GONE else View.VISIBLE
        btnInteract.text = if (searchedRoom) "🔍\nDone" else "🔍\nSearch"

        val dmTable = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTable) {
            btnSheet.text = "DM"
            turnBanner.text = when {
                status.contains("Game Over", ignoreCase = true) -> "DM · defeat…"
                getBattleRoster().substringBefore('|').isBlank() -> "DM · waiting for heroes to Join"
                else -> "DM · tap DM for story tools"
            }
        } else if (isOnlineClient && remoteDmName.isNotBlank()) {
            btnSheet.text = "Sheet"
        }


        val isGameOver = status.contains("Game Over", ignoreCase = true)
        val localDown = status.lineSequence().any {
            it.contains(localPlayerName) && (
                it.contains("[DOWN]", ignoreCase = true) ||
                it.contains("[DEAD]", ignoreCase = true) ||
                it.contains("[STABLE]", ignoreCase = true)
            )
        }
        val clearedForTurn = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
        val isMyTurn = !isGameOver && !localDown && (
            isShop || clearedForTurn ||
                status.contains("Turn: Safe", ignoreCase = true) ||
                status.contains("Turn: $localPlayerName") || status.contains("Turn: You")
        )

        val dmTableLock = isOnlineHost && try { isDmTable() } catch (_: Exception) { isOnlineHost }
        if (dmTableLock) {
            btnAttack.isEnabled = false
            btnSpecial.isEnabled = false
            btnHeal.isEnabled = false
            btnRest.isEnabled = false
            btnInteract.isEnabled = false
        } else {
            val inCombat = try { isInCombat() } catch (_: Exception) { false }
            val cleared = !isShop && try { isRoomCleared() } catch (_: Exception) { false }
            val searched = try { hasSearchedRoom() } catch (_: Exception) { false }
            // Cleared chamber uses Turn: Safe — treat like shop exploration for button unlocks.
            val canAct = isMyTurn || (!isGameOver && !localDown && (isShop || cleared ||
                status.contains("Turn: Safe", ignoreCase = true)))
            btnSpecial.isEnabled = canAct && !cleared && !isShop
            btnHeal.isEnabled = canAct && !cleared && !isShop
            // Onward / shop after clear even if local hero is downed (party continues).
            btnAttack.isEnabled = when {
                isGameOver -> false
                isShop -> canAct || !localDown
                cleared -> true
                else -> canAct && (inCombat || isMyTurn)
            }
            // Short Rest out of combat; party Rest may revive a downed local hero after a clear.
            btnRest.isEnabled = !isGameOver && (isShop || (!inCombat && (canAct || cleared)))
            // Search only when room clear, once per room (never while downed/dead).
            btnInteract.isEnabled = canAct && !isShop && cleared && !searched
        }

        val roomDesc = getRoomDescription()
        if (roomDesc != lastRoomDesc) {
            roomDescText.text = roomDesc
            lastRoomDesc = roomDesc
        }

        // Track chat for journal, but never clobber the combat feed with chat last-line.
        val chat = getChatHistory()
        if (chat != lastChatHistory) {
            lastChatHistory = chat
            gameAudio?.maybeSpeakDmFromChat(chat)
        }

        val inCombatNow = try { isInCombat() } catch (_: Exception) { false }
        if (lastCombatMusicState != inCombatNow) {
            lastCombatMusicState = inCombatNow
            gameAudio?.syncCombatMusic(inCombatNow)
        }

        val roomKey = try { getRoomDescription().take(40) } catch (_: Exception) { "" } + "|" +
            (try { getPlayerStatus().lineSequence().firstOrNull { it.startsWith("Room") }.orEmpty() } catch (_: Exception) { "" })
        if (roomKey != lastSelectionRoomKey) {
            if (lastSelectionRoomKey.isNotEmpty()) clearCombatSelection("room change")
            lastSelectionRoomKey = roomKey
        }

        val lastEv = getLastEvent()
        if (lastEv.isNotEmpty() && lastEv != lastProcessedEvent) {
            lastProcessedEvent = lastEv
            appendCombatFeed(lastEv)
            if (lastEv.contains("Game Over", ignoreCase = true)) {
                btnReset.visibility = View.VISIBLE
                clearSave("game over event")
            }
            maybeAnimateFromEvent(lastEv)
            // NPC/enemy AI attacks: show dice when we didn't already schedule from a player tap
            if (looksLikeCombatRoll(lastEv)) {
                maybeShowDiceOverlay(lastEv)
            }
        }

        if (isGameOver) {
            btnReset.visibility = View.VISIBLE
            btnAttack.isEnabled = false
            btnSpecial.isEnabled = false
            btnHeal.isEnabled = false
            btnRest.isEnabled = false
            btnInteract.isEnabled = false
            clearSave("game over ui")
        }

        refreshBattleArena()
        if (!isGameOver) maybeSoloDmCoach(status, isShop)
    }

    private data class BattleUnit(
        val isPlayer: Boolean,
        val name: String,
        val classId: Int,
        val hp: Int,
        val maxHp: Int
    )

    private fun parseBattleRoster(raw: String): Pair<List<BattleUnit>, List<BattleUnit>> {
        val party = mutableListOf<BattleUnit>()
        val foes = mutableListOf<BattleUnit>()
        if (raw.isBlank()) return party to foes
        val parts = raw.split('|', limit = 2)
        fun parseSide(side: String, players: Boolean) {
            if (side.isBlank()) return
            for (entry in side.split(';')) {
                if (entry.isBlank()) continue
                val bits = entry.split(',')
                if (bits.size < 5) continue
                val list = if (players) party else foes
                list += BattleUnit(
                    isPlayer = players,
                    name = bits[1],
                    classId = bits[2].toIntOrNull() ?: 0,
                    hp = bits[3].toIntOrNull() ?: 0,
                    maxHp = bits[4].toIntOrNull() ?: 1
                )
            }
        }
        parseSide(parts.getOrNull(0).orEmpty(), true)
        parseSide(parts.getOrNull(1).orEmpty(), false)
        return party to foes
    }

    private fun spriteFor(unit: BattleUnit): Int {
        if (!unit.isPlayer) {
            val n = unit.name.lowercase()
            return when {
                n.contains("skeleton") -> R.drawable.sprite_skeleton
                else -> R.drawable.sprite_goblin
            }
        }
        return when (unit.classId) {
            0 -> R.drawable.sprite_fighter
            1 -> R.drawable.sprite_wizard
            2 -> R.drawable.sprite_rogue
            3 -> R.drawable.sprite_cleric
            else -> R.drawable.sprite_fighter
        }
    }

    private fun refreshBattleArena() {
        val roster = getBattleRoster()
        if (roster == lastBattleRoster) {
            applySelectionHighlights()
            return
        }
        val oldNames = lastBattleRoster
        lastBattleRoster = roster
        val (party, foes) = parseBattleRoster(roster)
        // Structural change (names / count) clears selection; HP-only updates keep name selection.
        val oldPartyNames = oldNames.substringBefore('|').split(';').mapNotNull { it.split(',').getOrNull(1) }
        val oldFoeNames = oldNames.substringAfter('|', "").split(';').mapNotNull { it.split(',').getOrNull(1) }
        val newPartyNames = party.map { it.name }
        val newFoeNames = foes.map { it.name }
        if (oldNames.isNotEmpty() && (oldPartyNames != newPartyNames || oldFoeNames != newFoeNames)) {
            clearCombatSelection("roster membership changed")
        } else {
            // Drop selection if the named unit is now dead / gone
            if (selectedEnemyName != null && foes.none { it.name == selectedEnemyName && it.hp > 0 }) {
                selectedEnemyName = null
            }
            if (selectedAllyName != null && party.none { it.name == selectedAllyName }) {
                selectedAllyName = null
            }
        }
        rebuildColumn(partyColumn, party, alignEnd = false, isEnemyColumn = false)
        rebuildColumn(enemyColumn, foes, alignEnd = true, isEnemyColumn = true)
        applySelectionHighlights()
        if (foeTargetingMode) startTargetPulse(enemyColumn)
        if (allyTargetingMode) startTargetPulse(partyColumn)
    }

    private fun rebuildColumn(
        column: LinearLayout,
        units: List<BattleUnit>,
        alignEnd: Boolean,
        isEnemyColumn: Boolean
    ) {
        column.removeAllViews()
        val density = resources.displayMetrics.density
        val spriteSize = (104 * density).toInt()
        val barW = (88 * density).toInt()
        val barH = (10 * density).toInt()
        units.forEachIndexed { index, unit ->
            val wrap = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = if (alignEnd) Gravity.END else Gravity.START
                setPadding(6, 6, 6, 6)
                tag = unit.name
            }
            val img = ImageView(this).apply {
                setImageResource(spriteFor(unit))
                layoutParams = LinearLayout.LayoutParams(spriteSize, spriteSize)
                adjustViewBounds = true
                tag = "sprite"
                alpha = if (unit.hp <= 0) 0.35f else 1f
            }
            val name = TextView(this).apply {
                text = unit.name
                setTextColor(Color.parseColor("#FFE8D5A3"))
                textSize = 12f
                setTypeface(typeface, android.graphics.Typeface.BOLD)
                gravity = if (alignEnd) Gravity.END else Gravity.START
            }
            val bar = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
                max = unit.maxHp.coerceAtLeast(1)
                progress = unit.hp.coerceIn(0, max)
                layoutParams = LinearLayout.LayoutParams(barW, barH).apply {
                    topMargin = (4 * density).toInt()
                    bottomMargin = (2 * density).toInt()
                }
                progressDrawable = getDrawable(R.drawable.bg_hp_bar)?.mutate()
            }
            val hp = TextView(this).apply {
                text = "${unit.hp}/${unit.maxHp}"
                setTextColor(Color.parseColor("#FFB0A090"))
                textSize = 10f
                gravity = if (alignEnd) Gravity.END else Gravity.START
            }
            wrap.addView(img)
            wrap.addView(name)
            wrap.addView(bar)
            wrap.addView(hp)
            val selectable = if (isEnemyColumn) unit.hp > 0 else true
            if (selectable) {
                wrap.isClickable = true
                wrap.isFocusable = true
                wrap.setOnClickListener {
                    onBattleUnitTapped(unit.name, index, isEnemyColumn)
                }
            } else {
                wrap.isClickable = false
                wrap.setOnClickListener(null)
            }
            column.addView(wrap)
        }
    }

    private fun onBattleUnitTapped(name: String, index: Int, isEnemy: Boolean) {
        if (isEnemy) {
            if (selectedEnemyName == name) {
                selectedEnemyName = null // retap clears
            } else {
                selectedEnemyName = name
            }
            foeTargetingMode = false
            stopTargetPulse()
            applySelectionHighlights()
            Toast.makeText(this, "Target: $name", Toast.LENGTH_SHORT).show()
        } else {
            if (selectedAllyName == name) {
                selectedAllyName = null
            } else {
                selectedAllyName = name
            }
            allyTargetingMode = false
            stopTargetPulse()
            applySelectionHighlights()
            Toast.makeText(this, "Ally: $name", Toast.LENGTH_SHORT).show()
        }
    }

    private fun applySelectionHighlights() {
        fun paint(column: LinearLayout, selected: String?) {
            for (i in 0 until column.childCount) {
                val child = column.getChildAt(i)
                val selectedHere = selected != null && child.tag == selected
                child.background = if (selectedHere) getDrawable(R.drawable.bg_sprite_selected) else null
            }
        }
        paint(partyColumn, selectedAllyName)
        paint(enemyColumn, selectedEnemyName)
    }

    private fun startTargetPulse(column: LinearLayout) {
        stopTargetPulse()
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            val img = child.findViewWithTag<ImageView?>("sprite") ?: continue
            if (img.alpha < 0.5f) continue
            val anim = AlphaAnimation(0.45f, 1f).apply {
                duration = 550
                repeatMode = Animation.REVERSE
                repeatCount = Animation.INFINITE
            }
            img.startAnimation(anim)
        }
    }

    private fun stopTargetPulse() {
        fun clear(column: LinearLayout) {
            for (i in 0 until column.childCount) {
                val img = column.getChildAt(i).findViewWithTag<ImageView?>("sprite")
                img?.clearAnimation()
            }
        }
        clear(partyColumn)
        clear(enemyColumn)
    }

    private fun findSpriteByName(column: LinearLayout, name: String): ImageView? {
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            val tag = child.tag?.toString().orEmpty()
            if (tag.equals(name, ignoreCase = true) || tag.contains(name, ignoreCase = true) ||
                name.contains(tag, ignoreCase = true)
            ) {
                return child.findViewWithTag("sprite") as? ImageView
            }
        }
        return null
    }

    private fun firstSprite(column: LinearLayout): ImageView? {
        if (column.childCount == 0) return null
        return column.getChildAt(0).findViewWithTag("sprite") as? ImageView
    }

    private fun animateAttack(
        attackerIsPlayer: Boolean,
        targetEnemyIndex: Int? = null,
        targetAllyIndex: Int? = null,
        attackerName: String? = null
    ) {
        val attackerCol = if (attackerIsPlayer) partyColumn else enemyColumn
        val defenderCol = if (attackerIsPlayer) enemyColumn else partyColumn
        val a = when {
            !attackerName.isNullOrBlank() -> findSpriteByName(attackerCol, attackerName)
                ?: firstSprite(attackerCol)
            else -> firstSprite(attackerCol)
        } ?: return
        val defendView = when {
            attackerIsPlayer && targetEnemyIndex != null && targetEnemyIndex < enemyColumn.childCount ->
                enemyColumn.getChildAt(targetEnemyIndex).findViewWithTag<ImageView?>("sprite")
            !attackerIsPlayer && targetAllyIndex != null && targetAllyIndex < partyColumn.childCount ->
                partyColumn.getChildAt(targetAllyIndex).findViewWithTag<ImageView?>("sprite")
            selectedEnemyName != null && attackerIsPlayer ->
                findSpriteByName(enemyColumn, selectedEnemyName!!)
            selectedAllyName != null && !attackerIsPlayer ->
                findSpriteByName(partyColumn, selectedAllyName!!)
            else -> firstSprite(defenderCol)
        }
        val dx = if (attackerIsPlayer) 90f else -90f
        a.animate().cancel()
        a.translationX = 0f
        a.animate()
            .translationX(dx)
            .setDuration(140)
            .setInterpolator(AccelerateDecelerateInterpolator())
            .withEndAction {
                a.animate().translationX(0f).setDuration(180).start()
                defendView?.let { d ->
                    d.animate().cancel()
                    d.animate()
                        .scaleX(1.2f).scaleY(1.2f)
                        .setDuration(90)
                        .withEndAction {
                            d.animate().scaleX(1f).scaleY(1f).setDuration(140).start()
                        }.start()
                }
            }.start()
    }

    private fun maybeAnimateFromEvent(event: String) {
        if (event == lastAnimEvent) return
        lastAnimEvent = event
        val e = event.lowercase()
        when {
            e.contains("attacks") || e.contains("hits") || e.contains("slashes") || e.contains("fireball") || e.contains("sneak") || e.contains("action surge") || e.contains("magic missile") -> {
                // Prefer "Attacker attacks Target" so downed local heroes are not animated as attackers.
                val attackMatch = Regex("""^(.+?)\s+attacks\s+(.+?)(?:!|\.|$)""", RegexOption.IGNORE_CASE)
                    .find(event.trim())
                val attackerName = attackMatch?.groupValues?.getOrNull(1)?.trim().orEmpty()
                val foeNames = listOf("goblin", "skeleton")
                val attackerIsFoe = foeNames.any { attackerName.lowercase().contains(it) } ||
                    (attackerName.isBlank() && foeNames.any { e.contains(it) } &&
                        !e.startsWith(localPlayerName.lowercase()))
                animateAttack(
                    attackerIsPlayer = !attackerIsFoe,
                    attackerName = attackerName.ifBlank { null }
                )
                if (attackerIsFoe) {
                    gameAudio?.playGrowl()
                    gameAudio?.playAttack()
                } else {
                    gameAudio?.playAttack()
                }
                if (e.contains("hit") || e.contains("damage") || e.contains("critical")) {
                    gameAudio?.playHit()
                }
            }
            e.contains("damage") || e.contains("struck") || e.contains("wounded") -> {
                gameAudio?.playHit()
            }
        }
    }

    private fun wireDiceOverlay() {
        val root = findViewById<View?>(R.id.diceOverlayRoot) ?: return
        diceOverlayRoot = root
        diceTitle = root.findViewById(R.id.diceTitle)
        diceD20 = root.findViewById(R.id.diceD20)
        diceDamage = root.findViewById(R.id.diceDamage)
        diceSummary = root.findViewById(R.id.diceSummary)
        root.visibility = View.GONE
    }

    private fun looksLikeCombatRoll(event: String): Boolean {
        val e = event.lowercase()
        return e.contains("d20=") || e.contains("damage") || e.contains("miss!") ||
            e.contains("magic missile") || e.contains("healing") || e.contains("potion")
    }

    private var lastDiceEvent = ""
    private fun scheduleDiceFromLastEvent() {
        handler.postDelayed({
            val ev = try { getLastEvent() } catch (_: Exception) { "" }
            if (ev.isNotBlank()) maybeShowDiceOverlay(ev, force = true)
        }, 80)
    }

    private fun maybeShowDiceOverlay(event: String, force: Boolean = false) {
        if (!force && event == lastDiceEvent) return
        if (!looksLikeCombatRoll(event)) return
        lastDiceEvent = event
        showDiceOverlay(event)
    }

    private fun showDiceOverlay(event: String) {
        val root = diceOverlayRoot ?: return
        val d20Match = Regex("""d20=(\d+)""").find(event)
        val dmgMatch = Regex("""(?i)(?:Damage|deal(?:s)?|for)\s+(\d+)""").find(event)
        val miss = event.contains("Miss!", ignoreCase = true)
        val crit = event.contains("CRITICAL", ignoreCase = true)
        diceTitle?.text = when {
            event.contains("Magic Missile", ignoreCase = true) -> "Magic Missile"
            event.contains("Potion", ignoreCase = true) || event.contains("Healing", ignoreCase = true) -> "Healing"
            miss -> "Attack — Miss"
            crit -> "Critical Hit!"
            d20Match != null -> "Attack Roll"
            else -> "Combat"
        }
        if (d20Match != null) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = d20Match.groupValues[1]
        } else if (event.contains("Magic Missile", ignoreCase = true)) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "auto"
        } else if (event.contains("Potion", ignoreCase = true) || event.contains("Healing", ignoreCase = true)) {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "♥"
        } else {
            diceD20?.visibility = View.VISIBLE
            diceD20?.text = "—"
        }
        if (dmgMatch != null && !miss) {
            diceDamage?.visibility = View.VISIBLE
            diceDamage?.text = dmgMatch.groupValues[1]
        } else {
            diceDamage?.visibility = View.GONE
        }
        val summary = event.let { if (it.length > 120) it.take(117) + "…" else it }
        diceSummary?.text = summary
        root.isClickable = false
        root.isFocusable = false
        root.visibility = View.VISIBLE
        root.alpha = 0f
        root.animate().alpha(1f).setDuration(120).start()
        diceHideRunnable?.let { handler.removeCallbacks(it) }
        val holdMs = 1400L
        val hide = Runnable {
            root.animate().alpha(0f).setDuration(200).withEndAction {
                root.visibility = View.GONE
            }.start()
        }
        diceHideRunnable = hide
        handler.postDelayed(hide, holdMs)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUi()
    }


    /** If the DM never syncs / session is missing, stop waiting and return to start. */
    private fun scheduleJoinTimeout(sid: String, resuming: Boolean) {
        joinTimeoutRunnable?.let { handler.removeCallbacks(it) }
        val timeoutMs = 20_000L
        val r = Runnable {
            if (!isOnlineClient) return@Runnable
            val status = try { getPlayerStatus() } catch (_: Exception) { "" }
            val admitted = status.contains(localPlayerName) && status.contains("| HP:")
            val hasParty = try {
                getBattleRoster().substringBefore('|').isNotBlank()
            } catch (_: Exception) { false }
            if (admitted || hasParty) return@Runnable
            Log.w(TAG, "Join timeout for session $sid (resuming=$resuming)")
            Toast.makeText(
                this,
                "Could not reach the DM / session $sid (timed out). Returning to menu.",
                Toast.LENGTH_LONG
            ).show()
            resetToStartMenu()
        }
        joinTimeoutRunnable = r
        handler.postDelayed(r, timeoutMs)
    }

    private fun prefs() = getPreferences(MODE_PRIVATE)

    private fun maybeOfferTutorialThenCoach() {
        val seen = prefs().getBoolean("seen_beginner_tutorial", false)
        if (!seen) {
            showTutorial(0) {
                prefs().edit().putBoolean("seen_beginner_tutorial", true).apply()
                postCoach(
                    "DM: Tutorial done. I'll coach you on your turns. Tap Help anytime. " +
                        BeginnerGuide.specialBlurb(localClassId)
                )
            }
        } else {
            postCoach("DM: Welcome back. Tap Help if you forget what a button does.")
        }
    }

    private fun showHelpMenu() {
        val options = arrayOf(
            "Show beginner tutorial",
            "Action quick reference",
            "Toggle solo DM tips",
            "Settings…"
        )
        AlertDialog.Builder(this)
            .setTitle("Help")
            .setItems(options) { _, which ->
                when (which) {
                    0 -> showTutorial(0) {}
                    1 -> AlertDialog.Builder(this)
                        .setTitle("Actions")
                        .setMessage(BeginnerGuide.actionReference())
                        .setPositiveButton("Got it", null)
                        .show()
                    2 -> {
                        soloCoachEnabled = !soloCoachEnabled
                        val state = if (soloCoachEnabled) "ON" else "OFF"
                        Toast.makeText(this, "Solo DM tips: $state", Toast.LENGTH_SHORT).show()
                        prefs().edit().putBoolean("solo_coach_enabled", soloCoachEnabled).apply()
                    }
                    3 -> showSettingsOverlay()
                }
            }
            .setNegativeButton("Close", null)
            .show()
    }

    private fun showTutorial(pageIndex: Int, onFinished: () -> Unit) {
        if (pageIndex !in BeginnerGuide.tutorialPages.indices) {
            onFinished()
            return
        }
        val page = BeginnerGuide.tutorialPages[pageIndex]
        val last = pageIndex == BeginnerGuide.tutorialPages.lastIndex
        val builder = AlertDialog.Builder(this)
            .setTitle(page.title)
            .setMessage(page.body)
            .setCancelable(false)
        if (last) {
            builder.setPositiveButton("Let's play") { _, _ -> onFinished() }
            builder.setNeutralButton("Back") { _, _ -> showTutorial(pageIndex - 1, onFinished) }
        } else {
            builder.setPositiveButton("Next") { _, _ -> showTutorial(pageIndex + 1, onFinished) }
            if (pageIndex > 0) {
                builder.setNeutralButton("Back") { _, _ -> showTutorial(pageIndex - 1, onFinished) }
            }
            builder.setNegativeButton("Skip") { _, _ -> onFinished() }
        }
        builder.show()
    }

    private fun postCoach(tip: String) {
        if (!nativeReady || tip.isBlank()) return
        if (tip == lastCoachTip) return
        lastCoachTip = tip
        try {
            sendChatMessage("DM", tip)
        } catch (e: Exception) {
            Log.e(TAG, "coach chat failed: ${e.message}")
        }
        // Surface coach tips in the lasting combat feed (not a flicker overwrite)
        appendCombatFeed(tip)
    }

    private fun maybeSoloDmCoach(status: String, isShop: Boolean) {
        if (!soloCoachEnabled || !nativeReady) return
        val turnKey = status.lineSequence().firstOrNull { it.contains("Turn:") } ?: status.take(40)
        if (turnKey == lastCoachTurnKey) return
        lastCoachTurnKey = turnKey
        val special = try { getSpecialName() } catch (_: Exception) { "Special" }
        val tip = BeginnerGuide.turnCoachTip(
            status = status,
            localPlayerName = localPlayerName,
            isShop = isShop,
            specialName = special,
            classId = localClassId
        ) ?: return
        postCoach(tip)
    }

    private fun hideSystemUi() {
        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN)
    }
}
