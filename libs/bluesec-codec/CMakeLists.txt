# bluesec-codec -- the algorithm core (lifting-wavelet LOD decomposition, compression codec,
# interior occlusion volume generator, streaming order and detail grid). See README.md in
# this directory.
#
# Two ways to satisfy the `bluesec-codec` target every consumer (SplatLab, Surfels_DX12, the
# console test tools) links against:
#
#   1. PREBUILT (default when present): libs/bluesec-codec/prebuilt/{Debug,Release}/bluesec-codec.lib
#      are the real, proprietary algorithms compiled from the private repository and checked in
#      here via Git LFS (same arrangement as libs/cauldron-prebuilt; licensed under
#      prebuilt/LICENSE.txt, not Apache-2.0). Linking them gives this
#      public build the genuine wavelet, codec and occlusion-volume behaviour -- packages it
#      writes carry a baked occlusion volume, and it reads packages written by the private build.
#      The headers here are the public API/data contract those binaries were compiled against.
#
#   2. OPEN STAND-INS (fallback): if the prebuilt library is missing for this platform (LFS
#      objects not pulled, a non-MSVC/non-x64 toolchain, or BLUESEC_CODEC_USE_PREBUILT turned
#      off), the simplified Apache-2.0 .cpp files in this directory are compiled instead. They
#      keep the same signatures, but the wavelet is plain decimation, the codec is transpose-only,
#      the occlusion generator bakes nothing and the streaming order is plain -- see README.md for
#      exactly what differs. The app shows a banner row naming the missing pieces (CodecBuild.h).
cmake_minimum_required(VERSION 3.24)

project(bluesec-codec)

option(BLUESEC_CODEC_USE_PREBUILT "Link the prebuilt proprietary bluesec-codec.lib instead of compiling the open stand-ins" ON)

set(BLUESEC_CODEC_PREBUILT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt")
set(BLUESEC_CODEC_PREBUILT_RELEASE "${BLUESEC_CODEC_PREBUILT_DIR}/Release/bluesec-codec.lib")
set(BLUESEC_CODEC_PREBUILT_DEBUG   "${BLUESEC_CODEC_PREBUILT_DIR}/Debug/bluesec-codec.lib")

if(BLUESEC_CODEC_USE_PREBUILT)
    # The .lib files are MSVC x64 static libraries (compiled /MD for Release, /MDd for Debug --
    # the same CRT the rest of this project uses), so only an MSVC x64 build can consume them.
    if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "bluesec-codec: prebuilt library is MSVC x64 only -- compiling the open stand-ins for this toolchain instead.")
        set(BLUESEC_CODEC_USE_PREBUILT OFF)
    elseif(NOT EXISTS "${BLUESEC_CODEC_PREBUILT_RELEASE}" OR NOT EXISTS "${BLUESEC_CODEC_PREBUILT_DEBUG}")
        message(STATUS "bluesec-codec: prebuilt/{Debug,Release}/bluesec-codec.lib missing (Git LFS objects not pulled?) -- compiling the open stand-ins instead. Run 'git lfs pull' and re-configure to link the real codec.")
        set(BLUESEC_CODEC_USE_PREBUILT OFF)
    else()
        # A Git LFS pointer file that was never smudged is a ~130-byte text file, not a library;
        # linking it fails with an unhelpful "invalid or corrupt file". Catch that up front.
        file(SIZE "${BLUESEC_CODEC_PREBUILT_RELEASE}" _bluesecReleaseBytes)
        if(_bluesecReleaseBytes LESS 4096)
            message(STATUS "bluesec-codec: prebuilt/Release/bluesec-codec.lib is a Git LFS pointer, not the library (run 'git lfs pull') -- compiling the open stand-ins instead.")
            set(BLUESEC_CODEC_USE_PREBUILT OFF)
        endif()
    endif()
endif()

if(BLUESEC_CODEC_USE_PREBUILT)
    message(STATUS "bluesec-codec: linking the prebuilt proprietary library from ${BLUESEC_CODEC_PREBUILT_DIR}")
    add_library(bluesec-codec STATIC IMPORTED GLOBAL)
    set_target_properties(bluesec-codec PROPERTIES
        IMPORTED_LOCATION_DEBUG          "${BLUESEC_CODEC_PREBUILT_DEBUG}"
        IMPORTED_LOCATION_RELEASE        "${BLUESEC_CODEC_PREBUILT_RELEASE}"
        IMPORTED_LOCATION_MINSIZEREL     "${BLUESEC_CODEC_PREBUILT_RELEASE}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${BLUESEC_CODEC_PREBUILT_RELEASE}"
        # Single-config generators (Ninja, Makefiles) consult IMPORTED_LOCATION when no
        # per-config location matches; MSVC single-config Debug builds still pick the entry above.
        IMPORTED_LOCATION                "${BLUESEC_CODEC_PREBUILT_RELEASE}")
    # IMPORTED targets only accept INTERFACE here (nothing of bluesec-codec itself is compiled);
    # consumers see the include directory exactly as they would from PUBLIC on a source target.
    target_include_directories(bluesec-codec INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
else()
    message(STATUS "bluesec-codec: compiling the open stand-in implementation (no occlusion volume, decimation LOD, transpose-only codec, plain streaming order)")
    add_library(bluesec-codec STATIC
        LiftingWavelet.h
        LiftingWavelet.cpp
        ByteShuffle.h
        ByteShuffle.cpp
        OcclusionVolume.h
        OcclusionVolume.cpp
        StreamOrder.h
        StreamOrder.cpp
        DetailHeatmap.h
        DetailHeatmap.cpp
        CodecBuild.h
        CodecBuild.cpp
        SplatCodec.h
        SplatCodec.cpp
    )

    target_include_directories(bluesec-codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    set_target_properties(bluesec-codec PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        FOLDER "Core"
    )
endif()
