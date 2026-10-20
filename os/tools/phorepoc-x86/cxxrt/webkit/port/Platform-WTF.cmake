# PlatformSymbian.cmake -- OURS (Phorepoc, browser-plan.md section 9): the Symbian port builds WTF exactly as JSCOnly
# (whose Platform file carries the Phorepoc seams, webkit/apply_patches.sh).
include(${CMAKE_CURRENT_LIST_DIR}/PlatformJSCOnly.cmake)
# UnixFileDescriptor.h: SharedMemory / GLFence use it (WPE and Haiku export it too)
list(APPEND WTF_PUBLIC_HEADERS unix/UnixFileDescriptor.h)
