#include "transition-view.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../render.h"
#include "frame-view.h"
#include "anim-view.h"

fp_viewid fp_create_transition_view(
	unsigned int width,
	unsigned int height,
	unsigned int pageCount,
	fp_transition transition,
	unsigned int transitionPeriodMs
) {

	if(transition.viewA == 0 || transition.viewB == 0) {
		printf("error: fp_create_transition_view: must provide valid transition\n");
		return 0;
	}

	fp_viewid* pages = malloc(pageCount * sizeof(fp_viewid));
	if(!pages) {
		printf("error: fp_create_transition_view: failed to allocate memory for pages\n");
		return 0;
	}

	fp_transition_view_data* transitionData = malloc(sizeof(fp_transition_view_data));
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
	transitionData->transition = transition;
	transitionData->blendFn = &rgb_alpha;
	transitionData->transitionPeriodMs = transitionPeriodMs;

	fp_viewid id = fp_view_create(FP_VIEW_TRANSITION, false, transitionData);

	/* init pages */
	for(int i = 0; i < pageCount; i++) {
		pages[i] = fp_frame_view_create(width, height, rgb(0,0,0));
		fp_view_get(pages[i])->parent = id;
	}

	fp_view_get(transition.viewA)->parent = id;
	fp_view_get(transition.viewB)->parent = id;

	return id;
}

fp_viewid fp_create_transition_view_composite(
	unsigned int width,
	unsigned int height,
	fp_viewid* pages, 
	unsigned int pageCount,
	fp_transition transition,
	unsigned int transitionPeriodMs
) {

	if(transition.viewA == 0 || transition.viewB == 0) {
		printf("error: fp_create_transition_view_composite: must provide valid transition\n");
		return 0;
	}


	fp_viewid* newPages = malloc(pageCount * sizeof(fp_viewid));
	if(!newPages) {
		printf("error: fp_create_transition_view_composite: failed to allocate memory for pages\n");
		return 0;
	}

	fp_transition_view_data* transitionData = malloc(sizeof(fp_transition_view_data));
	if(!transitionData) {
		printf("error: fp_create_transition_view_composite: failed to allocate memory for transitionData\n");
		free(newPages);
		return 0;
	}

	transitionData->pageCount = pageCount;
	transitionData->pages = newPages;
	transitionData->pageIndex = 0;
	transitionData->previousPageIndex = 0;
	transitionData->frame = fp_create_frame(width, height, rgb(0,0,0));
	
	transitionData->transition = transition;
	transitionData->blendFn = &rgb_alpha;
	transitionData->transitionPeriodMs = transitionPeriodMs;

	fp_viewid id = fp_view_create(FP_VIEW_TRANSITION, true, transitionData);

	/* copy pages */
	for(int i = 0; i < pageCount; i++) {
		newPages[i] = pages[i];
		fp_view_get(newPages[i])->parent = id;
	}

	fp_view_get(transition.viewA)->parent = id;
	fp_view_get(transition.viewB)->parent = id;

	return id;
}

bool fp_transition_loop(fp_viewid transitionView, bool reverse) {
	fp_view* view = fp_view_get(transitionView);
	fp_transition_view_data* transitionData = view->data;

	TickType_t currentTick = xTaskGetTickCount();

	transitionData->loop = 1 - reverse*2;
	return fp_queue_render(transitionView, currentTick + pdMS_TO_TICKS(transitionData->transitionPeriodMs)); 
}

bool fp_transition_set(fp_viewid transitionView, unsigned int pageIndex) {
	fp_view* view = fp_view_get(transitionView);
	fp_transition_view_data* transitionData = view->data;

	transitionData->previousPageIndex = transitionData->pageIndex;
	transitionData->pageIndex = pageIndex % transitionData->pageCount;
	return fp_play_once_anim(transitionData->transition.viewA)
		&& fp_play_once_anim(transitionData->transition.viewB);
}

bool fp_transition_next(fp_viewid transitionView) {
	fp_view* view = fp_view_get(transitionView);
	fp_transition_view_data* transitionData = view->data;

	return fp_transition_set(transitionView, (transitionData->pageIndex + 1) % transitionData->pageCount);
}

bool fp_transition_prev(fp_viewid transitionView) {
	fp_view* view = fp_view_get(transitionView);
	fp_transition_view_data* transitionData = view->data;

	return fp_transition_set(
		transitionView,
		 (
			(transitionData->pageIndex - 1) % transitionData->pageCount
			 + transitionData->pageCount
		 )% transitionData->pageCount
	);
}

fp_frameid fp_transition_view_get_frame(fp_view* view) {
	return ((fp_transition_view_data*)view->data)->frame;
}

bool fp_transition_view_render(fp_view* view) {
	fp_transition_view_data* transitionData = view->data;
	// TODO: add anim_view start/stop/pause functions
	// add nextPage, previousPage, setPage, cycle functions that trigger transition animation playback
	fp_frame* frame = fp_frame_get(transitionData->frame);
	fp_frame* frameA = fp_frame_get(fp_view_get_frame(transitionData->pages[transitionData->previousPageIndex]));
	fp_frame* frameB = fp_frame_get(fp_view_get_frame(transitionData->pages[transitionData->pageIndex]));

	fp_frame* transitionA = fp_frame_get(fp_view_get_frame(transitionData->transition.viewA));
	fp_frame* transitionB = fp_frame_get(fp_view_get_frame(transitionData->transition.viewB));


	for(int row = 0; row < fp_frame_height(frame); row++) {
		for(int col = 0; col < frame->width; col++) {
			rgb_color colorA = rgb(0, 0, 0);
			rgb_color colorB = rgb(0, 0, 0);
			uint8_t alphaA = 0;
			uint8_t alphaB = 0;

			if(row < fp_frame_height(transitionA) && col < transitionA->width) {
				uint16_t indexA = transitionA->pixels[fp_fcalc_index(col, row, transitionA->width)].mapFields.index;
				if(indexA < frameA->length) {
					colorA = frameA->pixels[indexA];
				}
				alphaA = transitionA->pixels[fp_fcalc_index(col, row, transitionA->width)].mapFields.alpha;
			}

			if(row < fp_frame_height(transitionB) && col < transitionB->width) {
				uint16_t indexB = transitionB->pixels[fp_fcalc_index(col, row, transitionB->width)].mapFields.index;
				if(indexB < frameB->length) {
					colorB = frameB->pixels[indexB];
				}
				alphaB = transitionB->pixels[fp_fcalc_index(col, row, transitionB->width)].mapFields.alpha;
			}


			frame->pixels[fp_fcalc_index(col, row, frame->width)] =
				(*transitionData->blendFn)(colorA, alphaA, colorB, alphaB);
		}
	}

	return true;
}

bool fp_transition_view_onnext_render(fp_view* view) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_transition_view_data* transitionData = view->data;
	if(transitionData->loop != 0) {
		if(transitionData->loop > 0) {
			fp_transition_next(view->id);
		}
		else {
			fp_transition_prev(view->id);
		}

		// queue next transition
		fp_queue_render(view->id, currentTick + pdMS_TO_TICKS(transitionData->transitionPeriodMs));
	}

	return true;
}


fp_transition fp_create_sliding_transition(unsigned int width, unsigned int height, unsigned int frameratePeriodMs) {
	unsigned int frameCount = width + 1;

	fp_transition transition = {
		fp_create_anim_view(width, height, frameCount, frameratePeriodMs),
		fp_create_anim_view(width, height, frameCount, frameratePeriodMs)
	};

	fp_view* transitionViewA = fp_view_get(transition.viewA);
	fp_view* transitionViewB = fp_view_get(transition.viewB);

	for(int i = 0; i < frameCount; i++) {
		fp_frame* frameA = fp_frame_get(fp_view_get_frame(((fp_anim_view_data*)transitionViewA->data)->frames[i]));
		fp_frame* frameB = fp_frame_get(fp_view_get_frame(((fp_anim_view_data*)transitionViewB->data)->frames[i]));

		for(int row = 0; row < height; row++) {
			for(int col = 0; col < width; col++) {
				// TODO: calculate indexes for the transitions
				// it might be useful to write a helper function that generates these?
				unsigned int aIndex = fp_fcalc_index(col, row, width);
				unsigned int aAlpha = 0;
				if(col < (width - i)) {
					aAlpha = 255;
				}

				unsigned int bIndex = 0;
				unsigned int bAlpha = 0;
				if(col >= (width - i)) {
					bAlpha = 255;
				}

				frameA->pixels[fp_fcalc_index(col, row, width)].mapFields.index = aIndex;
				frameA->pixels[fp_fcalc_index(col, row, width)].mapFields.alpha = aAlpha;

				frameB->pixels[fp_fcalc_index(col, row, width)].mapFields.index = bIndex;
				frameB->pixels[fp_fcalc_index(col, row, width)].mapFields.alpha = bAlpha;
			}
		}
	}

	/*
	 0 1
	 2 3

	A:
	0 1  1 0  0 0
	2 3  3 0  0 0
	
	1 1  1 0  0 0
	1 1  1 0  0 0

	B:
	0 0  0 0  0 1
	0 0  0 2  2 3

	0 0  0 1  1 1
	0 0  0 1  1 1
	
	 */

	return transition;
}

bool fp_transition_view_free(fp_view* view) {
	fp_transition_view_data* transitionData = view->data;
	if(!view->composite) {
		for(int i = 0; i < transitionData->pageCount; i++) {
			fp_view_free(transitionData->pages[i]);
		}
	}

	fp_frame_free(transitionData->frame);
	free(transitionData->pages);
	free(transitionData);

	return true;
}
