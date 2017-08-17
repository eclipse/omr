# Cross compile to x86 from an x86-64 machine on a gnu-ish toolchain

set(CMAKE_SYSTEM_PROCESSOR "x86")
set(CMAKE_C_FLAGS "-m32 -msse2")
set(CMAKE_CXX_FLAGS "-m32 -msse2")
set(CMAKE_EXE_LINKER_FLAGS "-m32")
set(CMAKE_SHARED_LINKER_FLAGS "-m32")
set(CMAKE_STATIC_LINKER_FLAGS "-m32")
