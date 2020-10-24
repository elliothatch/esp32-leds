#ifndef TRANSITION_VIEW_H
#define TRANSITION_VIEW_H

#include "../view.h"

/* fp: fresh pixel */

	/* contains two anim_views where each frame contains data (mapFields) about how one view is mapped onto the transition
	 */
typedef struct {
	fp_viewid viewA;
	fp_viewid viewB;
} fp_transition;

typedef struct {
	unsigned int pageCount;
	fp_viewid* pages;
	unsigned int pageIndex; /* marks page we are currently transitioned/transitioning to */
	unsigned int previousPageIndex; /* marks page we are transitioning from */
	fp_transition transition;
	rgb_color (*blendFn)(rgb_color a, uint8_t aWeight, rgb_color b, uint8_t bWeight);
	unsigned int transitionPeriodMs;
	int loop; /* 1 = loop, 0 = stop, -1 = loop reverse */
	/** stores the result of render */
	fp_frameid frame;

} fp_view_transition_data;

fp_viewid fp_create_transition_view(fp_viewid* pageIds, unsigned int pageCount, fp_transition transition, unsigned int transitionPeriodMs, unsigned int width, unsigned int height);

fp_frameid fp_transition_view_get_frame(fp_view* view);
bool fp_transition_view_render(fp_view* view);
bool fp_transition_view_onnext_render(fp_view* view);

bool fp_transition_loop(fp_viewid transitionView, bool reverse);
bool fp_transition_set(fp_viewid transitionView, unsigned int pageIndex);
bool fp_transition_next(fp_viewid transitionView);
bool fp_transition_prev(fp_viewid transitionView);

/* simple sliding transition. starts on viewA and slides left one pixel each frame to viewB */
fp_transition fp_create_sliding_transition(unsigned int width, unsigned int height, unsigned int frameratePeriodMs);

static const fp_view_register_data fp_transition_view_register_data = {
	&fp_transition_view_get_frame,
	&fp_transition_view_render,
	&fp_transition_view_onnext_render
};

#endif /* TRANSITION_VIEW_H */
