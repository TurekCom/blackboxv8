package com.turek.blackbox.android

import android.speech.tts.TextToSpeech
import java.util.Locale

object BlackBoxEngine {
    const val VOICE_NAME = "blackbox_pl_pl_v8"
    const val CHECK_VOICE_DATA_NAME = "pol-POL-BLACKBOXV8"
    val LOCALE: Locale = Locale.Builder().setLanguage("pl").setRegion("PL").build()

    private val languageCodes = setOf("pl", "pol")
    private val countryCodes = setOf("", "pl", "pol")

    fun iso3Language(): String = try {
        LOCALE.isO3Language
    } catch (_: Exception) {
        "pol"
    }

    fun iso3Country(): String = try {
        LOCALE.isO3Country
    } catch (_: Exception) {
        "POL"
    }

    fun features(): Set<String> = setOf(TextToSpeech.Engine.KEY_FEATURE_EMBEDDED_SYNTHESIS)

    fun matches(language: String?, country: String?): Boolean {
        val lang = (language ?: "").lowercase(Locale.ROOT)
        val ctr = (country ?: "").lowercase(Locale.ROOT)
        return lang in languageCodes && ctr in countryCodes
    }
}
