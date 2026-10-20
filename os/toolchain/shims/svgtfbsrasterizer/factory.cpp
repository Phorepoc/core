// OURS: FBServ binds fbsrasterizer.dll ordinal 1 = CFbsRasterizer::New() (FBS's frozen
// fbsrasterizeru.def). S60's SvgtFbsRasterizer names its factory CSvgtFbsRasterizer::New() and its
// own .def is not part of the 2010 release -- bridge the two.
#include <graphics/fbsrasterizer.h>
#include "SvgtFbsRasterizer.h"

EXPORT_C CFbsRasterizer* CFbsRasterizer::New()
    {
    return CSvgtFbsRasterizer::New();
    }
