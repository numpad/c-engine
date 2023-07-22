#include "ugui.h"

void ugui_ctx_init(struct ugui_ctx_s *ctx) {
	ctx->on_event = NULL;
	ctx->userdata = NULL;
}

