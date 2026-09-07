package com.fintrack.dndbeginnerremote

import android.util.Log
import com.google.firebase.database.ChildEventListener
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ServerValue
import com.google.firebase.database.ValueEventListener

/**
 * Firebase Realtime Database session bridge.
 * Requires app/google-services.json + the Google Services plugin (see FIREBASE_SETUP.md).
 *
 * Path layout:
 *   sessions/{sessionId}/meta
 *   sessions/{sessionId}/gameState   (host-authored blob)
 *   sessions/{sessionId}/actions/{id} (client intents; host consumes)
 *   sessions/{sessionId}/chat/{id}
 */
class MultiplayerManager(private val sessionId: String) {
    private val tag = "Multiplayer"

    private var database: FirebaseDatabase? = try {
        FirebaseDatabase.getInstance().also {
            try { it.setPersistenceEnabled(false) } catch (_: Exception) { /* already configured */ }
        }
    } catch (e: Exception) {
        Log.w(tag, "Firebase unavailable (${e.message}). Online play disabled until google-services.json is set up.")
        null
    }

    private val sessionRef = database?.getReference("sessions")?.child(sessionId)

    private var stateListener: ValueEventListener? = null
    private var actionListener: ChildEventListener? = null
    private var chatListener: ChildEventListener? = null

    val isAvailable: Boolean get() = sessionRef != null

    fun publishMeta(hostName: String, role: String = "dm") {
        val meta = mapOf(
            "hostName" to hostName,
            "role" to role,
            "sessionId" to sessionId,
            "updatedAt" to ServerValue.TIMESTAMP
        )
        sessionRef?.child("meta")?.updateChildren(meta)
    }

    fun updateState(data: String) {
        sessionRef?.child("gameState")?.setValue(data)
            ?.addOnFailureListener { Log.e(tag, "updateState failed: ${it.message}") }
    }

    fun listenForUpdates(onUpdate: (String) -> Unit) {
        val ref = sessionRef?.child("gameState") ?: return
        stateListener?.let { ref.removeEventListener(it) }
        val listener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val data = snapshot.getValue(String::class.java)
                if (!data.isNullOrBlank()) onUpdate(data)
            }
            override fun onCancelled(error: DatabaseError) {
                Log.e(tag, "state cancelled: ${error.message}")
            }
        }
        stateListener = listener
        ref.addValueEventListener(listener)
    }

    fun pushAction(action: Map<String, Any>) {
        val payload = action.toMutableMap()
        payload["ts"] = ServerValue.TIMESTAMP
        sessionRef?.child("actions")?.push()?.setValue(payload)
            ?.addOnFailureListener { Log.e(tag, "pushAction failed: ${it.message}") }
    }

    fun listenForActions(onAction: (actionId: String, action: Map<String, Any>) -> Unit) {
        val ref = sessionRef?.child("actions") ?: return
        actionListener?.let { ref.removeEventListener(it) }
        val listener = object : ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                val id = snapshot.key ?: return
                @Suppress("UNCHECKED_CAST")
                val raw = snapshot.value as? Map<String, Any> ?: return
                onAction(id, raw)
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {
                Log.e(tag, "actions cancelled: ${error.message}")
            }
        }
        actionListener = listener
        ref.addChildEventListener(listener)
    }

    fun ackAction(actionId: String) {
        sessionRef?.child("actions")?.child(actionId)?.removeValue()
    }

    fun sendChat(message: String) {
        sessionRef?.child("chat")?.push()?.setValue(message)
    }

    fun listenForChat(callback: (String) -> Unit) {
        val ref = sessionRef?.child("chat") ?: return
        chatListener?.let { ref.removeEventListener(it) }
        val listener = object : ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                val msg = snapshot.getValue(String::class.java)
                if (msg != null) callback(msg)
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {}
        }
        chatListener = listener
        ref.addChildEventListener(listener)
    }

    fun detach() {
        stateListener?.let { sessionRef?.child("gameState")?.removeEventListener(it) }
        actionListener?.let { sessionRef?.child("actions")?.removeEventListener(it) }
        chatListener?.let { sessionRef?.child("chat")?.removeEventListener(it) }
        stateListener = null
        actionListener = null
        chatListener = null
    }
}
