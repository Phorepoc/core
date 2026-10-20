/*
 * MemoryFootprintSymbian.cpp -- OURS (Phorepoc). WTF::memoryFootprint() = the committed size of the process heap (the only memory
 * a Symbian process owns besides stacks), asked from the euser shim.
 */
#include "config.h"
#include <wtf/MemoryFootprint.h>
#include <wtf/symbian/PhSymbian.h>

namespace WTF {

size_t memoryFootprint()
{
    return ph_heap_used_bytes();
}

} // namespace WTF
