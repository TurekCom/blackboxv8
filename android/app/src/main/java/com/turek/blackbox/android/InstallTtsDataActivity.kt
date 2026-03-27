package com.turek.blackbox.android

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.speech.tts.TextToSpeech
import androidx.activity.ComponentActivity

class InstallTtsDataActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        sendBroadcast(
            Intent(TextToSpeech.Engine.ACTION_TTS_DATA_INSTALLED).apply {
                putExtra(TextToSpeech.Engine.EXTRA_TTS_DATA_INSTALLED, true)
                `package` = packageName
            },
        )
        setResult(Activity.RESULT_OK)
        startActivity(Intent(this, BlackBoxSettingsActivity::class.java))
        finish()
    }
}
