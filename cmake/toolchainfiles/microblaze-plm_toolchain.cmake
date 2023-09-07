# Copyright (C) 2023 Advanced Micro Devices, Inc.  All rights reserved.
# SPDX-License-Identifier: MIT
set( CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY" )
set( CMAKE_EXPORT_COMPILE_COMMANDS ON)
set( CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_LIBRARY_PATH)
set( CMAKE_INSTALL_MESSAGE LAZY)
set( CMAKE_C_COMPILER mb-gcc )
set( CMAKE_CXX_COMPILER mb-g++ )
set( CMAKE_C_COMPILER_LAUNCHER  )
set( CMAKE_CXX_COMPILER_LAUNCHER  )
set( CMAKE_ASM_COMPILER mb-gcc )
set( CMAKE_AR mb-ar CACHE FILEPATH "Archiver" )
set( CMAKE_SIZE mb-size CACHE FILEPATH "Size" )
set( CMAKE_SYSTEM_PROCESSOR "plm_microblaze" )
set( CMAKE_MACHINE "Versal" CACHE STRING "cmake machine" FORCE)
set( CMAKE_BUILD_TYPE Release)
set( CMAKE_SYSTEM_NAME "Generic" )
set( CMAKE_SPECS_FILE "$ENV{ESW_REPO}/scripts/specs/microblaze/Xilinx.spec" CACHE STRING "Specs file path for using CMAKE toolchain files" )
# FIXME: Use generic microblaze toolchain file for pmc microblaze processor as well.
set( TOOLCHAIN_PLM_C_FLAGS " -mlittle-endian -mxl-barrel-shift -mxl-pattern-compare -mxl-reorder -mno-xl-soft-mul -mxl-multiply-high -mno-xl-soft-div -Os -flto -ffat-lto-objects -mcpu=v10.0")
set( TOOLCHAIN_C_FLAGS " -O2 ${TOOLCHAIN_PLM_C_FLAGS} -DSDT" CACHE STRING "CFLAGS" )
set( TOOLCHAIN_CXX_FLAGS " -O2 ${TOOLCHAIN_PLM_C_FLAGS} -DSDT" CACHE STRING "CXXFLAGS" )
set( TOOLCHAIN_ASM_FLAGS " -O2 ${TOOLCHAIN_PLM_C_FLAGS} -DSDT" CACHE STRING "ASM FLAGS" )
set( TOOLCHAIN_DEP_FLAGS " -MMD -MP" CACHE STRING "Flags used by compiler to generate dependency files")
set( TOOLCHAIN_EXTRA_C_FLAGS " -g -ffunction-sections -fdata-sections -Wall -Wextra -fno-tree-loop-distribute-patterns" CACHE STRING "Extra CFLAGS")
set( CMAKE_C_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "Additional CFLAGS for release" )
set( CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "Additional CXXFLAGS for release" )
set( CMAKE_ASM_FLAGS_RELEASE "-DNDEBUG" CACHE STRING "Additional ASM FLAGS for release" )
