
#include "stdafx.h"
#include "ui_render.h"
#include "util/addresses.h"
#include "tig/tig_font.h"
#include "tig/tig_texture.h"

#pragma pack(push, 1)
struct DrawTexturedQuadArgs {
	int flags = 0;
	int textureId = 0;
	int field8;
	void *texBuffer = 0;
	int shaderId;
	const TigRect *srcRect;
	const TigRect *destRect;	
	ColorRect *vertexColors;
	float vertexZ;
	int field24; // May be padding from here on out
	int field28;
	int field2c;
};
#pragma pack(pop)

static struct UiRenderFuncs : AddressTable {
	
	/*
		This is one of the primary functions used to draw textured quads for UI purposes.
		It's a pretty flexible function. Not all arguments have been discovered yet.
	*/
	int (__cdecl *DrawTexturedQuad)(const DrawTexturedQuadArgs &);
	
	/*
		Simply calls TIG Font Draw but positions the given rectangle relative to the given widget.
	*/
	bool (__cdecl *DrawTextInWidget)(int widgetId, const char *text, const TigRect &rect, const TigTextStyle &style);



	UiRenderFuncs() {
		rebase(DrawTexturedQuad, 0x101D9300);
		rebase(DrawTextInWidget, 0x101F87C0);
	}
} uiRenderFuncs;

int UiRenderer::RegisterTexture(const string& path) {

	int textureId;
	if (textureFuncs.RegisterTexture(path.c_str(), &textureId) != 0) {
		// TODO: this should probably instead of failing, return "bad art" or something similar
		throw TempleException(format("Unable to register texture {}", path));
	}

	return textureId;

}

Texture UiRenderer::LoadTexture(const int textureId) {

	TigTextureRegistryEntry textureEntry;
	if (textureFuncs.LoadTexture(textureId, &textureEntry) != 0) {
		// TODO: Probably should return bad art here
		throw TempleException(format("Unable to load texture {}", textureId));
	}

	Texture result;
	result.id = textureEntry.textureId;
	result.width = textureEntry.width;
	result.height = textureEntry.height;
	result.rect = textureEntry.rect;
	strcpy(result.path, textureEntry.name);
	result.unk = textureEntry.set_to_true_in_shader;
	result.field_124 = textureEntry.field_124;
	result.buffer = textureEntry.buffer;

	return result;
	
}

Texture UiRenderer::LoadTexture(const string& path) {
	return LoadTexture(RegisterTexture(path));
}

void UiRenderer::DrawTexture(int texId, const TigRect &destRect) {

	DrawTexturedQuadArgs args;
	args.destRect = &destRect;

	// This function assumes dest rect encompasses the entire src rect at 0,0
	TigRect srcRect(0, 0, destRect.width, destRect.height);
	args.srcRect = &srcRect;

	args.textureId = texId;

	if (uiRenderFuncs.DrawTexturedQuad(args)) {
		logger->warn("DrawTexturedQuad failed!");
	}

}

void UiRenderer::DrawTexture(IDirect3DTexture9 *texture,
	int texWidth,
	int texHeight,
	const TigRect &srcRect,
	const TigRect &destRect) {

	static Direct3DTexture8Adapter adapter;

	DrawTexturedQuadArgs args;
	args.flags = 0x80; // Indicates we dont use a texture id, but a "tig buffer"
	args.destRect = &destRect;
	args.srcRect = &srcRect;

	TigBuffer buffer;
	buffer.d3dtexture = &adapter;
	adapter.delegate = texture; // We have to check if ToEE ever calls Release() on this
	buffer.texturewidth = texWidth;
	buffer.textureheight = texHeight;

	args.texBuffer = &buffer;

	if (uiRenderFuncs.DrawTexturedQuad(args)) {
		logger->warn("DrawTexturedQuad failed!");
	}

}

void UiRenderer::PushFont(PredefinedFont font) {
	switch (font) {
	case PredefinedFont::ARIAL_10:
		tigFont.PushFont("arial-10", 10, true);
		break;
	case PredefinedFont::ARIAL_12: 
		tigFont.PushFont("arial-12", 12, true);
		break;
	case PredefinedFont::ARIAL_BOLD_10: 
		tigFont.PushFont("arial-bold-10", 10, true);
		break;
	case PredefinedFont::ARIAL_BOLD_24: 
		tigFont.PushFont("arial-bold-24", 24, true);
		break;
	case PredefinedFont::PRIORY_12: 
		tigFont.PushFont("priory-12", 12, true);
		break;
	case PredefinedFont::SCURLOCK_48: 
		tigFont.PushFont("scurlock-48", 48, true);
		break;
	default: 
		throw TempleException("Unknown font literal was used!");
	}
}

void UiRenderer::PushFont(const std::string& faceName, int pixelSize, bool antialiased) {
	tigFont.PushFont(faceName.c_str(), pixelSize, antialiased);
}

void UiRenderer::PopFont() {
	tigFont.PopFont();
}

bool UiRenderer::DrawTextInWidget(int widgetId, const string &text, const TigRect &rect, const TigTextStyle &style) {
	return uiRenderFuncs.DrawTextInWidget(widgetId, text.c_str(), rect, style);
}

bool UiRenderer::RenderText(const string &text, TigRect &rect, const TigTextStyle &style) {
	return tigFont.Draw(text.c_str(), rect, style) == 0;
}

TigRect UiRenderer::MeasureTextSize(const string &text, const TigTextStyle &style, int maxWidth, int maxHeight) {
	TigFontMetrics metrics;
	metrics.text = text.c_str();
	metrics.width = maxWidth;
	metrics.height = maxHeight;
	tigFont.Measure(style, metrics);
	TigRect result;
	result.x = 0;
	result.y = 0;
	result.width = metrics.width;
	result.height = metrics.height;
	return result;
}
