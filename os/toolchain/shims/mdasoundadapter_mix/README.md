# mdasoundadapter_mix -- the Q0 userland audio mixer (OURS)

`ph_mixsc.h` is force-included (`-include`) into the unmodified 2010
`mdasoundadapter` and swaps `RSoundSc` for the virtual `RMixSoundSc`; `phmixer.cpp` sums all virtual channels onto the
one real SoundSc channel (48 kHz stereo). `phmixer_core.h` = the pure arithmetic (host test: `tests/user/audiomix`).
`ph_audio_props.h` = the P&S master-volume contract with `src/phvolume`. Wired in `emitter/components.py` (`_MIXQ0`).
