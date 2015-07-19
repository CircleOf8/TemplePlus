
#pragma once

#include "tig/tig.h"

struct TigTextStyle;
struct TigBuffer;

// Fonts shipping with the base game
enum class PredefinedFont {
	ARIAL_10,
	ARIAL_12,
	ARIAL_BOLD_10,
	ARIAL_BOLD_24,
	PRIORY_12,
	SCURLOCK_48
};

// Represents a texture as it is loaded and maintained by ToEE
struct Texture {
	bool unk;
	int id; // a.k.a artId
	char path[260];
	int width; // This is the original height BEFORE power of two stuff
	int height;
	TigRect rect;
	int field_124;
	TigBuffer *buffer;
};

/*
	Helper methods for rendering UI elements.
*/
class UiRenderer {
public:

	/*
		Assigns a unique id to the given texture path in the texture registry.
		Most drawing functions takes texture ids instead of texture paths.
	*/	
	static int RegisterTexture(const string &path);

	/*
		Loads a previously registered texture given its ID.
	*/
	static Texture LoadTexture(const int textureId);

	/*
		Loads a texture given its path. This will automatically register a texture
		if it hasn't been registered before.
	*/
	static Texture LoadTexture(const string &path);

	/*
		Draws the full texture in the given screen rectangle.
	*/
	static void DrawTexture(int texId, const TigRect &destRect);

	/*
		Draws an arbitrary Direct 3D texture on screen. The real texture size has to be given so the
		UV coordinates can be calculated correctly.
	*/
	static void DrawTexture(IDirect3DTexture9 *texture, 
		int texWidth,
		int texHeight,
		const TigRect &srcRect,
		const TigRect &destRect);
	
	/*
		Pushes a font for further text rendering.
	*/
	static void PushFont(PredefinedFont font);

	/*
		Pushes a custom font for further text rendering.
	*/
	static void PushFont(const std::string &faceName, int pixelSize, bool antialiased = true);

	/*
		Pops the last pushed font from the font stack.
	*/
	static void PopFont();

	/*
		Draws text positioned relative to widget.
	*/
	static bool DrawTextInWidget(int widgetId, const string &text, const TigRect &rect, const TigTextStyle &style);

	/*
		Draws text positioned in screen coordinates. Width of rectangle may be 0 to cause automatic
		measurement of the text.
	*/
	static bool RenderText(const string &text, TigRect &rect, const TigTextStyle &style);

	/*
		Measures the given text and returns the bounding rect.
	*/
	static TigRect MeasureTextSize(const string &text, const TigTextStyle &style, int maxWidth = 0, int maxHeight = 0);

};
