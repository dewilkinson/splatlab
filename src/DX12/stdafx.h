// Precompiled/shared header for the Surfels sample.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

// Cauldron (from libs/cauldron/src/DX12 and libs/cauldron/src/Common, both
// added as PUBLIC include dirs by the Cauldron_DX12 CMake target).
#include "base/Device.h"
#include "base/SwapChain.h"
#include "base/FrameworkWindows.h"
#include "base/ResourceViewHeaps.h"
#include "base/UploadHeap.h"
#include "base/DynamicBufferRing.h"
#include "base/CommandListRing.h"
#include "base/ShaderCompilerHelper.h"
#include "base/Helper.h"
#include "base/Imgui.h"
#include "base/ImGuiHelper.h"

#include "Misc/Misc.h"

using namespace CAULDRON_DX12;
