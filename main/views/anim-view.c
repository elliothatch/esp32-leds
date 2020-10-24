#include "anim-view.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "frame-view.h"
#include "../render.h"

fp_viewid fp_create_anim_view(
	unsigned int width,
	unsigned int height,
	unsigned int frameCount,
	unsigned int frameratePeriodMs
) {
	fp_viewid* frames = malloc(frameCount * sizeof(fp_viewid));
	if(!frames) {
		printf("error: fp_create_anim_view: failed to allocate memory for frames\n");
		return 0;
	}

	fp_anim_view_data* animData = malloc(sizeof(fp_anim_view_data));
	if(!animData) {
		printf("error: fp_create_anim_view: failed to allocate memory for animData\n");
		free(frames);
		return 0;
	}

	animData->frameCount = frameCount;
	animData->frames = frames;
	animData->frameIndex = 0;
	animData->frameratePeriodMs = frameratePeriodMs;
	animData->isPlaying = false;
	animData->loop = false;

	fp_viewid id = fp_create_view(FP_VIEW_ANIM, 0, animData);

	/* init frames */
	for(int i = 0; i < frameCount; i++) {
		frames[i] = fp_create_frame_view(width, height, rgb(0,0,0));
		fp_get_view(frames[i])->parent = id;
	}

	return id;
}

fp_viewid fp_create_anim_view_composite(
	fp_viewid* frames,
	unsigned int frameCount,
	unsigned int frameratePeriodMs
) {
	/** copy the values from the array */
	fp_viewid* newFrames = malloc(frameCount * sizeof(fp_viewid));
	if(!newFrames) {
		printf("error: fp_create_anim_view: failed to allocate memory for frames\n");
		return 0;
	}

	fp_anim_view_data* animData = malloc(sizeof(fp_anim_view_data));
	if(!animData) {
		printf("error: fp_create_anim_view: failed to allocate memory for animData\n");
		free(newFrames);
		return 0;
	}

	animData->frameCount = frameCount;
	animData->frames = newFrames;
	animData->frameIndex = 0;
	animData->frameratePeriodMs = frameratePeriodMs;
	animData->isPlaying = false;
	animData->loop = false;

	fp_viewid id = fp_create_view(FP_VIEW_ANIM, 0, animData);

	/* copy frame viewids */
	for(int i = 0; i < frameCount; i++) {
		newFrames[i] = frames[i];
		fp_get_view(newFrames[i])->parent = id;
	}

	return id;
}

fp_frameid fp_anim_view_get_frame(fp_view* view) {
	fp_anim_view_data* animData = view->data;
	return fp_get_view_frame(animData->frames[animData->frameIndex]);
}

bool fp_anim_view_render(fp_view* view) {
	return true;
}

bool fp_anim_view_onnext_render(fp_view* view) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_anim_view_data* animData = view->data;
	if(animData->isPlaying) {
		animData->frameIndex = (animData->frameIndex + 1) % animData->frameCount;

		if(animData->frameIndex < animData->frameCount - 1 || animData->loop) {
			fp_queue_render(view->id, currentTick + pdMS_TO_TICKS(animData->frameratePeriodMs)); 
		}
		else {
			animData->isPlaying = false;
		}
	}

	return true;
}


bool fp_play_once_anim(fp_viewid animView) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_view* view = fp_get_view(animView);
	fp_anim_view_data* animData = view->data;

	animData->isPlaying = true;
	animData->loop = false;
	animData->frameIndex = 0;
	fp_mark_view_dirty(animView);
	return fp_queue_render(animView, currentTick + pdMS_TO_TICKS(animData->frameratePeriodMs)); 
}

/** queues up next frame of animation */
bool fp_play_anim(fp_viewid animView) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_view* view = fp_get_view(animView);
	fp_anim_view_data* animData = view->data;

	animData->isPlaying = true;
	animData->loop = true;
	return fp_queue_render(animView, currentTick + pdMS_TO_TICKS(animData->frameratePeriodMs)); 
}

bool fp_pause_anim(fp_viewid animView) {
	fp_view* view = fp_get_view(animView);
	((fp_anim_view_data*)view->data)->isPlaying = false;

	return true;
}

bool fp_anim_view_free(fp_view* view) {
	fp_anim_view_data* animData = view->data;
	if(!view->composite) {
		for(int i = 0; i < animData->frameCount; i++) {
			fp_free_view(animData->frames[i]);
		}
	}

	free(animData->frames);
	free(animData);

	return true;
}
