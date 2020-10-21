#include "transition-view.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "anim-view.h"

fp_viewid fp_create_transition_view(
	fp_viewid* pageIds,
	unsigned int pageCount,
	fp_transition transition,
	unsigned int transitionPeriodMs,
	unsigned int width,
	unsigned int height
) {

	fp_viewid* pages = malloc(pageCount * sizeof(fp_viewid));
	if(!pages) {
		printf("error: fp_create_transition_view: failed to allocate memory for pages\n");
		return 0;
	}

	fp_view_transition_data* transitionData = malloc(sizeof(fp_view_transition_data));
	if(!transitionData) {
		printf("error: fp_create_transition_view: failed to allocate memory for transitionData\n");
		free(pages);
		return 0;
	}

	transitionData->pageCount = pageCount;
	transitionData->pages = pages;
	transitionData->pageIndex = 0;
	transitionData->previousPageIndex = 0;
	transitionData->frame = fp_create_frame(width, height, rgb(0,0,0));
	

	if(transition.viewA == 0 && transition.viewB == 0) {
		// TODO: handle/error when missing one view from transition
		transition = fp_create_sliding_transition( width, height, 1000/2);
	}

	transitionData->transition = transition;
	transitionData->blendFn = &rgb_alpha;
	transitionData->transitionPeriodMs = transitionPeriodMs;

	fp_viewid id = fp_create_view(FP_VIEW_TRANSITION, 0, transitionData);

	/* init pages */
	for(int i = 0; i < pageCount; i++) {

		fp_viewid pageView = 0;

		if(pageIds == NULL || pageIds[i] == 0) {
			pageView = fp_create_frame_view(width, height, rgb(0,0,0));
		}
		else {
			pageView = pageIds[i];
		}

		pages[i] = pageView;
		(&viewPool[pageView])->parent = id;
	}

	fp_get_view(transition.viewA)->parent = id;
	fp_get_view(transition.viewB)->parent = id;

	return id;
}

bool fp_transition_loop(fp_viewid transitionView, bool reverse) {
	fp_view* view = fp_get_view(transitionView);
	TickType_t currentTick = xTaskGetTickCount();

	view->data.TRANSITION->loop = 1 - reverse*2;
	return fp_queue_render(transitionView, currentTick + pdMS_TO_TICKS(view->data.TRANSITION->transitionPeriodMs)); 
}

bool fp_transition_set(fp_viewid transitionView, unsigned int pageIndex) {
	fp_view* view = fp_get_view(transitionView);

	view->data.TRANSITION->previousPageIndex = view->data.TRANSITION->pageIndex;
	view->data.TRANSITION->pageIndex = pageIndex % view->data.TRANSITION->pageCount;
	return fp_play_once_anim(view->data.TRANSITION->transition.viewA)
		&& fp_play_once_anim(view->data.TRANSITION->transition.viewB);
}

bool fp_transition_next(fp_viewid transitionView) {
	fp_view* view = fp_get_view(transitionView);
	return fp_transition_set(transitionView, (view->data.TRANSITION->pageIndex + 1) % view->data.TRANSITION->pageCount);
}

bool fp_transition_prev(fp_viewid transitionView) {
	fp_view* view = fp_get_view(transitionView);
	return fp_transition_set(
		transitionView,
		 (
			(view->data.TRANSITION->pageIndex - 1) % view->data.TRANSITION->pageCount
			 + view->data.TRANSITION->pageCount
		 )% view->data.TRANSITION->pageCount
	);
}
