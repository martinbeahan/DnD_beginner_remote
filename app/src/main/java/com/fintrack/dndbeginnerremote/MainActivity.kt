package com.fintrack.dndbeginnerremote

import android.content.ClipboardManager
import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import android.graphics.Color
import android.view.Gravity
import android.widget.ImageView
import android.widget.ProgressBar
import android.view.animation.AccelerateDecelerateInterpolator
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
    private lateinit var partyColumn: LinearLayout
    private lateinit var enemyColumn: LinearLayout
    
    private var multiplayer: MultiplayerManager? = null
    private var lastBattleRoster = ""
    private var lastAnimEvent = ""
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
        partyColumn = findViewById(R.id.partyColumn)
        enemyColumn = findViewById(R.id.enemyColumn)

        btnAttack.setOnClickListener { 
            Log.d(TAG, "Attack clicked")
            if (isMerchantRoom()) showShopDialog() 
            else handleActionWithTarget("Attack", isEnemy = true, actionType = "attack") { i ->
                doAttack(i); animateAttack(true)
            }
        }
        
        btnSpecial.setOnClickListener {
            Log.d(TAG, "Special clicked")
            handleActionWithTarget("Special", isEnemy = true, actionType = "special") { i ->
                doSpecial(i); animateAttack(true)
            }
        }
        btnHeal.setOnClickListener {
            Log.d(TAG, "Heal clicked")
            handleActionWithTarget("Heal", isEnemy = false, actionType = "heal") { i -> doHeal(i) }
        }
        btnInteract.setOnClickListener {
            Log.d(TAG, "Interact clicked")
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
        btnReset.setOnClickListener { resetToStartMenu() }

        findViewById<View>(R.id.topBar).setOnLongClickListener {
            val sid = getSessionId()
            val clip = android.content.ClipData.newPlainText("DND_SESSION", sid)
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
            Toast.makeText(this, "Session ID $sid Copied!", Toast.LENGTH_SHORT).show()
            true
        }

        soloCoachEnabled = prefs().getBoolean("solo_coach_enabled", true)

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
        super.onDestroy()
    }

    override fun onPause() {
        super.onPause()
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

    private fun handleActionWithTarget(
        title: String,
        isEnemy: Boolean = true,
        actionType: String,
        action: (Int) -> Unit
    ) {
        val status = getPlayerStatus()
        Log.d(TAG, "Targeting for $title. Status: $status")
        val targets = if (isEnemy) {
            status.substringAfter("--- Foes ---", "").lines().filter { it.contains("|") }
        } else {
            status.lines().takeWhile { !it.contains("--- Foes ---") }.filter { it.contains("|") }
        }

        if (targets.isEmpty()) {
            Log.d(TAG, "No targets found for $title, using index 0")
            performOrQueue(actionType, 0) { action(0) }
            return
        }

        AlertDialog.Builder(this).setTitle("Select Target").setItems(targets.toTypedArray()) { _, which ->
            Log.d(TAG, "Target $which selected for $title")
            performOrQueue(actionType, which) { action(which) }
        }.show()
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
                animateAttack(true)
            }
            "special" -> {
                if (!validateActorOrReject(player)) return
                doSpecial(target)
                animateAttack(true)
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
                doInteract(player)
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
        Log.d(TAG, "Showing start dialog")
        // Mode switch / Reset: drop any live Firebase listeners first.
        sessionActive = false
        detachOnlineSession()
        val saved = livableSavedState()
        val modes = mutableListOf<String>()
        if (saved != null) {
            val wasHost = prefs().getBoolean("was_online_host", false)
            val wasClient = prefs().getBoolean("was_online_client", false)
            val sid = prefs().getString("online_session_id", "")?.trim().orEmpty()
            val tag = when {
                wasHost && sid.isNotBlank() -> "Host $sid"
                wasClient && sid.isNotBlank() -> "Join $sid"
                else -> "solo"
            }
            modes += "Continue ($tag) — ${heroNameFromSave(saved)}"
        }
        modes += "Solo adventure"
        modes += "Host online (you are the DM)"
        modes += "Join session"

        AlertDialog.Builder(this)
            .setTitle("D&D Beginner")
            .setCancelable(false)
            .setItems(modes.toTypedArray()) { _, which ->
                val label = modes[which]
                when {
                    label.startsWith("Continue") -> continueSavedGame()
                    label.startsWith("Solo") -> askHeroName { name ->
                        localPlayerName = name
                        showClassSelection(mode = "solo")
                    }
                    label.startsWith("Host") -> {
                        if (!firebaseReady()) {
                            showFirebaseRequiredDialog { showStartDialog() }
                            return@setItems
                        }
                        askHeroName(hint = "Your DM name") { name ->
                            localPlayerName = name
                            startHostingAsDm()
                        }
                    }
                    label.startsWith("Join") -> {
                        if (!firebaseReady()) {
                            showFirebaseRequiredDialog { showStartDialog() }
                            return@setItems
                        }
                        askHeroName { name ->
                            localPlayerName = name
                            showJoinDialog()
                        }
                    }
                }
            }
            .show()
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
        val sheet = getDetailedSheet(localPlayerName)
        val builder = AlertDialog.Builder(this).setTitle("Hero Sheet").setMessage(sheet).setPositiveButton("Close", null)
        if (sheet.contains("POINTS TO SPEND")) {
            builder.setNeutralButton("Level Up!") { _, _ -> showStatUpgradeDialog() }
        }
        builder.show()
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
        val labels = entries.map { entry ->
            val parts = entry.split(':', limit = 2)
            if (parts.size == 2) parts[1] else entry
        }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Merchant")
            .setItems(labels) { _, which ->
                val entry = entries[which]
                val index = entry.substringBefore(':').toIntOrNull() ?: which
                if (isOnlineClient) {
                    performOrQueue("buy", index) { }
                    // performOrQueue already toasts "Sent to the DM…"
                } else {
                    val ok = try { doBuyItem(localPlayerName, index) } catch (_: Exception) { false }
                    if (ok) {
                        syncAndSave()
                        Toast.makeText(this, "Bought ${labels[which]}", Toast.LENGTH_SHORT).show()
                        updateUi()
                    } else {
                        val ev = try { getLastEvent() } catch (_: Exception) { "Purchase failed." }
                        Toast.makeText(this, ev.ifBlank { "Could not buy that." }, Toast.LENGTH_SHORT).show()
                    }
                }
            }
            .setNegativeButton("Leave", null)
            .show()
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
            isShop -> "Safe haven — merchant"
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
        btnSpecial.text = if (isShop) "Info" else "✦\n$specialName"
        btnAttack.text = if (isShop) "🛒\nShop" else "⚔\nAttack"
        btnHeal.text = "⚗\nPotion"
        btnRest.text = if (isShop) "Leave" else "🌙\nRest"
        btnInteract.visibility = if (isShop) View.GONE else View.VISIBLE

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
        val isMyTurn = !isGameOver && !localDown && (
            isShop || status.contains("Turn: $localPlayerName") || status.contains("Turn: You")
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
            btnAttack.isEnabled = isMyTurn
            btnSpecial.isEnabled = isMyTurn
            btnHeal.isEnabled = isMyTurn
            // Short Rest only out of combat (Leave merchant still allowed on Rest button).
            btnRest.isEnabled = isMyTurn && (isShop || !inCombat)
            btnInteract.isEnabled = isMyTurn || (isShop && !isGameOver)
        }

        val roomDesc = getRoomDescription()
        if (roomDesc != lastRoomDesc) {
            roomDescText.text = roomDesc
            lastRoomDesc = roomDesc
        }

        val chat = getChatHistory()
        if (chat != lastChatHistory) {
            lastChatHistory = chat
            if (chat.isNotBlank()) {
                logText.text = chat.lineSequence().lastOrNull().orEmpty()
            }
        }

        val lastEv = getLastEvent()
        if (lastEv.isNotEmpty() && lastEv != lastProcessedEvent) {
            logText.text = lastEv
            lastProcessedEvent = lastEv
            if (lastEv.contains("Game Over", ignoreCase = true)) {
                btnReset.visibility = View.VISIBLE
                clearSave("game over event")
            }
            maybeAnimateFromEvent(lastEv)
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
        if (roster == lastBattleRoster) return
        lastBattleRoster = roster
        val (party, foes) = parseBattleRoster(roster)
        rebuildColumn(partyColumn, party, alignEnd = false)
        rebuildColumn(enemyColumn, foes, alignEnd = true)
    }

    private fun rebuildColumn(column: LinearLayout, units: List<BattleUnit>, alignEnd: Boolean) {
        column.removeAllViews()
        val density = resources.displayMetrics.density
        val spriteSize = (104 * density).toInt()
        val barW = (88 * density).toInt()
        val barH = (10 * density).toInt()
        for (unit in units) {
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
            column.addView(wrap)
        }
    }

    private fun findSpriteByName(column: LinearLayout, name: String): ImageView? {
        for (i in 0 until column.childCount) {
            val child = column.getChildAt(i)
            if (child.tag == name) {
                return child.findViewWithTag("sprite") as? ImageView
            }
        }
        // fallback: first sprite in column
        if (column.childCount > 0) {
            return column.getChildAt(0).findViewWithTag("sprite") as? ImageView
        }
        return null
    }

    private fun firstSprite(column: LinearLayout): ImageView? {
        if (column.childCount == 0) return null
        return column.getChildAt(0).findViewWithTag("sprite") as? ImageView
    }

    private fun animateAttack(attackerIsPlayer: Boolean) {
        val attackerCol = if (attackerIsPlayer) partyColumn else enemyColumn
        val defenderCol = if (attackerIsPlayer) enemyColumn else partyColumn
        val a = firstSprite(attackerCol) ?: return
        val defendView = firstSprite(defenderCol)
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
            e.contains("attacks") || e.contains("hits") || e.contains("slashes") || e.contains("fireball") || e.contains("sneak") || e.contains("action surge") -> {
                val playerSide = e.contains(localPlayerName.lowercase()) || e.contains("you ") || e.contains("party")
                // If event mentions goblin/skeleton attacking, enemy side
                val enemyAttack = e.contains("goblin") || e.contains("skeleton")
                animateAttack(attackerIsPlayer = !(enemyAttack && !playerSide))
            }
            e.contains("damage") || e.contains("struck") || e.contains("wounded") -> {
                animateAttack(attackerIsPlayer = true)
            }
        }
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
        val options = arrayOf("Show beginner tutorial", "Action quick reference", "Toggle solo DM tips")
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
        // Also surface in the on-screen log immediately
        logText.text = "$tip\n${logText.text}"
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
