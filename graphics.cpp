#include "stdafx.h"

#include "graphics.h"
#include "temple_functions.h"
#include "addresses.h"
#include "idxtables.h"
#include "config.h"
#include "movies.h"
#include "tig_msg.h"
#include "tig_shader.h"
#include "tig_mouse.h"
#include "mainwindow.h"
#include "ui.h"
#include "folderutils.h"

// #include "d3d8/d3d8.h"
#include "d3d8to9/d3d8to9.h"
#include <d3d9.h>
#include <map>
#include <set>

GlobalBool<0x10D250EC> drawFps;
GlobalStruct<tig_text_style, 0x10D24DB0> drawFpsTextStyle;
VideoFuncs videoFuncs;
GlobalStruct<VideoData, 0x11E74580> video;

// Our precompiled header swallows this somehow...
static const DWORD D3D_SDK_VERSION = 32;

static D3DPRESENT_PARAMETERS CreatePresentParams() {
	D3DPRESENT_PARAMETERS presentParams;
	memset(&presentParams, 0, sizeof(presentParams));

	presentParams.BackBufferFormat = D3DFMT_X8R8G8B8;
	// Using discard here allows us to do multisampling.
	presentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParams.hDeviceWindow = video->hwnd;
	presentParams.Windowed = true;
	presentParams.EnableAutoDepthStencil = true;
	presentParams.AutoDepthStencilFormat = D3DFMT_D16;
	presentParams.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	presentParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	return presentParams;
}

static void SetDefaultRenderStates(IDirect3DDevice9 *d3d9Device) {
	/*
	SET DEFAULT RENDER STATES
	*/
	d3d9Device->SetRenderState(D3DRS_ZENABLE, TRUE);
	d3d9Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	d3d9Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	d3d9Device->SetRenderState(D3DRS_LIGHTING, TRUE);

	D3DLIGHT9 light;
	memset(&light, 0, sizeof(light));
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = 1.5f;
	light.Diffuse.g = 1.5f;
	light.Diffuse.b = 1.5f;
	light.Specular.r = 1.0f;
	light.Specular.g = 1.0f;
	light.Specular.b = 1.0f;
	light.Direction.x = -0.70700002f;
	light.Direction.y = -0.866f;
	light.Attenuation0 = 1;
	light.Range = 800;
	d3d9Device->SetLight(0, &light);
	d3d9Device->SetRenderState(D3DRS_AMBIENT, 0);
	d3d9Device->SetRenderState(D3DRS_SPECULARENABLE, 0);
	d3d9Device->SetRenderState(D3DRS_LOCALVIEWER, 0);

	D3DMATERIAL9 material;
	memset(&material, 0, sizeof(material));
	material.Diffuse.r = 1.0f;
	material.Diffuse.g = 1.0f;
	material.Diffuse.b = 1.0f;
	material.Diffuse.a = 1.0f;
	material.Ambient.r = 1.0f;
	material.Ambient.g = 1.0f;
	material.Ambient.b = 1.0f;
	material.Ambient.a = 1.0f;
	material.Power = 50.0f;
	d3d9Device->SetMaterial(&material);

	handleD3dError("SetRenderState", d3d9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
	handleD3dError("SetRenderState", d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
	handleD3dError("SetRenderState", d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));

	handleD3dError("SetTextureStageState", d3d9Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
	handleD3dError("SetTextureStageState", d3d9Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTOP_SELECTARG1));
	handleD3dError("SetTextureStageState", d3d9Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTOP_DISABLE));

	for (DWORD i = 0; i < video->maxActiveTextures; ++i) {
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MINFILTER, 1));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MAGFILTER, 2));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MIPFILTER, 1));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, 0));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MAXMIPLEVEL, 01));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MINFILTER, 1));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_MINFILTER, 1));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_ADDRESSU, 3));
		handleD3dError("SetSamplerState", d3d9Device->SetSamplerState(i, D3DSAMP_ADDRESSV, 3));
		handleD3dError("SetTextureStageState", d3d9Device->SetTextureStageState(i, D3DTSS_TEXTURETRANSFORMFLAGS, 0));
		handleD3dError("SetTextureStageState", d3d9Device->SetTextureStageState(i, D3DTSS_TEXCOORDINDEX, 0));
	}

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	handleD3dError("SetTransform", d3d9Device->SetTransform(D3DTS_TEXTURE0, &identity));

	d3d9Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	d3d9Device->SetRenderState(D3DRS_ALPHAREF, 1);
	d3d9Device->SetRenderState(D3DRS_ALPHAFUNC, 7);
}

static void StoreBackBufferSize(IDirect3DDevice9 *device) {
	/*
	Set backbuffer size
	*/
	IDirect3DSurface9 *backBufferSurface = nullptr;
	HRESULT result;
	if ((result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBufferSurface)) != D3D_OK) {
		handleD3dError("GetBackBuffer", result);
		return;
	}
	D3DSURFACE_DESC backBufferDesc;
	memset(&backBufferDesc, 0, sizeof(backBufferDesc));
	if ((result = backBufferSurface->GetDesc(&backBufferDesc)) != D3D_OK) {
		handleD3dError("GetDesc", result);
		backBufferSurface->Release();
		return;
	}
	backBufferSurface->Release();

	videoFuncs.backbufferWidth = backBufferDesc.Width;
	videoFuncs.backbufferHeight = backBufferDesc.Height;

}

bool CreateSharedVertexBuffers(IDirect3DDevice9* device) {
	/*
	Create several shared buffers. Most of these don't seem to be used much or ever.
	*/
	HRESULT d3dresult;
	IDirect3DVertexBuffer9* vbuffer;
	if ((d3dresult = device->CreateVertexBuffer(
		112, // Space for 4 vertices
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZRHW, // 28 bytes per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	video->blitVBuffer = new Direct3DVertexBuffer8Adapter(vbuffer);

	if ((d3dresult = device->CreateVertexBuffer(
		140, // Space for 5 vertices
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZRHW, // 28 bytes per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	videoFuncs.globalFadeVBuffer = new Direct3DVertexBuffer8Adapter(vbuffer);

	if ((d3dresult = device->CreateVertexBuffer(
		72, // 2 vertices (odd)
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_NORMAL | D3DFVF_XYZ, // 36 byte per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	videoFuncs.sharedVBuffer1 = new Direct3DVertexBuffer8Adapter(vbuffer);

	if ((d3dresult = device->CreateVertexBuffer(
		56, // 2 vertices
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZRHW, // 28 bytes per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	videoFuncs.sharedVBuffer2 = new Direct3DVertexBuffer8Adapter(vbuffer);

	if ((d3dresult = device->CreateVertexBuffer(
		7168, // 256 vertices
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZRHW, // 28 bytes per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	videoFuncs.sharedVBuffer3 = new Direct3DVertexBuffer8Adapter(vbuffer);

	if ((d3dresult = device->CreateVertexBuffer(
		4644, // 129 device
		D3DUSAGE_DYNAMIC,
		D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_NORMAL | D3DFVF_XYZ, // 36 byte per vertex
		D3DPOOL_SYSTEMMEM,
		&vbuffer,
		nullptr)) != D3D_OK) {
		handleD3dError("CreateVertexBuffer", d3dresult);
		return false;
	}
	videoFuncs.sharedVBuffer4 = new Direct3DVertexBuffer8Adapter(vbuffer);
	return true;
}

/*
	This is an attempt at extracting all state reset functionality from TigInitDirect3D.
*/
static bool TigResetDirect3D() {
	D3DXMatrixIdentity(&video->stru_11E75788);
	D3DXMatrixIdentity(&video->matrix_identity);

	IDirect3DDevice9 *device = video->d3dDevice->delegate;
	if (!CreateSharedVertexBuffers(device)) {
		return false;
	}
	StoreBackBufferSize(device);
	SetDefaultRenderStates(device);

	// Seems to be 4 VECTOR3's for the screen corners
	auto fadeScreenRect = videoFuncs.fadeScreenRect.ptr();
	fadeScreenRect[0] = 0;
	fadeScreenRect[1] = 0;
	fadeScreenRect[2] = 0;
	fadeScreenRect[3] = (float)video->current_width;
	fadeScreenRect[4] = 0;
	fadeScreenRect[5] = 0;
	fadeScreenRect[6] = (float)video->current_width;
	fadeScreenRect[7] = (float)video->current_height;
	fadeScreenRect[8] = 0;
	fadeScreenRect[9] = 0;
	fadeScreenRect[10] = (float)video->current_height;
	fadeScreenRect[11] = 0;

	video->unusedCap = 1; // Seems to be ref'd from light_init

	videoFuncs.ReadInitialState();
	memcpy(videoFuncs.renderStates.ptr(), videoFuncs.activeRenderStates.ptr(), sizeof(TigRenderStates));
	
	__asm mov eax, D3DFMT_X8R8G8B8
	if (!videoFuncs.tig_d3d_init_handleformat()) {
		LOG(error) << "Format init failed.";
	}
	
	videoFuncs.create_partsys_vertex_buffers();
	videoFuncs.tigMovieInitialized = true;
	videoFuncs.tig_font_related_init();
	videoFuncs.matrix_related(videoFuncs.tig_matrices2.ptr());
	videoFuncs.buffersFreed = false;

	// This is always the same pointer although it's callback 2 of the GameStartConfig	
	videoFuncs.GameCreateVideoBuffers();
	
	return true;
}

void ResizeBuffers(int width, int height) {

	if (!video->d3dDevice) {
		return;
	}

	auto device = video->d3dDevice->delegate;

	// TODO: Handle non d3d9ex case
	auto presentParams = CreatePresentParams();
	device->ResetEx(&presentParams, nullptr);

	video->current_width = width;
	video->current_height = height;
	videoFuncs.matrix_related(videoFuncs.tig_matrices2.ptr());

	videoFuncs.GameFreeVideoBuffers();
	videoFuncs.GameCreateVideoBuffers();

	temple_set<0x10D24E14>(width);

	// Mouse cursor disasppers after resizing
	mouseFuncs.RefreshCursor();

	videoFuncs.GamelibResizeScreen(video->adapter, 
		width,
		height,
		video->current_bpp,
		video->current_refresh,
		videoFuncs.currentFlags);
}

bool ReadCaps(IDirect3DDevice9Ex* device, uint32_t minTexWidth, uint32_t minTexHeight) {

	HRESULT d3dresult;
	D3DCAPS9 caps;
	if ((d3dresult = device->GetDeviceCaps(&caps)) != D3D_OK) {
		LOG(error) << "Unable to retrieve device caps.";
		handleD3dError("GetDeviceCaps", d3dresult);
		return false;
	}
	video->maxActiveLights = min(8, caps.MaxActiveLights);

	/*
	Several sanity checks follow
	*/
	if (!(caps.SrcBlendCaps & D3DPBLENDCAPS_SRCALPHA)) {
		LOG(info) << "source D3DPBLENDCAPS_SRCALPHA is missing";
	}
	if (!(caps.SrcBlendCaps & D3DPBLENDCAPS_ONE)) {
		LOG(info) << "source D3DPBLENDCAPS_ONE is missing";
	}
	if (!(caps.SrcBlendCaps & D3DPBLENDCAPS_ZERO)) {
		LOG(info) << "source D3DPBLENDCAPS_ZERO is missing";
	}
	if (!(caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA)) {
		LOG(info) << "destination D3DPBLENDCAPS_INVSRCALPHA is missing";
	}
	if (!(caps.DestBlendCaps & D3DPBLENDCAPS_ONE)) {
		LOG(info) << "destination D3DPBLENDCAPS_ONE is missing";
	}
	if (!(caps.DestBlendCaps & D3DPBLENDCAPS_ZERO)) {
		LOG(info) << "destination D3DPBLENDCAPS_ZERO is missing";
	}

	if (caps.MaxSimultaneousTextures < 4) {
		LOG(info) << "less than 4 active textures possible: " << caps.MaxSimultaneousTextures;
	}
	if (caps.MaxTextureBlendStages < 4) {
		LOG(info) << "less than 4 texture blend stages possible: " << caps.MaxTextureBlendStages;
	}
	video->maxActiveTextures = 4; // We do not accept less than 4

	if (!(caps.TextureOpCaps & D3DTOP_DISABLE)) {
		LOG(info) << "texture op D3DTOP_DISABLE is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_SELECTARG1)) {
		LOG(info) << "texture op D3DTOP_SELECTARG1 is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_SELECTARG2)) {
		LOG(info) << "texture op D3DTOP_SELECTARG2 is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_BLENDTEXTUREALPHA)) {
		LOG(info) << "texture op D3DTOP_BLENDTEXTUREALPHA is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_BLENDCURRENTALPHA)) {
		LOG(info) << "texture op D3DTOP_BLENDCURRENTALPHA is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_MODULATE)) {
		LOG(info) << "texture op D3DTOP_MODULATE is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_ADD)) {
		LOG(info) << "texture op D3DTOP_ADD is missing";
	}
	if (!(caps.TextureOpCaps & D3DTOP_MODULATEALPHA_ADDCOLOR)) {
		LOG(info) << "texture op D3DTOP_MODULATEALPHA_ADDCOLOR is missing";
	}
	if (caps.MaxTextureWidth < minTexWidth || caps.MaxTextureHeight < minTexHeight) {
		LOG(info) << "minimum texture resolution of " << minTexWidth << "x" << minTexHeight
				<< " is not supported. Supported: " << caps.MaxTextureWidth << "x" << caps.MaxTextureHeight;
		return false;
	}

	/*
		Vermutlich kein Effekt
	*/
	if (video->makesSthLarger) {
		video->neverReadFlag1 = 4096;
		video->neverReadFlag2 = 16;
	} else {
		video->neverReadFlag1 = 2048;
		video->neverReadFlag2 = 0;
	}

	if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0) {
		LOG(info) << "Textures must be power of two";
		video->capPowerOfTwoTextures = true;
	}
	if ((caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0) {
		LOG(info) << "Textures must be square";
		video->capSquareTextures = true;
	}
	return true;
}

static bool TigInitDirect3D(TigConfig* settings) {

	HRESULT d3dresult;

	if (settings->bpp < 32) {
		LOG(error) << "BPP settings must be 32-bit";
		return false;
	}

	/*
		Set some global flags.
	*/
	video->unusedCap = 0;
	video->makesSthLarger = 0;
	video->capPowerOfTwoTextures = 0;
	video->capSquareTextures = 0;
	video->neverReadFlag1 = 0;
	video->neverReadFlag2 = 0;
	video->dword_11E7572C = 0;
	video->enableMipMaps = (settings->flags & SF_MIPMAPPING) != 0;

	IDirect3D9Ex* d3d9 = nullptr;
	if (config.useDirect3d9Ex) {
		d3dresult = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9);
		if (d3dresult != D3D_OK) {
			LOG(error) << "Unable to create Direct3D9Ex device.";
			handleD3dError("TigInitDirect3D", d3dresult);
			return false;
		}
	} else {
		d3d9 = static_cast<IDirect3D9Ex*>(Direct3DCreate9(D3D_SDK_VERSION));
		if (!d3d9) {
			LOG(error) << "Unable to create Direct3D9 device.";
			return false;
		}
	}
	video->d3d = new Direct3D8Adapter;
	video->d3d->delegate = d3d9;

	/** START OF OLD WINDOWED INIT */
	IDirect3DDevice9Ex* d3d9Device = nullptr;
	// At this point we only do a GetDisplayMode to check the resolution. We could also do this elsewhere
	D3DDISPLAYMODE displayMode;
	d3dresult = d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode);
	if (d3dresult != D3D_OK) {
		LOG(error) << "Unable to query display mode for primary adapter.";
		handleD3dError("GetAdapterDisplayMode", d3dresult);
		return false;
	}

	// We need at least 1024x768
	if (displayMode.Width < 1024 || displayMode.Height < 768) {
		LOG(error) << "You need at least a display resolution of 1024x768.";
		return false;
	}

	// This is only really used by alloc_texture_mem and the init func
	video->adapterformat = D3DFMT_X8R8G8B8;
	video->current_bpp = 32;
	settings->bpp = 32;

	/*
		Check for linear->srgb conversion support (also known as gamma correction)
		*/
	D3DCAPS9 caps;
	if ((d3dresult = d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) != D3D_OK) {
		LOG(error) << "Unable to retrieve device caps.";
		handleD3dError("GetDeviceCaps", d3dresult);
		return false;
	}

	enableLinearPresent = false;
	if ((caps.Caps3 & D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION) != 0) {
		LOG(info) << "Automatic gamma corection is supported by driver.";
		enableLinearPresent = true;
	}
	d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);

	auto presentParams = CreatePresentParams();

	// presentParams.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES;
	// presentParams.MultiSampleQuality = 0;

	// Nvidia drivers seriously barf on D3d9ex if we use software vertex processing here, as ToEE specifies.
	// I think we are safe with hardware vertex processing, since HW T&L has been standard for more than 10 years.
	if (config.useDirect3d9Ex) {
		LOG(info) << "Creating Direct3D9Ex device.";
		d3dresult = d3d9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, video->hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &presentParams, nullptr, &d3d9Device);
	} else {
		LOG(info) << "Creating Direct3D9 device.";
		d3dresult = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, video->hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &presentParams, reinterpret_cast<IDirect3DDevice9**>(&d3d9Device));
	}
	if (d3dresult != D3D_OK) {
		LOG(error) << "Unable to create Direct3d9 device.";
		handleD3dError("CreateDevice", d3dresult);
		return false;
	}
	video->d3dDevice = new Direct3DDevice8Adapter;
	video->d3dDevice->delegate = d3d9Device;

	video->fullscreen = false;
	video->unk2 = false;
	video->gammaSupported = false;

	// TODO: color bullshit is not yet done (tig_d3d_init_handleformat et al)

	/** END OF OLD WINDOWED INIT */
	videoFuncs.currentFlags = settings->flags;

	// Get the device caps for real this time.
	if (!ReadCaps(d3d9Device, settings->minTexWidth, settings->minTexHeight)) {
		return false;
	}

	if (!TigResetDirect3D()) {
		return false;
	}

	return true;
}

int __cdecl HookedCleanUpBuffers() {
	LOG(info) << "Buffer cleanup called";
	return 0;
}

int __cdecl HookedSetVideoMode(int adapter, int nWidth, int nHeight, int bpp, int refresh, int flags) {
	LOG(info) << "set_video_mode adapter=" << adapter << " w=" << nWidth << " h="
			<< nHeight << " bpp=" << bpp << " refresh=" << refresh << " flags=" << flags;
	return 0;

	LOG(info) << "Buffers Freed: " << videoFuncs.buffersFreed;

	// We probabl need to create a shadow for this...
	bool changed =
		video->adapter != adapter
		|| video->current_bpp != bpp
		|| video->current_width != nWidth
		|| video->current_height != nHeight;

	if (!changed) {
		return 0;
	}

	nWidth = video->current_width;
	nHeight = video->current_height;
	bpp = video->current_bpp;

	video->current_width = nWidth;
	video->current_height = nHeight;
	video->current_bpp = 32; // Never anything else
	// Adapter changes will not happen...
	video->adapter = adapter;
	videoFuncs.currentFlags = flags;

	if (!videoFuncs.buffersFreed) {
		videoFuncs.CleanUpBuffers();
	}

	video->halfWidth = nWidth * 0.5f;
	video->halfHeight = nHeight * 0.5f;

	// this was the old reset stuff for the window
	// RECT rect;
	// GetWindowRect(video->hwnd, &rect);
	// MoveWindow(video->hwnd, 0, 0, nWidth, nHeight, 0);

	TigResetDirect3D();

	videoFuncs.create_partsys_vertex_buffers();
	videoFuncs.tigMovieInitialized = true;
	videoFuncs.tig_font_related_init();
	videoFuncs.matrix_related(videoFuncs.tig_matrices2.ptr());
	videoFuncs.buffersFreed = false;

	// This is always the same pointer although it's callback 2 of the GameStartConfig
	videoFuncs.GameCreateVideoBuffers();

	/*
		Basically this is the inverse of CleanBuffers. Sadly it's not an easily callable function.
	*/
	/*config.bpp = bpp;
	config.width = nWidth;
	config.height = nHeight;
	config.framelimit = refresh;
	backbuffer_width = nWidth;
	backbuffer_height = nHeight;

	if (tig_d3d_init(&config, flags))
	{
		result = return_EAX_0();
		if ( result )
		return result;
		create_partsys_vertex_buffers();
		tig_movie_set_initialized();
		tig_font_related_init();
		matrix_related(&matrix_related_2);
		buffers_freed = 0;
		if ( set_video_mode_callback )
		set_video_mode_callback();
		v8 = (long double)current_width;
		LODWORD(screen_rect[0].x) = 0;
		LODWORD(screen_rect[0].y) = 0;
		LODWORD(screen_rect[0].z) = 0;
		screen_rect[1].x = v8;
		LODWORD(screen_rect[1].y) = 0;
		screen_rect[2].x = v8;
		LODWORD(screen_rect[1].z) = 0;
		v9 = (long double)current_height;
		LODWORD(screen_rect[2].z) = 0;
		LODWORD(screen_rect[3].x) = 0;
		LODWORD(screen_rect[3].z) = 0;
		screen_rect[2].y = v9;
		screen_rect[3].y = v9;
	}*/

	return 0;
}

int __cdecl VideoStartup(TigConfig* settings) {
	memset(video, 0, 4796);

	bool windowed = config.windowed;

	video->adapter = 0;

	// create window call
	if (!CreateMainWindow(settings)) {
		return 17;
	}

	if (!TigInitDirect3D(settings)) {
		video->hwnd = 0;
		return 17;
	}

	video->width = settings->width;
	video->height = settings->height;
	video->halfWidth = video->width * 0.5f;
	video->halfHeight = video->height * 0.5f;

	if (settings->flags & SF_FPS) {
		drawFps = true;
		temple_set<0x10D24DB0>(0);
		temple_set<0x10D24DB4>(5);
		temple_set<0x10D24DB8>(2);
		temple_set<0x10D24DBC>(0);

		temple_set<0x10D24DD8>(8);
		temple_set<0x10D24DDC>(-1);
		temple_set<0x10D24DE4>(temple_address<0x103008F4>());
		temple_set<0x10D24DE8>(temple_address<0x103008F4>());
		temple_set<0x10D24DEC>(temple_address<0x10300904>());
		temple_set<0x10D24DF0>(temple_address<0x103008F4>());
	} else {
		drawFps = false;
	}

	// Seems always enabled in default config and never read
	temple_set<0x11E7570C, int>((settings->flags & 4) != 0);

	video->current_bpp = settings->bpp;

	temple_set<0x10D250E0, int>(0);
	temple_set<0x10D250E4, int>(1);
	temple_set<0x10300914, int>(-1);
	
	// Unused mkscreenshot related pointer
	// temple_set<0x10D2511C, int>(0);

	/*
		This stuff doesn't really seem to be used.
	*/
	uint32_t v3 = 0x10D24CAC;
	do {
		temple_set(v3, 0);
		v3 += 12;
	} while (v3 < 0x10D24D6C);

	v3 = 0x10D24C8C;
	do {
		temple_set(v3, 0);
		v3 += 8;
	} while (v3 < 0x10D24CAC);

	// These may actually no longer be needed since we replaced the referencing subsystems directly
	temple_set<0x10D25134>(settings->createBuffers);
	temple_set<0x10D25138>(settings->freeBuffers);

	memcpy(temple_address<0x11E75840>(), settings, 0x4C);

	return 0;
}

struct TempleTextureType {
	D3DFORMAT d3dFormat;
	int fallbackIndex;
};

struct TempleTextureTypeTable {
	TempleTextureType formats[8];
};

GlobalStruct<TempleTextureTypeTable, 0x102A05A8> textureFormatTable;

bool __cdecl AllocTextureMemory(Direct3DDevice8Adapter* adapter, int w, int h, int flags, Direct3DTexture8Adapter** textureOut, int* textureTypePtr) {
	auto device = adapter->delegate;

	int levels = 1;
	D3DFORMAT format;
	IDirect3DTexture9* texture = nullptr;

	auto textureType = *textureTypePtr;
	auto desiredType = textureFormatTable->formats[textureType];
	format = desiredType.d3dFormat;

	DWORD usage = D3DUSAGE_DYNAMIC;
	// d3d9ex does not support managed anymore, but default has better guarantees now anyway
	D3DPOOL pool = config.useDirect3d9Ex ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	if (flags & 0x40) {
		usage = D3DUSAGE_RENDERTARGET;
		pool = D3DPOOL_DEFAULT;
	}

	if (flags & 0x20 && video->enableMipMaps) {
		levels = 2;
	}

	HRESULT result;
	if ((result = device->CreateTexture(
		w,
		h,
		levels,
		usage,
		format,
		pool,
		&texture,
		nullptr
	)) != D3D_OK) {
		handleD3dError("CreateTexture", result);
		return false;
	}

	*textureOut = new Direct3DTexture8Adapter(texture);

	return true;
}

/*
	The original function could not handle PCs with >4GB ram
*/
void GetSystemMemory(int* totalMem, int* availableMem) {
	MEMORYSTATUS status;

	GlobalMemoryStatus(&status);

	// Max 1GB because of process space limitations
	*totalMem = min(1024 * 1024 * 1024, status.dwTotalPhys);
	*availableMem = min(1024 * 1024 * 1024, status.dwAvailPhys);
}

bool __cdecl HookedPresentFrame() {
	static bool dumpedPacketStructs = true;

	/*void(__cdecl*ShowConsole)() = (void(__cdecl*)()) temple_address(0x101DF7C0);
	ShowConsole();*/

	if (!dumpedPacketStructs) {
		for (const auto& node : shaderRegistry) {
			LOG(info) << "Registered shader: " << node.data->name;
		}

		auto defaultShader = shaderRegistry.get(0);
		if (defaultShader) {
			LOG(info) << "Found default shader!";
		} else {
			LOG(info) << "Did not find default shader!";
		}

		LOG(info) << "=================================================================================================";
		auto listEntry = *idxTablesList.ptr();
		while (listEntry) {
			LOG(info) << "Index Table allocated @ " << listEntry->sourceFile << ":" << listEntry->lineNumber;

			auto table = listEntry->table;
			LOG(info) << "   Buckets: " << table->bucketCount << " Item Count: " << table->itemCount << " Item Size: "
					<< table->itemSize << " Address: " << format("%x") % reinterpret_cast<uint32_t>(table);

			listEntry = listEntry->next;
		}
	}

	// tig_font_extents extents;
	// extents.x = 100;
	// extents.y = 100;
	// tigFont.Draw("Hello World!\nHow are you today?", &extents, drawFpsTextStyle);

	return videoFuncs.PresentFrame();
}

// They keycode parameter can be ignored
void __cdecl TakeScreenshot(int) {

	if (!video->d3dDevice) {
		return;
	}
		
	// Calculate size of screenshot
	RECT rect;
	GetWindowRect(video->hwnd, &rect);
	auto width = rect.right - rect.left;
	auto height = rect.bottom - rect.top;

	// Calculate output filename
	auto folder = GetScreenshotFolder();
	wstring path;
	if (!folder.empty()) {
		folder.append(L"\\");
	}
	for (auto i = 1; i <= 9999; ++i) {
		path = (wformat(L"%sToEE%04d.jpg") % folder % i).str();
		if (!PathFileExistsW(path.c_str())) {
			break;
		}
	}
	
	// Take screenshot
	auto device = video->d3dDevice->delegate;
	IDirect3DSurface9* surface;
	device->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SCRATCH, &surface, nullptr);
	if (surface) {
		// device->GetFrontBufferData(0, surface);
		device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &surface);
		D3DXSaveSurfaceToFileW(path.c_str(), D3DXIFF_JPG, surface, nullptr, nullptr);
		surface->Release();
	}

}

void hook_graphics() {
	/*
		These assertions are based on mallocs or memsets in the code that allow us to deduce the original struct
		size.
	*/
	static_assert(sizeof(TigRenderStates) == 0x1C4, "TigRenderStates has the wrong size.");
	static_assert(sizeof(VideoData) == 4796, "Video Data struct has the wrong size.");

	// We only differ between borderless and normal window mode.
	videoFuncs.startupFlags = SF_WINDOW;

	// Hook into present frame to do after-frame stuff
	MH_CreateHook(videoFuncs.PresentFrame, HookedPresentFrame, reinterpret_cast<LPVOID*>(&videoFuncs.PresentFrame));

	MH_CreateHook(videoFuncs.SetVideoMode, HookedSetVideoMode, reinterpret_cast<LPVOID*>(&videoFuncs.SetVideoMode));
	MH_CreateHook(videoFuncs.CleanUpBuffers, HookedCleanUpBuffers, reinterpret_cast<LPVOID*>(&videoFuncs.CleanUpBuffers));

	// We hook the entire video subsystem initialization function
	MH_CreateHook(temple_address<0x101DC6E0>(), VideoStartup, nullptr);
	MH_CreateHook(temple_address<0x101DBC80>(), AllocTextureMemory, nullptr);
	MH_CreateHook(temple_address<0x101E0750>(), GetSystemMemory, nullptr);	
	MH_CreateHook(temple_address<0x101DBD80>(), TakeScreenshot, nullptr);

	hook_movies();
}
