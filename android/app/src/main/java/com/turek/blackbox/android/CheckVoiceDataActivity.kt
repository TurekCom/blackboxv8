package com.turek.blackbox.android

import android.content.Intent
import android.os.Bundle
import android.speech.tts.TextToSpeech
import androidx.activity.ComponentActivity

class CheckVoiceDataActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val data = Intent().apply {
            putStringArrayListExtra(
                TextToSpeech.Engine.EXTRA_AVAILABLE_VOICES,
                arrayListOf(BlackBoxEngine.CHECK_VOICE_DATA_NAME),
            )
            putStringArrayListExtra(
                TextToSpeech.Engine.EXTRA_UNAVAILABLE_VOICES,
                arrayListOf(),
            )
            putExtra(
                TextToSpeech.Engine.EXTRA_VOICE_DATA_ROOT_DIRECTORY,
                filesDir.absolutePath,
            )
            putExtra(
                TextToSpeech.Engine.EXTRA_TTS_DATA_INSTALLED,
                true,
            )
        }
        setResult(TextToSpeech.Engine.CHECK_VOICE_DATA_PASS, data)
        finish()
    }
}
