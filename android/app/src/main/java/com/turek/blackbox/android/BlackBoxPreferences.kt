package com.turek.blackbox.android

import android.content.Context

data class EngineSettings(
    val ratePercent: Int = 50,
    val pitchPercent: Int = 50,
    val volumePercent: Int = 100,
    val modulationPercent: Int = 50,
    val speakEmoji: Boolean = true,
    val dictionaryName: String? = null,
)

object BlackBoxPreferences {
    private const val PREFS_NAME = "blackbox_android_settings"
    private const val KEY_RATE = "rate_percent"
    private const val KEY_PITCH = "pitch_percent"
    private const val KEY_VOLUME = "volume_percent"
    private const val KEY_MODULATION = "modulation_percent"
    private const val KEY_SPEAK_EMOJI = "speak_emoji"
    private const val KEY_DICTIONARY_NAME = "dictionary_name"

    fun load(context: Context): EngineSettings {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return EngineSettings(
            ratePercent = prefs.getInt(KEY_RATE, 50).coerceIn(0, 100),
            pitchPercent = prefs.getInt(KEY_PITCH, 50).coerceIn(0, 100),
            volumePercent = prefs.getInt(KEY_VOLUME, 100).coerceIn(0, 100),
            modulationPercent = prefs.getInt(KEY_MODULATION, 50).coerceIn(0, 100),
            speakEmoji = prefs.getBoolean(KEY_SPEAK_EMOJI, true),
            dictionaryName = prefs.getString(KEY_DICTIONARY_NAME, null),
        )
    }

    fun save(context: Context, settings: EngineSettings) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_RATE, settings.ratePercent.coerceIn(0, 100))
            .putInt(KEY_PITCH, settings.pitchPercent.coerceIn(0, 100))
            .putInt(KEY_VOLUME, settings.volumePercent.coerceIn(0, 100))
            .putInt(KEY_MODULATION, settings.modulationPercent.coerceIn(0, 100))
            .putBoolean(KEY_SPEAK_EMOJI, settings.speakEmoji)
            .putString(KEY_DICTIONARY_NAME, settings.dictionaryName)
            .apply()
    }
}
