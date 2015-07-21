#pragma once

#include "../stdafx.h"
#include "../util/addresses.h"
#include "../util/config.h"

#include "dxerr.h"

inline HRESULT handleD3dError(const char* method, HRESULT result) {
	if (result != S_OK) {
		logger->warn("Direct3D Error @ {}: {}", method, DXGetErrorString(result));
	}
	return result;
}

#define D3DLOG(CMD) handleD3dError(#CMD, CMD)

// Forward declaration for all our adapters
struct Direct3DVertexBuffer8Adapter;
struct Direct3DIndexBuffer8Adapter;
struct Direct3DTexture8Adapter;
struct Direct3DDevice8Adapter;
struct Direct3DSurface8Adapter;
struct Direct3D8Adapter;
