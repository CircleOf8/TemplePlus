#include "stdafx.h"
#include "widget_content.h"

#include <infrastructure/binaryreader.h>
#include <infrastructure/stringutil.h>
#include <infrastructure/vfs.h>

#include <graphics/device.h>
#include <graphics/shaperenderer2d.h>

#include "widget_styles.h"
#include "fonts/fonts.h"

#include "tig/tig_startup.h"

#include "ui/ui_render.h"

// Render this font using the old engine
static eastl::string sScurlockFont = "Scurlock";
static eastl::string sPriory12 = "Priory-12";
static eastl::string sArialBold10 = "Arial-Bold-ToEE";

static TigTextStyle GetScurlockStyle(const gfx::Brush &brush) {
	static ColorRect sColorRect;
	sColorRect.topLeft = brush.primaryColor;
	sColorRect.topRight = brush.primaryColor;
	if (brush.gradient) {
		sColorRect.bottomLeft = brush.secondaryColor;
		sColorRect.bottomRight = brush.secondaryColor;
	} else {
		sColorRect.bottomLeft = brush.primaryColor;
		sColorRect.bottomRight = brush.primaryColor;
	}

	static ColorRect sShadowColor(XMCOLOR{ 0, 0, 0, 0.5 });

	TigTextStyle textStyle(&sColorRect);
	textStyle.leading = 1;
	textStyle.kerning = 0;
	textStyle.tracking = 10;
	textStyle.flags = TTSF_DROP_SHADOW;
	textStyle.shadowColor = &sShadowColor;

	return textStyle;
}

static TigTextStyle GetPrioryStyle(const gfx::Brush &brush) {
	static ColorRect sColorRect;
	sColorRect.topLeft = brush.primaryColor;
	sColorRect.topRight = brush.primaryColor;
	if (brush.gradient) {
		sColorRect.bottomLeft = brush.secondaryColor;
		sColorRect.bottomRight = brush.secondaryColor;
	}
	else {
		sColorRect.bottomLeft = brush.primaryColor;
		sColorRect.bottomRight = brush.primaryColor;
	}

	static ColorRect sShadowColor(XMCOLOR{ 0, 0, 0, 0.5 });

	TigTextStyle textStyle(&sColorRect);
	textStyle.leading = 0;
	textStyle.kerning = 1;
	textStyle.tracking = 3;
	textStyle.flags = TTSF_DROP_SHADOW;
	textStyle.shadowColor = &sShadowColor;

	return textStyle;
}

static TigTextStyle GetArialBoldStyle(const gfx::Brush &brush) {
	static ColorRect sColorRect;
	sColorRect.topLeft = brush.primaryColor;
	sColorRect.topRight = brush.primaryColor;
	if (brush.gradient) {
		sColorRect.bottomLeft = brush.secondaryColor;
		sColorRect.bottomRight = brush.secondaryColor;
	}
	else {
		sColorRect.bottomLeft = brush.primaryColor;
		sColorRect.bottomRight = brush.primaryColor;
	}

	static ColorRect sShadowColor(XMCOLOR{ 0, 0, 0, 0.5 });

	TigTextStyle textStyle(&sColorRect);
	textStyle.leading = 0;
	textStyle.kerning = 1;
	textStyle.tracking = 3;
	textStyle.flags = TTSF_DROP_SHADOW;
	textStyle.shadowColor = &sShadowColor;

	return textStyle;
}

WidgetContent::WidgetContent()
{
	mContentArea = { 0, 0, 0, 0 };
	mPreferredSize = { 0, 0 };
}

WidgetImage::WidgetImage(const std::string &path)
{
	SetTexture(path);
}

void WidgetImage::Render()
{
	auto &renderer = tig->GetShapeRenderer2d();
	renderer.DrawRectangle(
		(float) mContentArea.x, 
		(float) mContentArea.y,
		(float) mContentArea.width,
		(float) mContentArea.height,
		*mTexture
	);
}

void WidgetImage::SetTexture(const std::string & path)
{
	mPath = path;
	mTexture = tig->GetRenderingDevice().GetTextures().Resolve(path, false);
	if (mTexture->IsValid()) {
		mPreferredSize = mTexture->GetSize();
	}
}

WidgetText::WidgetText()
{
	mText.defaultStyle = widgetTextStyles->GetDefaultStyle();
}

WidgetText::WidgetText(const std::string & text, const std::string &styleId)
{
	mText.defaultStyle = widgetTextStyles->GetTextStyle(styleId.c_str());
	SetText(text);
}

void WidgetText::SetText(const std::string & text)
{
	// TODO: Process mes file placeholders
	mText.text = local_to_ucs2(uiAssets->ApplyTranslation(text));
	UpdateBounds();
}

void WidgetText::SetStyleId(const std::string & id)
{
	mStyleId = id;
	mText.defaultStyle = widgetTextStyles->GetTextStyle(id.c_str());
	UpdateBounds();
}

const std::string & WidgetText::GetStyleId() const
{
	return mStyleId;
}

void WidgetText::SetStyle(const gfx::TextStyle & style)
{
	mText.defaultStyle = style;
	UpdateBounds();
}

const gfx::TextStyle & WidgetText::GetStyle() const
{
	return mText.defaultStyle;
}

void WidgetText::SetCenterVertically(bool isCentered) {
	mCenterVertically = isCentered;
}

void WidgetText::Render()
{
	if (mText.defaultStyle.fontFace == sScurlockFont) {

		RenderWithPredefinedFont(PredefinedFont::SCURLOCK_48, GetScurlockStyle(mText.defaultStyle.foreground));

	} else if (mText.defaultStyle.fontFace == sPriory12) {

		RenderWithPredefinedFont(PredefinedFont::PRIORY_12, GetPrioryStyle(mText.defaultStyle.foreground));

	} else if (mText.defaultStyle.fontFace == sArialBold10) {

		RenderWithPredefinedFont(PredefinedFont::ARIAL_BOLD_10, GetArialBoldStyle(mText.defaultStyle.foreground));

	} else {

		auto area = mContentArea; // Will be modified below

		if (mCenterVertically) {
			gfx::TextMetrics metrics;
			tig->GetRenderingDevice().GetTextEngine().MeasureText(mText, metrics);
			area = TigRect(area.x,
				area.y + (area.height - metrics.height) / 2,
				area.width, metrics.height);
		}
		
		tig->GetRenderingDevice().GetTextEngine().RenderText(area, mText);

	}
}

void WidgetText::UpdateBounds()
{
	if (mText.defaultStyle.fontFace == sScurlockFont) {

		UpdateBoundsWithPredefinedFont(PredefinedFont::SCURLOCK_48, GetScurlockStyle(mText.defaultStyle.foreground));

	} else if (mText.defaultStyle.fontFace == sPriory12) {

		UpdateBoundsWithPredefinedFont(PredefinedFont::PRIORY_12, GetPrioryStyle(mText.defaultStyle.foreground));

	} else if (mText.defaultStyle.fontFace == sArialBold10) {

		UpdateBoundsWithPredefinedFont(PredefinedFont::ARIAL_BOLD_10, GetArialBoldStyle(mText.defaultStyle.foreground));

	} else {
		gfx::TextMetrics textMetrics;
		tig->GetRenderingDevice().GetTextEngine().MeasureText(mText, textMetrics);
		mPreferredSize.width = textMetrics.width;
		mPreferredSize.height = textMetrics.height;
	}

}

void WidgetText::RenderWithPredefinedFont(PredefinedFont font, TigTextStyle textStyle)
{
	auto area = mContentArea; // Will be modified below

	UiRenderer::PushFont(font);

	auto text = ucs2_to_local(mText.text);

	if (mText.defaultStyle.align == gfx::TextAlign::Center) {
		textStyle.flags |= TTSF_CENTER;
		if (mCenterVertically) {
			auto textMeas = UiRenderer::MeasureTextSize(text, textStyle);
			area = TigRect(area.x + (area.width - textMeas.width) / 2,
				area.y + (area.height - textMeas.height) / 2,
				textMeas.width, textMeas.height);
		}
	}
	tigFont.Draw(text.c_str(), area, textStyle);

	UiRenderer::PopFont();
}

void WidgetText::UpdateBoundsWithPredefinedFont(PredefinedFont font, TigTextStyle textStyle)
{

	if (mText.defaultStyle.align == gfx::TextAlign::Center) {
		textStyle.flags |= TTSF_CENTER;
	}
	UiRenderer::PushFont(font);
	auto rect = UiRenderer::MeasureTextSize(ucs2_to_local(mText.text), textStyle, 0, 0);
	UiRenderer::PopFont();
	if (mText.defaultStyle.align == gfx::TextAlign::Center) {
		// Return 0 here to be in sync with the new renderer
		mPreferredSize.width = 0;
	}
	else {
		mPreferredSize.width = rect.width;
	}
	mPreferredSize.height = rect.height;

}
