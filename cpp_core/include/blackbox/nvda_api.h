#pragma once

#include <stdint.h>

#ifdef _WIN32
#define BBX_NVDA_API extern "C" __declspec(dllexport)
#else
#define BBX_NVDA_API extern "C"
#endif

BBX_NVDA_API int bbx_get_api_version();
BBX_NVDA_API int bbx_get_sample_rate();
BBX_NVDA_API int bbx_synthesize_utf16(
    const wchar_t* text,
    int rate_percent,
    int pitch_percent,
    int volume_percent,
    int modulation_percent,
    unsigned char** out_pcm,
    uint32_t* out_size
);
BBX_NVDA_API void bbx_free_buffer(void* buffer);
