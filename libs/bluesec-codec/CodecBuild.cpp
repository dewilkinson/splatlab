// CodecBuild.cpp
// Surfels -- Copyright (c) 2026 Dave Wilkinson / Blueshell LLC
// SPDX-License-Identifier: Apache-2.0
//
// Open-build identification: this file is compiled only when the open stand-in implementation of
// bluesec-codec is built (see README.md in this directory). The application shows the line below
// as a banner row so nobody mistakes the stand-in's output for the proprietary build's.

#include "CodecBuild.h"

namespace Surfels
{
    bool        CodecBuild::IsProprietary()   { return false; }
    const char* CodecBuild::Name()            { return "open stand-in"; }
    const char* CodecBuild::MissingFeatures() { return "no occlusion bake, simple LOD, plain order, no splat SH"; }
}
