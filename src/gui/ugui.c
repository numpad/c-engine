#include "ugui.h"

#include <assert.h>
#include <nanovg.h>
#include "engine.h"

//
// vars
//

const float g_bar_height = 60.0f;
const float g_icon_size = 42.0f;
const float g_bookmark_width = 110.0f;
const float g_bookmark_pointyness = 30.0f;
const float g_border_radius = 10.0f;

//
// private funcs
//
static void mix_color(NVGcolor *out, NVGcolor a, NVGcolor b, float v);

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

void ugui_mainmenu_icon(engine_t *engine, float x, const char *label, int icon, int font, float active) {
	NVGcontext *vg = engine->vg;

	float label_y = g_bar_height * 0.5f + 4.0f;

	// icon
	if (active > 0.0f) {
		const float icon_size = g_icon_size * glm_ease_exp_inout(active);
		const NVGpaint ipaint = nvgImagePattern(vg, x - icon_size * 0.5f, g_bar_height * 0.5f - icon_size * 0.6f, icon_size, icon_size, 0.0f, icon, 1.0f);
		nvgBeginPath(vg);
		nvgRect(vg, x - icon_size * 0.5f, g_bar_height * 0.5f - icon_size * 0.6f, icon_size, icon_size);
		nvgFillPaint(vg, ipaint);
		nvgFill(vg);
		label_y = glm_lerp(label_y, g_bar_height + 2.0f, glm_ease_exp_inout(active));
	}

	// text
	nvgFontFaceId(vg, font);
	nvgTextLetterSpacing(vg, 2.0f);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFontSize(vg, 23.0f);

	nvgFontBlur(vg, 5.0f);
	if (active > 0.0f) {
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.7f));
	} else {
		nvgFillColor(vg, nvgRGBf(0.0f, 0.0f, 0.0f));
	}
	nvgText(vg, x, label_y + 1.0f, label, NULL);

	nvgFontBlur(vg, 0.0f);
	nvgFillColor(vg, nvgRGBf(0.9f, 0.9f, 1.0f));
	nvgText(vg, x, label_y, label, NULL);
}


//////////////////////
// Modals & Windows //
//////////////////////

rendered_modal_t ugui_modal_begin(engine_t *engine) {
	NVGcontext *vg = engine->vg;

	const float padding_x = 60.0f;
	const float modal_x = padding_x;
	const float modal_y = 200.0f;
	const float modal_w = engine->window_width - padding_x * 2.0f;
	const float modal_h = engine->window_height - 200.0f - 150.0f;

	// Shade background darker
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 60));
	nvgRect(vg, 0.0f, 0.0f, engine->window_width, engine->window_height);
	nvgFill(vg);

	// Shadow
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBA(0, 0, 0, 80));
	nvgRoundedRect(vg, padding_x + 6.0f, 200.0f + 7.0f,
			engine->window_width - padding_x * 2.0f, engine->window_height - 200.0f - 150.0f,
			g_border_radius + 5.0f);
	nvgFill(vg);

	// Background
	nvgBeginPath(vg);
	nvgFillColor(vg, nvgRGBf(0.95f, 0.97f, 0.78f));
	nvgRoundedRect(vg, padding_x, 200.0f,
			engine->window_width - padding_x * 2.0f, engine->window_height - 200.0f - 150.0f,
			g_border_radius);
	nvgFill(vg);

	// Prevent Overdraw
	nvgScissor(vg, modal_x, modal_y, modal_w, modal_h);

	// Restore state
	nvgSave(vg);
	nvgTranslate(vg, modal_x, modal_y);

	return (rendered_modal_t) {
		.vg = vg,
		.width = modal_w,
		.height = modal_h
	};
}

void ugui_modal_end(rendered_modal_t modal) {
	NVGcontext *vg = modal.vg;
	nvgResetScissor(vg);

	// Outline
	nvgBeginPath(vg);
	nvgRoundedRect(vg, 0.0f, 0.0f, modal.width, modal.height, g_border_radius);
	nvgStrokeColor(vg, nvgRGBf(0.33f, 0.2f, 0.25f));
	nvgStrokeWidth(vg, 3.5f);
	nvgStroke(vg);

	nvgRestore(vg);
}


///////////////////
// UI Components //
///////////////////

void ugui_mainmenu_button(engine_t *engine, float x, float y, float w, float h, const char *text1, const char *text2, const char *subtext, int font, NVGcolor color_bg, NVGcolor color_bg_darker, NVGcolor color_text_outline, float is_pressed) {
	assert(text1 != NULL);
	NVGcontext *vg = engine->vg;
	const float height_3d = 10.0f;
	const float hp = glm_ease_elast_out(is_pressed) * 0.5f;

	const float radius = g_border_radius - 6.0f * hp;
	const float active_outline_width = 13.5f * hp;

	x = x - 7.0f * hp * 2.0f;
	w = w + 14.0f * hp * 2.0f;

	// active outline
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x - active_outline_width * 0.5f, y - active_outline_width * 0.5f + height_3d * hp, w + active_outline_width, h + active_outline_width - height_3d * hp, radius + 1.5f);
	nvgStrokeColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, hp * 1.5f));
	nvgStrokeWidth(vg, active_outline_width);
	nvgStroke(vg);

	// shadow
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x + 5.5f - 2.5f * hp, y + 5.5f - 2.5f * hp, w - 2.0f * hp, h - 2.0f * hp, radius + 1.5f);
	nvgFillColor(vg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.4f));
	nvgFill(vg);

	// bg 3d
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y + h - 20.0f - height_3d, w, 30.0f, radius);
	nvgFillColor(vg, color_bg_darker);
	nvgFill(vg);

	// bg
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y + height_3d * hp, w, h - height_3d, radius);
	const NVGpaint p = nvgRadialGradient(vg, x + w * 0.15f, y + 25.0f + height_3d * hp, 30.0f, 30.0f + h * 0.8f, color_bg_darker, color_bg);
	nvgFillPaint(vg, p);
	nvgFill(vg);

	// outline inner light
	const float inset = 3.0f;
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x + inset, y + inset + height_3d * hp, w - inset * 2.0f, h - inset * 1.5f - height_3d, radius);
	nvgStrokeColor(vg, color_bg);
	nvgStrokeWidth(vg, 5.0f);
	nvgStroke(vg);

	// outline dark
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y + height_3d * hp, w, h - height_3d * hp, radius);
	nvgStrokeColor(vg, color_text_outline);
	nvgStrokeWidth(vg, 2.5f);
	nvgStroke(vg);

	nvgSave(vg);
	nvgTranslate(vg, 0.0f, height_3d * hp);
	// main text
	nvgFontFaceId(vg, font);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	nvgFontSize(vg, 42.0f);
	nvgFontBlur(vg, 0.0f);

	// outline
	const float outline_width = 3.0f;
	nvgFillColor(vg, color_text_outline);
	if (text1) nvgText(vg, x + w * 0.5f - outline_width, y + 80.0f,                         text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f - outline_width, y + 80.0f + 30.0f,                 text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f + outline_width, y + 80.0f,                         text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f + outline_width, y + 80.0f + 30.0f,                 text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f,                 y + 80.0f         - outline_width, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f,                 y + 80.0f + 30.0f - outline_width, text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f,                 y + 80.0f         + outline_width, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f,                 y + 80.0f + 30.0f + outline_width, text2,  NULL);
	// diag
	if (text1) nvgText(vg, x + w * 0.5f - outline_width * 0.75f, y + 80.0f         - outline_width * 0.75f, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f - outline_width * 0.75f, y + 80.0f + 30.0f - outline_width * 0.75f, text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f + outline_width * 0.75f, y + 80.0f         + outline_width * 0.75f, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f + outline_width * 0.75f, y + 80.0f + 30.0f + outline_width * 0.75f, text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f + outline_width * 0.75f, y + 80.0f         - outline_width * 0.75f, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f + outline_width * 0.75f, y + 80.0f + 30.0f - outline_width * 0.75f, text2,  NULL);
	if (text1) nvgText(vg, x + w * 0.5f - outline_width * 0.75f, y + 80.0f         + outline_width * 0.75f, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f - outline_width * 0.75f, y + 80.0f + 30.0f + outline_width * 0.75f, text2,  NULL);

	// foreground
	nvgFontBlur(vg, 0.0f);
	nvgFillColor(vg, nvgRGBf(0.94f, 0.94f, 0.94f));
	nvgText(vg, x + w * 0.5f, y + 80.0f - 1.0f, text1, NULL);
	if (text2) nvgText(vg, x + w * 0.5f, y + 80.0f + 29.0f, text2, NULL);

	// info
	nvgFontBlur(vg, 0.0f);
	nvgFontSize(vg, 14.0f);
	nvgFillColor(vg, color_text_outline);
	if (subtext) nvgText(vg, x + w * 0.5f, y + 80.0f + 58.0f, subtext, NULL);

	nvgRestore(vg);
}

void ugui_mainmenu_checkbox(engine_t *engine, float x, float y, float w, float h, int font, const char *text, float is_checked) {
	assert(text != NULL);
	NVGcontext *vg = engine->vg;
	const float hp = glm_ease_exp_inout(is_checked);

	const NVGcolor color_red = nvgRGBf(0.79f, 0.3f, 0.16f);
	const NVGcolor color_green = nvgRGBf(0.39f, 0.82f, 0.2f);
	NVGcolor color_knob;
	mix_color(&color_knob, color_red, color_green, glm_ease_circ_inout(hp));

	const NVGcolor color_bg = nvgRGBf(0.95f, 0.95f, 0.95f);
	const float radius = h * 0.5f;
	const float knob_radius = radius - 5.0f;

	nvgSave(vg);

	// background
	nvgBeginPath(vg);
	nvgArc(vg, x + radius, y + radius, radius, GLM_PIf - GLM_PI_2f, 2.0f * GLM_PI - GLM_PI_2f, NVG_CW);
	nvgFillColor(vg, color_bg);
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgRect(vg, x + radius - 1.0f, y, w - radius * 2.0f + 2.0f, h);
	nvgFillColor(vg, color_bg);
	nvgFill(vg);

	nvgBeginPath(vg);
	nvgArc(vg, x + w - radius, y + radius, radius, GLM_PIf + GLM_PI_2f, 2.0f * GLM_PI + GLM_PI_2f, NVG_CW);
	nvgFillColor(vg, color_bg);
	nvgFill(vg);
	
	// outline
	nvgBeginPath(vg);
	nvgArc(vg, x + radius, y + radius, radius, GLM_PIf - GLM_PI_2f, 2.0f * GLM_PI - GLM_PI_2f, NVG_CW);
	nvgArc(vg, x + w - radius, y + radius, radius, GLM_PIf + GLM_PI_2f, 2.0f * GLM_PI + GLM_PI_2f, NVG_CW);
	nvgLineTo(vg, x + radius, y + h);
	nvgStrokeColor(vg, nvgRGBf(0.2f, 0.2f, 0.2f));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// knob
	nvgBeginPath(vg);
	nvgEllipse(vg, x + radius + (w - radius * 2.0f) * hp, y + radius, knob_radius, knob_radius * (2.0f * hp - 1.0f));
	nvgFillColor(vg, color_knob);
	nvgFill(vg);
	nvgStrokeColor(vg, nvgRGBf(0.2f, 0.2f, 0.2f));
	nvgStrokeWidth(vg, 3.0f);
	nvgStroke(vg);

	// text
	nvgFontFaceId(vg, font);
	nvgFontSize(vg, 34.0f);
	nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
	nvgFillColor(vg, nvgRGBf(0.1f, 0.1f, 0.1f));
	nvgText(vg, x + w + 20.0f, y + h * 0.5f + 3.0f, text, NULL);

	nvgRestore(vg);
}


//
// private impls
//

static void mix_color(NVGcolor *out, NVGcolor a, NVGcolor b, float v) {
	out->r = glm_lerp(a.r, b.r, v);
	out->g = glm_lerp(a.g, b.g, v);
	out->b = glm_lerp(a.b, b.b, v);
	out->a = 1.0f;
}

