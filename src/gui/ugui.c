#include "ugui.h"

#include <nanovg.h>
#include "engine.h"

//
// vars
//

const float g_bar_height = 60.0f;
const float g_icon_size = 42.0f;
const float g_bookmark_width = 110.0f;
const float g_bookmark_pointyness = 30.0f;

//
// public api
//

void ugui_mainmenu_bar(engine_t *engine) {
	const float W = engine->window_width;
	NVGcontext *vg = engine->vg;

	// bar bg
	nvgBeginPath(vg);
	nvgRect(vg, 0.0f, 0.0f, W, g_bar_height);
	NVGpaint p = nvgLinearGradient(vg, 0.0f, 90.0f, 0, 55.0f, nvgRGBf(0.04f, 0.14f, 0.23f), nvgRGBf(0.01f, 0.09f, 0.18f));
	nvgFillPaint(vg, p);
	nvgFill(vg);
	// bar outline
	nvgBeginPath(vg);
	nvgMoveTo(vg, 0.0f, g_bar_height);
	nvgLineTo(vg, W, g_bar_height);
	nvgStrokeColor(vg, nvgRGBf(0.01f, 0.01f, 0.02f));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);
}

void ugui_mainmenu_bookmark(engine_t *engine, float x) {
	NVGcontext *vg = engine->vg;
	// shadow
	const float sh_x = 2.0f;
	const float sh_y = 1.0f;
	nvgBeginPath(vg);
	nvgMoveTo(vg, x - g_bookmark_width * 0.5f, g_bar_height + 10.0f);
	nvgLineTo(vg, x + sh_x, g_bar_height + g_bookmark_pointyness + sh_y);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f + sh_x, g_bar_height + 10.0f + sh_y);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f + sh_x, sh_y);
	nvgLineJoin(vg, NVG_ROUND);
	nvgStrokeColor(vg, nvgRGBAf(0.0f, 0.0f, 0.2f, 0.3f));
	nvgStrokeWidth(vg, 4.0f);
	nvgStroke(vg);

	// bg
	nvgBeginPath(vg);
	nvgMoveTo(vg, x - g_bookmark_width * 0.5f, 0.0f);
	nvgLineTo(vg, x - g_bookmark_width * 0.5f, g_bar_height + 10.0f);
	nvgLineTo(vg, x, g_bar_height + g_bookmark_pointyness);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f, g_bar_height + 10.0f);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f, 0.0f);
	NVGpaint rpaint = nvgRadialGradient(vg, x - 15.0f, g_bar_height - 35.0f, 8.0f, 70.0f,
			nvgRGBf(0.0f, 0.6f, 1.0f), nvgRGBf(0.0f, 0.45f, 1.0f));
	nvgFillPaint(vg, rpaint);
	nvgFill(vg);

	// highlight
	nvgBeginPath(vg);
	nvgMoveTo(vg, x - g_bookmark_width * 0.5f, g_bar_height + 10.0f - 4.0f);
	nvgLineTo(vg, x, g_bar_height + g_bookmark_pointyness - 4.0f);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f, g_bar_height + 10.0f - 4.0f);
	nvgStrokeColor(vg, nvgRGBf(0.00f, 0.33f, 0.82f));
	nvgStrokeWidth(vg, 5.5f);
	nvgStroke(vg);

	// outline
	nvgBeginPath(vg);
	nvgMoveTo(vg, x - g_bookmark_width * 0.5f, 0.0f);
	nvgLineTo(vg, x - g_bookmark_width * 0.5f, g_bar_height + 10.0f);
	nvgLineTo(vg, x, g_bar_height + g_bookmark_pointyness);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f, g_bar_height + 10.0f);
	nvgLineTo(vg, x + g_bookmark_width * 0.5f, 0.0f);
	nvgLineJoin(vg, NVG_ROUND);
	nvgStrokeColor(vg, nvgRGBf(0.00f, 0.00f, 0.02f));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);
}

void ugui_mainmenu_icon(engine_t *engine, float x, const char *label, int icon, int font, int active) {
	NVGcontext *vg = engine->vg;

	float label_y = g_bar_height * 0.5f + 4.0f;

	// icon
	if (active) {
		const NVGpaint ipaint = nvgImagePattern(vg, x - g_icon_size * 0.5f, g_bar_height * 0.5f - g_icon_size * 0.6f, g_icon_size, g_icon_size, 0.0f, icon, 1.0f);
		nvgBeginPath(vg);
		nvgRect(vg, x - g_icon_size * 0.5f, g_bar_height * 0.5f - g_icon_size * 0.6f, g_icon_size, g_icon_size);
		nvgFillPaint(vg, ipaint);
		nvgFill(vg);
		label_y = g_bar_height + 2.0f;
	}

	// text
	nvgFontFaceId(vg, font);
	nvgTextLetterSpacing(vg, 3.0f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
	nvgFontSize(vg, 23.0f);

	nvgFontBlur(vg, 5.0f);
	if (active) {
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.7f));
	} else {
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.0f));
	}
	nvgText(vg, x, label_y + 1.0f, label, NULL);

	nvgFontBlur(vg, 0.0f);
	nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 1.0f));
	nvgText(vg, x, label_y, label, NULL);
}

