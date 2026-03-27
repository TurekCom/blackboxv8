package com.turek.blackbox.android

object NativeBlackBox {
    init {
        System.loadLibrary("blackbox_android_native")
    }

    external fun getSampleRate(): Int
    external fun synthesizePcm(
        text: String,
        ratePercent: Int,
        pitchPercent: Int,
        volumePercent: Int,
        modulationPercent: Int,
    ): ByteArray
}
