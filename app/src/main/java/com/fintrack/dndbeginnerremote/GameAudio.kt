package com.fintrack.dndbeginnerremote

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.util.Log
import java.util.Locale

/**
 * Copyright-safe BGM (MediaPlayer), SFX (SoundPool), and optional on-device TTS for DM lines.
 * All playback is fire-and-forget; never blocks the UI thread beyond lightweight calls.
 */
class GameAudio(private val context: Context) : TextToSpeech.OnInitListener {
    companion object {
        private const val TAG = "GameAudio"
    }

    enum class Track { EXPLORE, TENSION }

    private var musicEnabled = true
    private var sfxEnabled = true
    private var dmVoiceEnabled = false

    private var bgm: MediaPlayer? = null
    private var currentTrack: Track? = null
    private var pausedForLifecycle = false

    private var soundPool: SoundPool? = null
    private var sfxAttack = 0
    private var sfxHit = 0
    private var sfxGrowl = 0
    private var sfxUiClick = 0
    private var sfxReady = false

    private var tts: TextToSpeech? = null
    private var ttsReady = false
    private var lastSpokenDmLine: String? = null

    fun start(musicOn: Boolean, sfxOn: Boolean, dmVoiceOn: Boolean) {
        musicEnabled = musicOn
        sfxEnabled = sfxOn
        dmVoiceEnabled = dmVoiceOn
        initSfx()
        if (dmVoiceEnabled) ensureTts()
        if (musicEnabled) playTrack(Track.EXPLORE)
    }

    fun release() {
        stopBgm()
        soundPool?.release()
        soundPool = null
        sfxReady = false
        tts?.stop()
        tts?.shutdown()
        tts = null
        ttsReady = false
    }

    fun setMusicEnabled(enabled: Boolean) {
        musicEnabled = enabled
        if (!enabled) {
            stopBgm()
        } else if (!pausedForLifecycle) {
            playTrack(currentTrack ?: Track.EXPLORE)
        }
    }

    fun setSfxEnabled(enabled: Boolean) {
        sfxEnabled = enabled
    }

    fun setDmVoiceEnabled(enabled: Boolean) {
        dmVoiceEnabled = enabled
        if (enabled) {
            ensureTts()
        } else {
            try {
                tts?.stop()
            } catch (_: Exception) {
            }
        }
    }

    /** Pause looping BGM when the activity leaves the foreground. */
    fun onPause() {
        pausedForLifecycle = true
        try {
            bgm?.let { if (it.isPlaying) it.pause() }
        } catch (e: Exception) {
            Log.w(TAG, "bgm pause failed", e)
        }
        try {
            tts?.stop()
        } catch (_: Exception) {
        }
    }

    /** Resume BGM if music is still enabled. */
    fun onResume() {
        pausedForLifecycle = false
        if (!musicEnabled) return
        try {
            val player = bgm
            if (player != null) {
                if (!player.isPlaying) player.start()
            } else {
                playTrack(currentTrack ?: Track.EXPLORE)
            }
        } catch (e: Exception) {
            Log.w(TAG, "bgm resume failed", e)
            playTrack(currentTrack ?: Track.EXPLORE)
        }
    }

    fun syncCombatMusic(inCombat: Boolean) {
        if (!musicEnabled || pausedForLifecycle) return
        playTrack(if (inCombat) Track.TENSION else Track.EXPLORE)
    }

    fun playAttack() = playSfx(sfxAttack)
    fun playHit() = playSfx(sfxHit)
    fun playGrowl() = playSfx(sfxGrowl)
    fun playUiClick() = playSfx(sfxUiClick)

    /**
     * Speak new [DM]: chat lines when DM voice is enabled.
     * Silently no-ops if TTS is missing or not ready.
     */
    fun maybeSpeakDmFromChat(chatHistory: String) {
        if (!dmVoiceEnabled) return
        ensureTts()
        if (!ttsReady) return
        val lines = chatHistory.lineSequence()
            .map { it.trim() }
            .filter { it.startsWith("[DM]:") }
            .map { it.removePrefix("[DM]:").trim() }
            .filter { it.isNotEmpty() }
            .toList()
        if (lines.isEmpty()) return
        val newest = lines.last()
        if (newest == lastSpokenDmLine) return
        lastSpokenDmLine = newest
        speak(newest)
    }

    fun speakNarrator(line: String) {
        if (!dmVoiceEnabled || line.isBlank()) return
        ensureTts()
        if (!ttsReady) return
        lastSpokenDmLine = line
        speak(line)
    }

    override fun onInit(status: Int) {
        ttsReady = status == TextToSpeech.SUCCESS
        if (!ttsReady) {
            Log.i(TAG, "TTS unavailable; DM voice will be skipped")
            return
        }
        try {
            tts?.language = Locale.US
        } catch (_: Exception) {
            ttsReady = false
        }
    }

    private fun ensureTts() {
        if (tts != null) return
        try {
            tts = TextToSpeech(context.applicationContext, this)
        } catch (e: Exception) {
            Log.w(TAG, "TTS init failed", e)
            tts = null
            ttsReady = false
        }
    }

    private fun speak(text: String) {
        val engine = tts ?: return
        try {
            engine.speak(text, TextToSpeech.QUEUE_FLUSH, Bundle(), "dm_${text.hashCode()}")
        } catch (e: Exception) {
            Log.w(TAG, "TTS speak failed", e)
        }
    }

    private fun initSfx() {
        if (soundPool != null) return
        try {
            val attrs = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build()
            val pool = SoundPool.Builder()
                .setMaxStreams(4)
                .setAudioAttributes(attrs)
                .build()
            sfxAttack = pool.load(context, R.raw.sfx_attack, 1)
            sfxHit = pool.load(context, R.raw.sfx_hit, 1)
            sfxGrowl = pool.load(context, R.raw.sfx_growl, 1)
            sfxUiClick = pool.load(context, R.raw.sfx_ui_click, 1)
            soundPool = pool
            sfxReady = true
        } catch (e: Exception) {
            Log.e(TAG, "SFX init failed", e)
            soundPool = null
            sfxReady = false
        }
    }

    private fun playSfx(soundId: Int) {
        if (!sfxEnabled || !sfxReady || soundId == 0) return
        try {
            soundPool?.play(soundId, 0.85f, 0.85f, 1, 0, 1f)
        } catch (e: Exception) {
            Log.w(TAG, "SFX play failed", e)
        }
    }

    private fun playTrack(track: Track) {
        if (!musicEnabled) return
        if (currentTrack == track && bgm != null) {
            try {
                if (bgm?.isPlaying == false && !pausedForLifecycle) bgm?.start()
                return
            } catch (_: Exception) {
                // recreate below
            }
        }
        stopBgm()
        val resId = when (track) {
            Track.EXPLORE -> R.raw.bgm_explore
            Track.TENSION -> R.raw.bgm_tension
        }
        try {
            val player = MediaPlayer.create(context, resId) ?: return
            player.isLooping = true
            player.setVolume(0.45f, 0.45f)
            if (!pausedForLifecycle) player.start()
            bgm = player
            currentTrack = track
        } catch (e: Exception) {
            Log.e(TAG, "BGM start failed", e)
            bgm = null
            currentTrack = null
        }
    }

    private fun stopBgm() {
        try {
            bgm?.stop()
        } catch (_: Exception) {
        }
        try {
            bgm?.release()
        } catch (_: Exception) {
        }
        bgm = null
    }
}
