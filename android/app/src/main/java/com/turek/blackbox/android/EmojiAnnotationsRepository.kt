package com.turek.blackbox.android

import android.content.Context
import java.nio.charset.StandardCharsets

object EmojiAnnotationsRepository {
    private const val ASSET_PATH = "emoji/emoji_pl_cldr.tsv"

    @Volatile
    private var cached: Map<String, String>? = null

    fun load(context: Context): Map<String, String> {
        cached?.let { return it }
        return synchronized(this) {
            cached?.let { return@synchronized it }
            val loaded = LinkedHashMap<String, String>()
            context.assets.open(ASSET_PATH).bufferedReader(StandardCharsets.UTF_8).useLines { lines ->
                lines.forEach { line ->
                    val tab = line.indexOf('\t')
                    if (tab <= 0 || tab >= line.lastIndex) {
                        return@forEach
                    }
                    val cp = line.substring(0, tab)
                    val tts = line.substring(tab + 1).trim()
                    if (cp.isNotEmpty() && tts.isNotEmpty()) {
                        loaded[cp] = tts
                    }
                }
            }
            cached = loaded
            loaded
        }
    }
}
