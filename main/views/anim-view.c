#include "anim-view.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

fp_viewid fp_create_anim_view(fp_viewid* views, unsigned int frameCount, unsigned int frameratePeriodMs, unsigned int width, unsigned int height) {
	fp_viewid* frames = malloc(frameCount * sizeof(fp_viewid));
	if(!frames) {
		printf("error: fp_create_anim_view: failed to allocate memory for frames\n");
		return 0;
	}

	fp_view_anim_data* animData = malloc(sizeof(fp_view_anim_data));
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
		if(views == NULL || views[i] == 0) {
			frames[i] = fp_create_frame_view(width, height, rgb(0,0,0));
		}
		else {
			frames[i] = views[i];
		}
		(&viewPool[frames[i]])->parent = id;
	}

	return id;
}

bool fp_play_once_anim(fp_viewid animView) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_view* view = fp_get_view(animView);

	view->data.ANIM->isPlaying = true;
	view->data.ANIM->loop = false;
	view->data.ANIM->frameIndex = 0;
	fp_mark_view_dirty(animView);
	return fp_queue_render(animView, currentTick + pdMS_TO_TICKS(view->data.ANIM->frameratePeriodMs)); 
}

/** queues up next frame of animation */
bool fp_play_anim(fp_viewid animView) {
	TickType_t currentTick = xTaskGetTickCount();
	fp_view* view = fp_get_view(animView);

	view->data.ANIM->isPlaying = true;
	view->data.ANIM->loop = true;
	return fp_queue_render(animView, currentTick + pdMS_TO_TICKS(view->data.ANIM->frameratePeriodMs)); 
}

bool fp_pause_anim(fp_viewid animView) {
	fp_view* view = fp_get_view(animView);
	view->data.ANIM->isPlaying = false;

	return true;
}
