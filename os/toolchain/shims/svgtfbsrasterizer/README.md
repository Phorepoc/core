# svgtfbsrasterizer factory bridge (OURS)

`SvgtFbsRasterizer.mmp` names `../eabi/fbsrasterizer.def`, which is not in the 2010 release, and its
factory is `CSvgtFbsRasterizer::New()` — not the `CFbsRasterizer::New()` that FBS's
`fbsrasterizeru.def` freezes at ordinal 1 and FBServ imports. `factory.cpp` bridges them.
