#define _CRT_SECURE_NO_WARNINGS

// We'd rather use std::min
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <timeapi.h>

#include "Shlobj.h"
#include "Shobjidl.h"
#pragma comment(lib, "shell32.lib")

#include "Shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include <string>
#include <functional>
#include <vector>
#include <chrono>
#include <cassert>
#include <memory>
#include <algorithm>
#include <locale>
#include <future>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace std;

#define _USE_MATH_DEFINES
#include <math.h>

namespace d3d8
{
#include "d3d8/d3d8.h"
}

#undef D3DMATRIX_DEFINED
#undef D3DFVF_POSITION_MASK
#undef D3DFVF_RESERVED2
#undef D3DSP_REGNUM_MASK
#undef DIRECT3D_VERSION
#undef D3D_SDK_VERSION
#include <d3d9.h>
#include <D3dx9tex.h>

#include "dependencies/cppformat/format.h"
using fmt::format;

#include "MinHook.h"
#include "dependencies/spdlog/spdlog.h"

#define Py_NO_ENABLE_SHARED
#include "Python.h"
#undef _GNU_SOURCE // Defined by python for some reason
#undef LONG_LONG

extern shared_ptr<spdlog::logger> logger;
