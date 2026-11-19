# Usage: cmake -S . -B build-arm-gcc -DCMAKE_TOOLCHAIN_FILE=cmake/gec6818.cmake
# Override with -DARM_CC=/absolute/path/to/arm-linux-gcc if needed.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(ARM_CC arm-linux-gcc CACHE STRING "Board SDK C compiler")
set(CMAKE_C_COMPILER "${ARM_CC}")
set(TERMINAL_TESTS OFF CACHE BOOL "Do not build desktop tests for the target" FORCE)
