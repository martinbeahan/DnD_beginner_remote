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
    external fun doBuyItem(playerName: String, itemIndex: Int)
    external fun setHost(isHost: Boolean)
    external fun addRemoteAlly(characterClass: Int, playerName: String)
    external fun sendChatMessage(sender: String, message: String)
    external fun processGameTurn()
    external fun resetGame(characterClass: Int, playerName: String)
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

        btnSheet.setOnClickListener { showCharacterSheet() }
        btnJournal.setOnClickListener { showJournal() }
        btnHelp.setOnClickListener { showHelpMenu() }
        btnReset.setOnClickListener { showStartDialog() }

        findViewById<View>(R.id.topBar).setOnLongClickListener {
            val sid = getSessionId()
            val clip = android.content.ClipData.newPlainText("DND_SESSION", sid)
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
            Toast.makeText(this, "Session ID $sid Copied!", Toast.LENGTH_SHORT).show()
            true
        }

        soloCoachEnabled = prefs().getBoolean("solo_coach_enabled", true)

        // If the last run died mid-frame, drop the save so Continue can't boot-loop a bad state.
        if (prefs().getBoolean("crash_guard", false)) {
            Log.w(TAG, "Previous run did not exit cleanly — clearing save_state")
            prefs().edit()
                .remove("save_state")
                .putBoolean("crash_guard", false)
                .apply()
            Toast.makeText(this, "Cleared a broken save from a previous crash.", Toast.LENGTH_LONG).show()
        } else {
            prefs().edit().putBoolean("crash_guard", true).apply()
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
            if (data.isNotBlank() && data.contains("|")) {
                prefs().edit()
                    .putString("save_state", data)
                    .putString("hero_name", localPlayerName)
                    .putInt("hero_class", localClassId)
                    .commit()
            }
        } catch (e: Exception) {
            Log.e(TAG, "onPause save failed", e)
        }
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
                    "1. Add app/google-services.json from the Firebase console\n" +
                    "2. Uncomment the Google Services plugin in app/build.gradle.kts\n" +
                    "3. Enable Realtime Database (see FIREBASE_SETUP.md)\n" +
                    "4. Clean + Rebuild\n\n" +
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

    private fun applyHostAction(action: Map<String, Any>) {
        val type = action["type"]?.toString() ?: return
        val player = action["playerName"]?.toString() ?: "Hero"
        val target = (action["targetIndex"] as? Number)?.toInt()
            ?: action["targetIndex"]?.toString()?.toIntOrNull()
            ?: 0
        val classId = (action["classId"] as? Number)?.toInt()
            ?: action["classId"]?.toString()?.toIntOrNull()
            ?: 0

        when (type) {
            "join" -> {
                if (!getPlayerStatus().contains(player)) {
                    addRemoteAlly(classId, player)
                    try { sendChatMessage("DM", "$player has joined the party.") } catch (_: Exception) {}
                    Toast.makeText(this, "$player joined the table.", Toast.LENGTH_SHORT).show()
                }
            }
            "attack" -> {
                doAttack(target)
                animateAttack(true)
            }
            "special" -> {
                doSpecial(target)
                animateAttack(true)
            }
            "heal" -> doHeal(target)
            "rest" -> doRest()
            "interact" -> doInteract(player)
            "buy" -> doBuyItem(player, target)
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
        val data = livableSavedState() ?: run {
            Toast.makeText(this, "No living adventure to continue — start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        localPlayerName = heroNameFromSave(data)
        localClassId = prefs().getInt("hero_class", 0)
        try {
            loadGameState(data)
            setHost(true)
            isOnlineHost = false
            isOnlineClient = false
            multiplayer = null
            onlineSessionId = ""
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load save", e)
            Toast.makeText(this, "Save looked broken — start a new game.", Toast.LENGTH_LONG).show()
            prefs().edit().remove("save_state").apply()
            showStartDialog()
            return
        }
        sessionActive = true
        lastBattleRoster = ""
        lastRoomDesc = ""
        lastChatHistory = ""
        lastProcessedEvent = ""
        lastCoachTip = ""
        lastCoachTurnKey = ""
        val status = try { getPlayerStatus() } catch (_: Exception) { "" }
        if (!status.contains(localPlayerName) && !status.contains("| HP:")) {
            Toast.makeText(this, "Save looked empty — starting fresh is safer.", Toast.LENGTH_LONG).show()
            prefs().edit().remove("save_state").apply()
            sessionActive = false
            showStartDialog()
            return
        }
        if (status.contains("Game Over", ignoreCase = true) || saveIsGameOver(data)) {
            clearSave("continued into game over")
            sessionActive = false
            Toast.makeText(this, "That adventure already ended. Start a new one.", Toast.LENGTH_LONG).show()
            showStartDialog()
            return
        }
        btnReset.visibility = View.GONE
        refreshBattleArena()
        updateUi()
        Toast.makeText(this, "Welcome back, $localPlayerName (solo)", Toast.LENGTH_SHORT).show()
    }


    private fun showStartDialog() {
        Log.d(TAG, "Showing start dialog")
        val saved = livableSavedState()
        val modes = mutableListOf<String>()
        if (saved != null) modes += "Continue solo save (${heroNameFromSave(saved)})"
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
                            // Don't reopen the menu underneath — wait for OK
                            showFirebaseRequiredDialog { showStartDialog() }
                            return@setItems
                        }
                        askHeroName(hint = "DM / host name") { name ->
                            localPlayerName = name
                            showClassSelection(mode = "host")
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

    private fun showClassSelection(mode: String, sid: String = "") {
        val classes = arrayOf("Fighter", "Wizard", "Rogue", "Cleric")
        val title = when (mode) {
            "host" -> "DM character (you still sit at the table)"
            "join" -> "Choose your class"
            else -> "Choose your class"
        }
        AlertDialog.Builder(this).setTitle(title).setItems(classes) { _, which ->
            Log.d(TAG, "Class $which selected. mode=$mode")
            localClassId = which
            when (mode) {
                "solo" -> {
                    resetGame(which, localPlayerName)
                    setHost(true)
                    isOnlineHost = false
                    isOnlineClient = false
                    multiplayer = null
                    onlineSessionId = ""
                    maybeOfferTutorialThenCoach()
                    sessionActive = true
                    btnReset.visibility = View.GONE
                    syncAndSave()
                    updateUi()
                }
                "host" -> {
                    resetGame(which, localPlayerName)
                    setHost(true)
                    sessionActive = true
                    btnReset.visibility = View.GONE
                    val sidNow = try { getSessionId() } catch (_: Exception) { "" }
                    setupMultiplayer(sidNow, asHost = true)
                    if (!isOnlineHost || multiplayer?.isAvailable != true) {
                        Toast.makeText(
                            this,
                            "Could not start online session — check FIREBASE_SETUP.md",
                            Toast.LENGTH_LONG
                        ).show()
                        isOnlineHost = false
                        multiplayer = null
                        showFirebaseRequiredDialog {
                            sessionActive = false
                            showStartDialog()
                        }
                        return@setItems
                    }
                    syncAndSave()
                    updateUi()
                    maybeOfferTutorialThenCoach()
                    showSessionShareDialog()
                }
                "join" -> {
                    setHost(false)
                    sessionActive = true
                    btnReset.visibility = View.GONE
                    setupMultiplayer(sid, asHost = false)
                    // Ask the DM to add us to the party
                    multiplayer?.pushAction(
                        mapOf(
                            "type" to "join",
                            "playerName" to localPlayerName,
                            "classId" to which,
                            "targetIndex" to 0
                        )
                    )
                    updateUi()
                    Toast.makeText(this, "Joining $sid — waiting for the DM…", Toast.LENGTH_LONG).show()
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
                    "Share this code with friends. They tap Join session and enter it.\n\n" +
                    "You control the world (enemy turns & saves). Their actions are sent to you.\n\n" +
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
            doIncreaseStat(localPlayerName, which)
            syncAndSave()
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
                performOrQueue("buy", index) { doBuyItem(localPlayerName, index) }
                Toast.makeText(this, "Bought ${labels[which]}", Toast.LENGTH_SHORT).show()
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
            isOnlineHost && isShop -> "DM · merchant"
            isOnlineHost && status.contains("Game Over", ignoreCase = true) -> "DM · defeat…"
            isOnlineHost && myTurnBanner -> "DM · your move"
            isOnlineHost -> "DM · $whose"
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

        val isGameOver = status.contains("Game Over", ignoreCase = true)
        val localDown = status.lineSequence().any {
            it.contains(localPlayerName) && it.contains("[DOWN]", ignoreCase = true)
        }
        val isMyTurn = !isGameOver && !localDown && (
            isShop || status.contains("Turn: $localPlayerName") || status.contains("Turn: You")
        )

        btnAttack.isEnabled = isMyTurn
        btnSpecial.isEnabled = isMyTurn
        btnHeal.isEnabled = isMyTurn
        btnRest.isEnabled = isMyTurn
        btnInteract.isEnabled = isMyTurn || (isShop && !isGameOver)

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
