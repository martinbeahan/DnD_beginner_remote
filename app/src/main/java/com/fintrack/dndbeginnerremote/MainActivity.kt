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
import androidx.appcompat.app.AlertDialog
import com.google.androidgamesdk.GameActivity
import com.google.firebase.FirebaseApp

class MainActivity : GameActivity() {
    private val TAG = "DnDMain"

    private lateinit var statusText: TextView
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
    
    private var multiplayer: MultiplayerManager? = null
    private val handler = Handler(Looper.getMainLooper())
    private var lastProcessedEvent = ""
    private var lastRoomDesc = ""
    private var lastChatHistory = ""
    private var isUpdatingFromRemote = false
    private var localPlayerName = "Hero"

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
        // Let the native OpenGL battle surface show through the HUD overlay
        window.setBackgroundDrawableResource(android.R.color.transparent)

        
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

        btnAttack.setOnClickListener { 
            Log.d(TAG, "Attack clicked")
            if (isMerchantRoom()) showShopDialog() 
            else handleActionWithTarget("Attack") { i -> doAttack(i) } 
        }
        
        btnSpecial.setOnClickListener { Log.d(TAG, "Special clicked"); handleActionWithTarget("Special") { i -> doSpecial(i) } }
        btnHeal.setOnClickListener { Log.d(TAG, "Heal clicked"); handleActionWithTarget("Heal", false) { i -> doHeal(i) } }
        btnInteract.setOnClickListener { Log.d(TAG, "Interact clicked"); doInteract(localPlayerName); syncAndSave() }
        btnRest.setOnClickListener { Log.d(TAG, "Rest clicked"); doRest(); syncAndSave() }

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
        btnReset.setOnClickListener { showStartDialog() }

        findViewById<View>(R.id.topBar).setOnLongClickListener {
            val sid = getSessionId()
            val clip = android.content.ClipData.newPlainText("DND_SESSION", sid)
            (getSystemService(CLIPBOARD_SERVICE) as ClipboardManager).setPrimaryClip(clip)
            Toast.makeText(this, "Session ID $sid Copied!", Toast.LENGTH_SHORT).show()
            true
        }

        showStartDialog()
        startUiUpdateLoop()
    }

    private fun handleActionWithTarget(title: String, isEnemy: Boolean = true, action: (Int) -> Unit) {
        val status = getPlayerStatus()
        Log.d(TAG, "Targeting for $title. Status: $status")
        val targets = if (isEnemy) {
            status.substringAfter("--- Foes ---", "").lines().filter { it.contains("|") }
        } else {
            status.lines().takeWhile { !it.contains("--- Foes ---") }.filter { it.contains("|") }
        }

        if (targets.isEmpty()) { 
            Log.d(TAG, "No targets found for $title, using index 0")
            action(0); syncAndSave(); return 
        }
        
        AlertDialog.Builder(this).setTitle("Select Target").setItems(targets.toTypedArray()) { _, which ->
            Log.d(TAG, "Target $which selected for $title")
            action(which)
            syncAndSave()
        }.show()
    }

    private fun setupMultiplayer(id: String, isNewPlayer: Boolean = false, selectedClass: Int = 0) {
        Log.d(TAG, "Setting up multiplayer for session: $id")
        multiplayer = MultiplayerManager(id)
        multiplayer?.listenForUpdates(object : MultiplayerManager.StateUpdateListener {
            override fun onStateUpdated(data: String) {
                if (data == saveGameState()) return
                isUpdatingFromRemote = true
                loadGameState(data)
                
                if (isNewPlayer && !getPlayerStatus().contains(localPlayerName)) {
                    addRemoteAlly(selectedClass, localPlayerName)
                    syncAndSave()
                }
                isUpdatingFromRemote = false
            }
        })
        multiplayer?.listenForChat { msg -> if (!getChatHistory().contains(msg)) sendChatMessage("Remote", msg) }
    }

    private fun syncAndSave() {
        val data = saveGameState()
        getPreferences(MODE_PRIVATE).edit().putString("save_state", data).apply()
        if (!isUpdatingFromRemote) multiplayer?.updateState(data)
    }

    private fun showStartDialog() {
        Log.d(TAG, "Showing start dialog")
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(60, 40, 60, 10)
        }
        val nameInput = EditText(this).apply { hint = "Enter Hero Name" }
        layout.addView(nameInput)

        AlertDialog.Builder(this)
            .setTitle("D&D Remote Multiplayer")
            .setView(layout)
            .setCancelable(false)
            .setPositiveButton("New Game") { _, _ ->
                localPlayerName = nameInput.text.toString().ifEmpty { "Hero" }
                showClassSelection(isNewGame = true)
            }
            .setNegativeButton("Join Session") { _, _ ->
                localPlayerName = nameInput.text.toString().ifEmpty { "Hero" }
                showJoinDialog()
            }
            .show()
    }

    private fun showClassSelection(isNewGame: Boolean, sid: String = "") {
        val classes = arrayOf("Fighter", "Wizard", "Rogue", "Cleric")
        AlertDialog.Builder(this).setTitle("Choose Your Role").setItems(classes) { _, which ->
            Log.d(TAG, "Class $which selected. NewGame=$isNewGame")
            if (isNewGame) {
                resetGame(which, localPlayerName)
                setHost(true)
                setupMultiplayer(getSessionId())
            } else {
                setHost(false)
                setupMultiplayer(sid, isNewPlayer = true, selectedClass = which)
            }
            btnReset.visibility = View.GONE
            syncAndSave()
            updateUi()
        }.show()
    }

    private fun showJoinDialog() {
        val input = EditText(this).apply { hint = "Enter Session ID (e.g. DND-A1B2)" }
        AlertDialog.Builder(this).setTitle("Join Ally").setView(input).setPositiveButton("Join") { _, _ ->
            val sid = input.text.toString().uppercase()
            if (sid.isNotEmpty()) showClassSelection(isNewGame = false, sid = sid)
        }.setNegativeButton("Back") { _, _ -> showStartDialog() }.show()
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
                doBuyItem(localPlayerName, index)
                syncAndSave()
                Toast.makeText(this, "Bought ${labels[which]}", Toast.LENGTH_SHORT).show()
            }
            .setNegativeButton("Leave", null)
            .show()
    }

    private fun showJournal() = AlertDialog.Builder(this).setTitle("Journal").setMessage(getJournal()).setPositiveButton("Close", null).show()

    private fun startUiUpdateLoop() {
        handler.post(object : Runnable {
            override fun run() {
                processGameTurn()
                updateUi()
                handler.postDelayed(this, 300)
            }
        })
    }

    private fun updateUi() {
        val status = getPlayerStatus()
        val isShop = isMerchantRoom()
        
        statusText.text = status
        btnSpecial.text = if (isShop) "Status" else getSpecialName()
        btnAttack.text = if (isShop) "Shop" else "Attack"
        btnInteract.visibility = if (isShop) View.GONE else View.VISIBLE
        
        val isMyTurn = isShop || status.contains("Turn: $localPlayerName") || status.contains("Turn: You")
        val isGameOver = status.contains("Game Over")

        btnAttack.isEnabled = isMyTurn && !isGameOver
        btnSpecial.isEnabled = isMyTurn && !isGameOver
        btnHeal.isEnabled = isMyTurn && !isGameOver
        btnRest.isEnabled = isMyTurn && !isGameOver

        val roomDesc = getRoomDescription()
        if (roomDesc != lastRoomDesc) { roomDescText.text = roomDesc; lastRoomDesc = roomDesc }
        
        val chat = getChatHistory()
        if (chat != lastChatHistory) { logText.text = chat; lastChatHistory = chat }
        
        val lastEv = getLastEvent()
        if (lastEv.isNotEmpty() && lastEv != lastProcessedEvent) {
            if (!chat.contains(lastEv)) logText.text = "$lastEv\n${logText.text}"
            lastProcessedEvent = lastEv
            if (lastEv.contains("Game Over")) btnReset.visibility = View.VISIBLE
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUi()
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
