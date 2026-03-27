package com.turek.blackbox.android

import android.content.Context
import java.text.BreakIterator
import java.text.Normalizer
import java.util.Locale

object SpeechTextNormalizer {
    private val singleLabels = mapOf(
        "a" to "a",
        "ą" to "a z ogonkiem",
        "b" to "be",
        "c" to "ce",
        "ć" to "ci",
        "d" to "de",
        "e" to "e",
        "ę" to "e z ogonkiem",
        "f" to "ef",
        "g" to "gie",
        "h" to "ha",
        "i" to "i",
        "j" to "jot",
        "k" to "ka",
        "l" to "el",
        "ł" to "eł",
        "m" to "em",
        "n" to "en",
        "ń" to "eń",
        "o" to "o",
        "ó" to "u z kreską",
        "p" to "pe",
        "q" to "ku",
        "r" to "er",
        "s" to "es",
        "ś" to "si",
        "t" to "te",
        "u" to "u",
        "v" to "fał",
        "w" to "wu",
        "x" to "iks",
        "y" to "igrek",
        "z" to "zet",
        "ź" to "zi",
        "ż" to "żet",
        "0" to "zero",
        "1" to "jeden",
        "2" to "dwa",
        "3" to "trzy",
        "4" to "cztery",
        "5" to "pięć",
        "6" to "sześć",
        "7" to "siedem",
        "8" to "osiem",
        "9" to "dziewięć",
        " " to "spacja",
        "\t" to "tabulator",
        "\n" to "enter",
        "\r" to "enter",
        "." to "kropka",
        "," to "przecinek",
        ":" to "dwukropek",
        ";" to "średnik",
        "!" to "wykrzyknik",
        "?" to "znak zapytania",
        "@" to "małpa",
        "#" to "kratka",
        "$" to "dolar",
        "%" to "procent",
        "^" to "daszek",
        "&" to "ampersand",
        "*" to "gwiazdka",
        "(" to "nawias otwierający",
        ")" to "nawias zamykający",
        "[" to "lewy nawias kwadratowy",
        "]" to "prawy nawias kwadratowy",
        "{" to "lewa klamra",
        "}" to "prawa klamra",
        "<" to "mniejsze niż",
        ">" to "większe niż",
        "/" to "ukośnik",
        "\\" to "ukośnik wsteczny",
        "|" to "pionowa kreska",
        "-" to "minus",
        "_" to "podkreślnik",
        "=" to "równa się",
        "+" to "plus",
        "\"" to "cudzysłów",
        "'" to "apostrof",
        "`" to "akcent odwrotny",
        "~" to "tylda",
    )

    private val asciiEmoticons = linkedMapOf(
        "<3" to "serce",
        ":-)" to "uśmiechnięta buźka",
        ":)" to "uśmiechnięta buźka",
        ":-(" to "smutna buźka",
        ":(" to "smutna buźka",
        ";-)" to "mrugająca buźka",
        ";)" to "mrugająca buźka",
        ":-D" to "roześmiana buźka",
        ":D" to "roześmiana buźka",
        ":-P" to "buźka z językiem",
        ":P" to "buźka z językiem",
        ":-*" to "buziak",
        ":*" to "buziak",
    )

    private val fallbackEmojiLabels = mapOf(
        "😀" to "uśmiechnięta buźka",
        "😃" to "szeroki uśmiech",
        "😄" to "uśmiechnięta buźka z otwartymi ustami",
        "😁" to "roześmiana buźka",
        "😂" to "buźka ze łzami radości",
        "🤣" to "tarza się ze śmiechu",
        "🙂" to "lekko uśmiechnięta buźka",
        "🙃" to "odwrócona buźka",
        "😉" to "mrugająca buźka",
        "😊" to "uśmiechnięta buźka z rumieńcami",
        "😍" to "buźka z sercami w oczach",
        "🥰" to "uśmiechnięta buźka z sercami",
        "😘" to "buźka wysyłająca buziaka",
        "😎" to "buźka w okularach",
        "😢" to "płacząca buźka",
        "😭" to "głośno płacząca buźka",
        "😡" to "wściekła buźka",
        "🤔" to "zamyślona buźka",
        "🙄" to "przewracanie oczami",
        "😐" to "neutralna buźka",
        "😑" to "buźka bez wyrazu",
        "😅" to "uśmiech z potem",
        "😇" to "uśmiechnięta buźka z aureolą",
        "😋" to "oblizująca się buźka",
        "😜" to "mrugająca buźka z językiem",
        "😴" to "śpiąca buźka",
        "🤖" to "robot",
        "💩" to "kupka",
        "👍" to "kciuk w górę",
        "👎" to "kciuk w dół",
        "👏" to "klaskanie",
        "🙏" to "złożone dłonie",
        "💪" to "napięty biceps",
        "❤" to "czerwone serce",
        "💔" to "złamane serce",
        "💯" to "sto punktów",
        "🔥" to "ogień",
        "🎉" to "konfetti",
        "✨" to "iskry",
        "⭐" to "gwiazda",
        "🌟" to "świecąca gwiazda",
        "✅" to "zielony znacznik wyboru",
        "❌" to "krzyżyk",
        "⚠" to "ostrzeżenie",
        "💬" to "dymek rozmowy",
        "📧" to "e-mail",
        "📞" to "telefon",
        "☺" to "uśmiechnięta buźka",
        "☹" to "smutna buźka",
    )

    private val skinToneLabels = mapOf(
        0x1F3FB to "jasna karnacja",
        0x1F3FC to "średnio jasna karnacja",
        0x1F3FD to "średnia karnacja",
        0x1F3FE to "średnio ciemna karnacja",
        0x1F3FF to "ciemna karnacja",
    )

    fun normalize(context: Context, text: String, speakEmoji: Boolean): String {
        return normalize(text, speakEmoji, EmojiAnnotationsRepository.load(context))
    }

    fun normalize(text: String, speakEmoji: Boolean): String {
        return normalize(text, speakEmoji, emptyMap())
    }

    fun normalize(text: String, speakEmoji: Boolean, emojiLabels: Map<String, String>): String {
        if (text.isEmpty()) {
            return text
        }
        val asciiNormalized = if (speakEmoji) replaceAsciiEmoticons(text) else text
        val clusters = graphemeClusters(asciiNormalized)
        if (clusters.size == 1) {
            return labelForSingleCluster(clusters.first(), speakEmoji, emojiLabels) ?: asciiNormalized
        }
        if (!speakEmoji) {
            return asciiNormalized
        }

        val out = StringBuilder(asciiNormalized.length + 32)
        var lastWasEmojiWord = false
        for (cluster in clusters) {
            val emojiLabel = emojiLabelForCluster(cluster, emojiLabels)
            if (emojiLabel != null) {
                appendSpacer(out)
                out.append(emojiLabel)
                lastWasEmojiWord = true
                continue
            }
            if (lastWasEmojiWord && needsLeadingSpace(cluster)) {
                out.append(' ')
            }
            out.append(cluster)
            lastWasEmojiWord = false
        }
        return out.toString().replace(Regex(" {2,}"), " ").trim()
    }

    internal fun labelForSingleCluster(
        cluster: String,
        speakEmoji: Boolean,
        emojiLabels: Map<String, String> = emptyMap(),
    ): String? {
        if (cluster.isEmpty()) {
            return null
        }
        val normalizedCluster = Normalizer.normalize(cluster, Normalizer.Form.NFC)
        val labelKey = normalizedCluster.lowercase(Locale.ROOT)
        singleLabels[labelKey]?.let { return it }
        if (speakEmoji) {
            emojiLabelForCluster(normalizedCluster, emojiLabels)?.let { return it }
        }
        return null
    }

    private fun replaceAsciiEmoticons(text: String): String {
        var out = text
        for ((emoticon, spoken) in asciiEmoticons) {
            out = out.replace(emoticon, " $spoken ")
        }
        return out
    }

    private fun graphemeClusters(text: String): List<String> {
        val iterator = BreakIterator.getCharacterInstance(Locale.ROOT)
        iterator.setText(text)
        val out = ArrayList<String>(text.length.coerceAtLeast(1))
        var start = iterator.first()
        var end = iterator.next()
        while (end != BreakIterator.DONE) {
            out += text.substring(start, end)
            start = end
            end = iterator.next()
        }
        return out
    }

    private fun emojiLabelForCluster(cluster: String, emojiLabels: Map<String, String>): String? {
        emojiLabels[cluster]?.let { return it }
        val simplified = simplifyEmojiKey(cluster)
        emojiLabels[simplified]?.let { return it }
        fallbackEmojiLabels[simplified]?.let { base ->
            return withSkinTone(base, cluster)
        }
        keycapBaseLabel(cluster)?.let { return it }
        if (isFlagCluster(cluster)) {
            return "flaga"
        }
        if (isEmojiCluster(cluster)) {
            return "emotikona"
        }
        return null
    }

    private fun simplifyEmojiKey(cluster: String): String {
        val out = StringBuilder(cluster.length)
        forEachCodePoint(cluster) { codePoint ->
            if (codePoint == 0xFE0F || codePoint in 0x1F3FB..0x1F3FF) {
                return@forEachCodePoint
            }
            out.appendCodePoint(codePoint)
        }
        return out.toString()
    }

    private fun keycapBaseLabel(cluster: String): String? {
        if (!cluster.contains('\u20E3')) {
            return null
        }
        val simplified = simplifyEmojiKey(cluster)
        val base = simplified.firstOrNull { it.isDigit() || it == '#' || it == '*' } ?: return null
        return singleLabels[base.toString()]
    }

    private fun withSkinTone(base: String, cluster: String): String {
        val tone = extractSkinTone(cluster) ?: return base
        return "$base, $tone"
    }

    private fun extractSkinTone(cluster: String): String? {
        var found: String? = null
        forEachCodePoint(cluster) { codePoint ->
            skinToneLabels[codePoint]?.let { found = it }
        }
        return found
    }

    private fun isFlagCluster(cluster: String): Boolean {
        var count = 0
        var allRegionalIndicators = true
        forEachCodePoint(cluster) { codePoint ->
            count += 1
            if (codePoint !in 0x1F1E6..0x1F1FF) {
                allRegionalIndicators = false
            }
        }
        return allRegionalIndicators && count == 2
    }

    private fun isEmojiCluster(cluster: String): Boolean {
        var hasEmoji = false
        forEachCodePoint(cluster) { codePoint ->
            if (
                codePoint in 0x1F000..0x1FAFF ||
                codePoint in 0x2600..0x27BF ||
                codePoint in 0x2300..0x23FF ||
                codePoint in 0x1F1E6..0x1F1FF ||
                codePoint == 0x00A9 ||
                codePoint == 0x00AE ||
                codePoint == 0x20E3 ||
                codePoint == 0x200D ||
                codePoint == 0xFE0F
            ) {
                hasEmoji = true
            }
        }
        return hasEmoji
    }

    private fun appendSpacer(out: StringBuilder) {
        if (out.isNotEmpty() && !out.last().isWhitespace()) {
            out.append(' ')
        }
    }

    private fun needsLeadingSpace(cluster: String): Boolean {
        if (cluster.isEmpty()) {
            return false
        }
        val codePoint = cluster.codePointAt(0)
        if (Character.isWhitespace(codePoint)) {
            return false
        }
        return !isPunctuationLike(codePoint)
    }

    private fun isPunctuationLike(codePoint: Int): Boolean {
        return when (Character.getType(codePoint)) {
            Character.CONNECTOR_PUNCTUATION.toInt(),
            Character.DASH_PUNCTUATION.toInt(),
            Character.START_PUNCTUATION.toInt(),
            Character.END_PUNCTUATION.toInt(),
            Character.OTHER_PUNCTUATION.toInt(),
            Character.INITIAL_QUOTE_PUNCTUATION.toInt(),
            Character.FINAL_QUOTE_PUNCTUATION.toInt(),
            Character.MATH_SYMBOL.toInt(),
            Character.CURRENCY_SYMBOL.toInt(),
            Character.MODIFIER_SYMBOL.toInt(),
            Character.OTHER_SYMBOL.toInt() -> true
            else -> false
        }
    }

    private inline fun forEachCodePoint(text: String, block: (Int) -> Unit) {
        var index = 0
        while (index < text.length) {
            val codePoint = text.codePointAt(index)
            block(codePoint)
            index += Character.charCount(codePoint)
        }
    }
}
