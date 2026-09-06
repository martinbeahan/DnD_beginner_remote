package com.fintrack.dndbeginnerremote

import android.util.Log
import com.google.firebase.FirebaseApp
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener

class MultiplayerManager(private val sessionId: String) {
    // Firebase is optional until google-services.json is added and the Google Services plugin is enabled.
    private var database: FirebaseDatabase? = try {
        FirebaseDatabase.getInstance()
    } catch (e: Exception) {
        Log.w("Multiplayer", "Firebase unavailable (${e.message}). Continuing in local/single-player mode.")
        null
    }
    
    private val sessionRef = database?.getReference("sessions")?.child(sessionId)

    interface StateUpdateListener {
        fun onStateUpdated(data: String)
    }

    fun updateState(data: String) {
        sessionRef?.child("gameState")?.setValue(data)
    }

    fun listenForUpdates(listener: StateUpdateListener) {
        sessionRef?.child("gameState")?.addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val data = snapshot.getValue(String::class.java)
                if (data != null) {
                    listener.onStateUpdated(data)
                }
            }

            override fun onCancelled(error: DatabaseError) {
                Log.e("Multiplayer", "Update cancelled: ${error.message}")
            }
        })
    }
    
    fun sendChat(message: String) {
        sessionRef?.child("chat")?.push()?.setValue(message)
    }
    
    fun listenForChat(callback: (String) -> Unit) {
        sessionRef?.child("chat")?.addChildEventListener(object : com.google.firebase.database.ChildEventListener {
            override fun onChildAdded(snapshot: DataSnapshot, previousChildName: String?) {
                val msg = snapshot.getValue(String::class.java)
                if (msg != null) callback(msg)
            }
            override fun onChildChanged(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onChildRemoved(snapshot: DataSnapshot) {}
            override fun onChildMoved(snapshot: DataSnapshot, previousChildName: String?) {}
            override fun onCancelled(error: DatabaseError) {}
        })
    }
}
