# minimp3 (vendored)

- Upstream: https://github.com/lieff/minimp3 (`minimp3.h`, master as of 2026-09-27), licence CC0 1.0 (`LICENSE`).
- Used by `src/codecs/arimp3decwrapper` (OURS), the MP3 decoder behind the 2010 release's Aricent MP3 plug-in. The
  policy is in ("Codec policy"): MP3 patents have expired, and the decoder is public domain.
- **One local change**, in `mp3dec_decode_frame`: when `MINIMP3_SCRATCH` is defined, the ~16 KB `mp3dec_scratch_t`
  is taken from that pointer instead of the stack. The MMF controller thread that runs the codec has a small stack,
  and the first decoded frame overflowed it (KERN-EXEC 3 at the stack guard). Without the macro, the code is upstream's.
