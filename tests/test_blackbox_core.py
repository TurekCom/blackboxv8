import unittest

import blackbox_core


class BlackboxCoreTests(unittest.TestCase):
    def test_synthesize_non_empty_for_polish_text(self):
        pcm = blackbox_core.synthesize_pcm16("Dzień dobry")
        self.assertTrue(len(pcm) > 0)

    def test_synthesize_empty_for_blank_input(self):
        pcm = blackbox_core.synthesize_pcm16("   ")
        self.assertEqual(pcm, b"")

    def test_output_is_stable_for_same_input(self):
        a = blackbox_core.synthesize_pcm16("zażółć gęślą jaźń", rate=55, pitch=45, volume=95)
        b = blackbox_core.synthesize_pcm16("zażółć gęślą jaźń", rate=55, pitch=45, volume=95)
        self.assertEqual(a, b)


if __name__ == "__main__":
    unittest.main()
