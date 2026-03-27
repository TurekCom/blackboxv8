package com.turek.blackbox.android

import android.media.AudioFormat
import android.os.Bundle
import android.speech.tts.SynthesisCallback
import android.speech.tts.SynthesisRequest
import android.speech.tts.TextToSpeech
import android.speech.tts.TextToSpeechService
import android.speech.tts.Voice
import kotlin.math.roundToInt

class BlackBoxTtsService : TextToSpeechService() {
    @Volatile
    private var stopRequested = false

    override fun onGetLanguage(): Array<String> = arrayOf(
        BlackBoxEngine.iso3Language(),
        BlackBoxEngine.iso3Country(),
        "",
    )

    override fun onIsLanguageAvailable(language: String, country: String, variant: String): Int {
        return if (BlackBoxEngine.matches(language, country)) {
            TextToSpeech.LANG_COUNTRY_AVAILABLE
        } else {
            TextToSpeech.LANG_NOT_SUPPORTED
        }
    }

    override fun onLoadLanguage(language: String, country: String, variant: String): Int {
        return onIsLanguageAvailable(language, country, variant)
    }

    override fun onGetDefaultVoiceNameFor(language: String, country: String, variant: String): String? {
        return if (BlackBoxEngine.matches(language, country)) BlackBoxEngine.VOICE_NAME else null
    }

    override fun onGetVoices(): MutableList<Voice> {
        return mutableListOf(
            Voice(
                BlackBoxEngine.VOICE_NAME,
                BlackBoxEngine.LOCALE,
                Voice.QUALITY_NORMAL,
                Voice.LATENCY_NORMAL,
                false,
                BlackBoxEngine.features(),
            )
        )
    }

    override fun onStop() {
        stopRequested = true
    }

    override fun onSynthesizeText(request: SynthesisRequest, callback: SynthesisCallback) {
        stopRequested = false
        val originalText = request.charSequenceText?.toString() ?: request.text.orEmpty()
        val settings = BlackBoxPreferences.load(this)
        val text = SpeechTextNormalizer.normalize(
            this,
            DictionaryRepository.apply(this, originalText),
            settings.speakEmoji,
        )
        val ratePercent = mergePercent(settings.ratePercent, request.speechRate)
        val pitchPercent = mergePercent(settings.pitchPercent, request.pitch)
        val volumePercent = mergeVolume(settings.volumePercent, request.params)
        val modulationPercent = settings.modulationPercent

        val pcm = try {
            NativeBlackBox.synthesizePcm(text, ratePercent, pitchPercent, volumePercent, modulationPercent)
        } catch (_: Throwable) {
            callback.error(TextToSpeech.ERROR_SYNTHESIS)
            return
        }

        if (stopRequested) {
            callback.error(TextToSpeech.STOPPED)
            return
        }

        val startResult = callback.start(NativeBlackBox.getSampleRate(), AudioFormat.ENCODING_PCM_16BIT, 1)
        if (startResult != TextToSpeech.SUCCESS) {
            callback.error(TextToSpeech.ERROR_OUTPUT)
            return
        }

        if (pcm.isEmpty()) {
            callback.done()
            return
        }

        val chunkSize = callback.maxBufferSize.coerceAtLeast(2048)
        var offset = 0
        while (offset < pcm.size) {
            if (stopRequested || callback.hasFinished()) {
                callback.error(TextToSpeech.STOPPED)
                return
            }
            val size = minOf(chunkSize, pcm.size - offset)
            val result = callback.audioAvailable(pcm, offset, size)
            if (result != TextToSpeech.SUCCESS) {
                callback.error(TextToSpeech.ERROR_OUTPUT)
                return
            }
            offset += size
        }

        callback.done()
    }

    private fun mergePercent(basePercent: Int, requestPercent: Int): Int {
        val normalizedRequest = if (requestPercent <= 0) 100 else requestPercent
        return ((basePercent.coerceIn(0, 100) / 100f) * normalizedRequest).roundToInt().coerceIn(0, 100)
    }

    private fun mergeVolume(basePercent: Int, params: Bundle?): Int {
        val rawVolume = params?.getFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, 1.0f) ?: 1.0f
        return (basePercent * rawVolume).roundToInt().coerceIn(0, 100)
    }
}
