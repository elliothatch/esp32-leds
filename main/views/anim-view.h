#ifndef ANIM_VIEW_H
#define ANIM_VIEW_H

#include "../view.h"

/* fp: fresh pixel */

typedef struct {
	unsigned int frameCount;
	fp_viewid* frames;
	unsigned int frameIndex;
	unsigned int frameratePeriodMs;
	bool isPlaying;
	bool loop;
} fp_view_anim_data;

fp_viewid fp_create_anim_view(fp_viewid* views, unsigned int frameCount, unsigned int frameratePeriodMs, unsigned int width, unsigned int height);

fp_frameid fp_anim_view_get_frame(fp_view* view);
bool fp_anim_view_render(fp_view* view);
bool fp_anim_view_onnext_render(fp_view* view);

/** plays the animation through once from the beginning, then stops */
bool fp_play_once_anim(fp_viewid animView);
/** resumes the animation at current frame and loop continuously */
bool fp_play_anim(fp_viewid animView);
bool fp_pause_anim(fp_viewid animView);

static const fp_view_register_data fp_anim_view_register_data = {
	&fp_anim_view_get_frame,
	&fp_anim_view_render,
	&fp_anim_view_onnext_render
};

#endif /* ANIM_VIEW_H */
