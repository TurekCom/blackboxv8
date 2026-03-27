package com.turek.blackbox.android

import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.view.accessibility.AccessibilityEvent
import android.widget.SeekBar
import android.widget.TextView
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import com.turek.blackbox.android.databinding.ActivitySettingsBinding

class BlackBoxSettingsActivity : ComponentActivity(), TextToSpeech.OnInitListener {
    private lateinit var binding: ActivitySettingsBinding
    private var settings = EngineSettings()
    private var previewTts: TextToSpeech? = null
    private var previewReady = false

    private val pickDictionary = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) {
            return@registerForActivityResult
        }
        runCatching {
            val name = DictionaryRepository.importFromUri(this, uri)
            settings = settings.copy(dictionaryName = name)
            BlackBoxPreferences.save(this, settings)
            updateDictionaryStatus(name)
            announce(getString(R.string.dictionary_loaded, name))
        }.onFailure {
            updateDictionaryStatus(null)
            announce(getString(R.string.dictionary_error))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)
        previewTts = TextToSpeech(this, this, packageName)

        settings = BlackBoxPreferences.load(this)
        bindSeekBar(binding.rateSeek, binding.rateValue, settings.ratePercent) { value ->
            settings = settings.copy(ratePercent = value)
            BlackBoxPreferences.save(this, settings)
        }
        bindSeekBar(binding.pitchSeek, binding.pitchValue, settings.pitchPercent) { value ->
            settings = settings.copy(pitchPercent = value)
            BlackBoxPreferences.save(this, settings)
        }
        bindSeekBar(binding.volumeSeek, binding.volumeValue, settings.volumePercent) { value ->
            settings = settings.copy(volumePercent = value)
            BlackBoxPreferences.save(this, settings)
        }
        bindSeekBar(binding.modulationSeek, binding.modulationValue, settings.modulationPercent) { value ->
            settings = settings.copy(modulationPercent = value)
            BlackBoxPreferences.save(this, settings)
        }
        binding.speakEmojiCheck.isChecked = settings.speakEmoji
        binding.speakEmojiCheck.setOnCheckedChangeListener { _, checked ->
            settings = settings.copy(speakEmoji = checked)
            BlackBoxPreferences.save(this, settings)
            announce(
                if (checked) getString(R.string.emoji_enabled)
                else getString(R.string.emoji_disabled)
            )
        }

        updateDictionaryStatus(settings.dictionaryName)
        binding.importDictionaryButton.setOnClickListener {
            pickDictionary.launch(arrayOf("text/plain", "text/*", "application/octet-stream"))
        }
        binding.previewSpeakButton.setOnClickListener {
            speakPreview()
        }
        binding.previewStopButton.setOnClickListener {
            previewTts?.stop()
        }
    }

    override fun onInit(status: Int) {
        val tts = previewTts ?: return
        if (status == TextToSpeech.SUCCESS) {
            tts.language = BlackBoxEngine.LOCALE
            runCatching { tts.setVoice(tts.voices.firstOrNull { it.name == BlackBoxEngine.VOICE_NAME }) }
            previewReady = true
            announce(getString(R.string.preview_ready))
        } else {
            previewReady = false
            announce(getString(R.string.preview_error))
        }
    }

    override fun onDestroy() {
        previewTts?.stop()
        previewTts?.shutdown()
        previewTts = null
        super.onDestroy()
    }

    private fun bindSeekBar(seekBar: SeekBar, valueView: TextView, initialValue: Int, onChanged: (Int) -> Unit) {
        seekBar.progress = initialValue.coerceIn(0, 100)
        updatePercentText(seekBar, valueView, seekBar.progress)
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                updatePercentText(seekBar, valueView, progress)
                if (fromUser) {
                    onChanged(progress)
                }
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit

            override fun onStopTrackingTouch(seekBar: SeekBar) {
                announce(getString(R.string.percent_value, seekBar.progress))
            }
        })
    }

    private fun updatePercentText(seekBar: SeekBar, valueView: TextView, value: Int) {
        val text = getString(R.string.percent_value, value)
        valueView.text = text
        ViewCompat.setStateDescription(seekBar, text)
    }

    private fun updateDictionaryStatus(name: String?) {
        binding.dictionaryStatus.text = if (name.isNullOrBlank()) {
            getString(R.string.dictionary_none)
        } else {
            getString(R.string.dictionary_loaded, name)
        }
    }

    private fun speakPreview() {
        val text = binding.previewEdit.text?.toString()?.trim().orEmpty()
        if (text.isEmpty()) {
            announce(getString(R.string.preview_empty))
            return
        }
        if (!previewReady) {
            announce(getString(R.string.preview_error))
            return
        }
        previewTts?.stop()
        previewTts?.speak(text, TextToSpeech.QUEUE_FLUSH, null, "blackbox-preview")
    }

    private fun announce(text: String) {
        window.decorView.announceForAccessibility(text)
        window.decorView.sendAccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT)
    }
}
