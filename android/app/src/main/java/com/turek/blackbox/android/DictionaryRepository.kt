package com.turek.blackbox.android

import android.content.Context
import android.net.Uri
import java.io.File
import java.nio.ByteBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.Charset
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets
import java.util.Locale
import java.util.concurrent.atomic.AtomicLong

object DictionaryRepository {
    private const val FILE_NAME = "blackbox_dictionary.txt"
    private val lastLoaded = AtomicLong(Long.MIN_VALUE)
    @Volatile
    private var cachedDictionary: Map<String, String> = emptyMap()

    fun importFromUri(context: Context, uri: Uri): String {
        val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
            ?: throw IllegalStateException("Unable to read dictionary")
        val decoded = decodeText(bytes)
        val parsed = parse(decoded)
        require(parsed.isNotEmpty()) { "Dictionary is empty" }
        val target = dictionaryFile(context)
        target.writeText(parsed.entries.joinToString(separator = "\n") { "${it.key}=${it.value}" }, StandardCharsets.UTF_8)
        lastLoaded.set(Long.MIN_VALUE)
        return uri.lastPathSegment?.substringAfterLast('/')?.ifBlank { FILE_NAME } ?: FILE_NAME
    }

    fun apply(context: Context, text: String): String {
        val dictionary = loadDictionary(context)
        if (dictionary.isEmpty() || text.isEmpty()) {
            return text
        }
        val wordRegex = Regex("\\p{L}[\\p{L}\\p{M}\\p{Nd}_'-]*")
        return wordRegex.replace(text) { match ->
            dictionary[match.value.lowercase(Locale.ROOT)] ?: match.value
        }
    }

    private fun loadDictionary(context: Context): Map<String, String> {
        val file = dictionaryFile(context)
        if (!file.exists()) {
            cachedDictionary = emptyMap()
            lastLoaded.set(Long.MIN_VALUE)
            return emptyMap()
        }
        val modified = file.lastModified()
        if (lastLoaded.get() == modified) {
            return cachedDictionary
        }
        val parsed = parse(file.readText(StandardCharsets.UTF_8))
        cachedDictionary = parsed
        lastLoaded.set(modified)
        return parsed
    }

    private fun dictionaryFile(context: Context): File = File(context.filesDir, FILE_NAME)

    private fun decodeText(bytes: ByteArray): String {
        val utf8 = StandardCharsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT)
        return try {
            utf8.decode(ByteBuffer.wrap(bytes)).toString()
        } catch (_: CharacterCodingException) {
            String(bytes, Charset.forName("windows-1250"))
        }
    }

    private fun parse(raw: String): LinkedHashMap<String, String> {
        val out = LinkedHashMap<String, String>()
        raw.lineSequence().forEach { line ->
            val trimmed = line.trim()
            if (trimmed.isEmpty() || trimmed.startsWith("#")) {
                return@forEach
            }
            val eq = trimmed.indexOf('=')
            if (eq <= 0 || eq == trimmed.lastIndex) {
                return@forEach
            }
            val original = trimmed.substring(0, eq).trim().lowercase(Locale.ROOT)
            val spoken = trimmed.substring(eq + 1).trim()
            if (original.isNotEmpty() && spoken.isNotEmpty()) {
                out[original] = spoken
            }
        }
        return out
    }
}
