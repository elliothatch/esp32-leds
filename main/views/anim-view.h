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
} fp_anim_view_data;

fp_viewid fp_anim_view_create(
	unsigned int width,
	unsigned int height,
	unsigned int frameCount,
	unsigned int frameratePeriodMs
);
fp_viewid fp_anim_view_create_composite(
	fp_viewid* frames,
	unsigned int frameCount,
	unsigned int frameratePeriodMs
);

/** plays the animation through once from the beginning, then stops */
bool fp_anim_play_once(fp_viewid animView);
/** resumes the animation at current frame and loop continuously */
bool fp_anim_play(fp_viewid animView);
bool fp_anim_pause(fp_viewid animView);


fp_frameid fp_anim_view_get_frame(fp_view* view);
bool fp_anim_view_render(fp_view* view);
bool fp_anim_view_onnext_render(fp_view* view);
bool fp_anim_view_free(fp_view* view);

static const fp_view_register_data fp_anim_view_register_data = {
	&fp_anim_view_get_frame,
	&fp_anim_view_render,
	&fp_anim_view_onnext_render,
	&fp_anim_view_free
};

#endif /* ANIM_VIEW_H */
