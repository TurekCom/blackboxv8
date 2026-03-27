package com.turek.blackbox.android;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class SpeechTextNormalizerTest {
    @Test
    public void singleLetterGetsPolishName() {
        assertEquals("ce", SpeechTextNormalizer.INSTANCE.normalize("c", true));
        assertEquals("be", SpeechTextNormalizer.INSTANCE.normalize("b", true));
        assertEquals("si", SpeechTextNormalizer.INSTANCE.normalize("ś", true));
    }

    @Test
    public void singleSymbolGetsPolishName() {
        assertEquals("małpa", SpeechTextNormalizer.INSTANCE.normalize("@", true));
        assertEquals("kropka", SpeechTextNormalizer.INSTANCE.normalize(".", true));
    }

    @Test
    public void emojiCanBeReadOrLeftAsIs() {
        assertEquals("uśmiechnięta buźka", SpeechTextNormalizer.INSTANCE.normalize("😀", true));
        assertEquals("😀", SpeechTextNormalizer.INSTANCE.normalize("😀", false));
    }

    @Test
    public void cldrEmojiMapSupportsNewerEmojiAndVariants() {
        java.util.Map<String, String> map = new java.util.LinkedHashMap<>();
        map.put("🦬", "żubr");
        map.put("👋🏻", "machająca dłoń: karnacja jasna");
        assertEquals("żubr", SpeechTextNormalizer.INSTANCE.normalize("🦬", true, map));
        assertEquals("machająca dłoń: karnacja jasna", SpeechTextNormalizer.INSTANCE.normalize("👋🏻", true, map));
    }

    @Test
    public void inlineEmojiAndAsciiEmoticonsAreExpanded() {
        assertEquals("Hej uśmiechnięta buźka.", SpeechTextNormalizer.INSTANCE.normalize("Hej 😀.", true));
        assertEquals("To jest uśmiechnięta buźka", SpeechTextNormalizer.INSTANCE.normalize("To jest :)", true));
    }
}
