/**
 * pixel.c
 * Manages a pool of "frames" and "views" to support complex displays and animations
 *  - frame: Rectangle of RGB pixels
 *  - view: Contains one or more frames and data to display those frames. the parent is rendered immediately after rendering the child
 *
 *
 *  view types:
 *		frame_view: wraps a single frame (used internally, and useful in composite views)
 *		screen_view: represents a physical LED "screen" (strip, matrix). Render causes an LED update
 *
 *		COMPOSITE VIEWS - composite views combine multiple frames in some way. They always have two types of constructors:
 *			- fp_create_..._view(...)
 *			- fp_create_..._view_composite(..., ids)
 *		the composite version's parameter list always ends with "ids", an array? of fp_viewids to be used as each "frame" in the view
 *
 *		anim_view: stores frames of an animation and playback data
 *		layer_view*: stores layers that can be blended together. each layer has an xy offset, so they can be tiled or cropped across a display
 *		transition_view*: stores pages that can be transitions between with a smooth animation
 *
 *  Example layouts:
 *
 *
 * animated picture frame:
 *
 *                      / anim
 *  screen - transition - anim
 *                      \ anim
 *
 *  weather station display:
 *                                           / frame
 *                              / transition - frame
 *  screen - transition / layer - anim
 *                      |       \ frame
 *                      \ frame
 *   transitions between two displays, one uses tiled layers to show multiple pieces of data at once, including a nested transition that frequently updates
 *  (frames updated by custom code /w manually triggered render)
 *
 *
 * "looking glass" masking of background animation:
 *
 *  screen - layer (AND BLENDING) / anim (moving white rectangle where the image is see-through)
 *                                \ anim (bg)
 *
 *  moving 
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

#include "pixel.h"
#include "ws2812_control.h"

#define FP_FRAME_COUNT 512
#define FP_VIEW_COUNT 256
#define FP_PENDING_VIEW_RENDER_COUNT 64

#define DEBUG true

typedef struct {
	fp_viewid view;
	TickType_t tick; /* the view will be as soon as possible after this tick */
} fp_pending_view_render;


unsigned int calc_index(unsigned int x, unsigned int y, unsigned int width) {
	return y * width + x % width;
}

struct led_state ledState;
unsigned int framePoolCount = 1;
fp_frame framePool[FP_FRAME_COUNT] = {{ 0, 0, NULL}};
unsigned int viewCount = 1;
fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, false, {.FRAME = 0}}};

/* circular buffer */
unsigned int pendingViewRenderCount = 0;
unsigned int pendingViewRenderIndex = 0;
fp_pending_view_render pendingViewRenderPool[FP_PENDING_VIEW_RENDER_COUNT];

/** locks fp_create_frame. allows multiple tasks to safely create frames */
SemaphoreHandle_t createFrameLock = NULL;

bool fp_queue_render(fp_viewid view, TickType_t tick) {
	if(pendingViewRenderCount >= FP_PENDING_VIEW_RENDER_COUNT) {
		printf("error: fp_queue_render: pending render pool full. limit: %d\n", FP_PENDING_VIEW_RENDER_COUNT);
		return false;
	}

	unsigned int nextRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
	pendingViewRenderCount++;
	pendingViewRenderPool[nextRender].view = view;
	pendingViewRenderPool[nextRender].tick = tick;

	return true;
}

fp_pending_view_render fp_dequeue_render() {

	fp_pending_view_render render = pendingViewRenderPool[pendingViewRenderIndex];
	if(pendingViewRenderCount == 0) {
		render.view = 0;
		render.tick = 0;
		return render;
	}

	pendingViewRenderIndex = (pendingViewRenderIndex + 1) % FP_PENDING_VIEW_RENDER_COUNT;
	pendingViewRenderCount--;
	
	return render;
}

void fp_task_render(void *pvParameters) {
	const fp_task_render_params* params = (const fp_task_render_params*) pvParameters;
	ws2812_control_init();

	/* uint8_t brightness = 1; */
	/* fp_frameid frame1 = fp_create_frame(8, 8, rgb(brightness, 0, 0)); */
	/* fp_ffill_rect(frame1, 0, 0, 6, 6, rgb(0, brightness, 0)); */
	/* fp_ffill_rect(frame1, 0, 0, 4, 4, rgb(0, 0, brightness)); */


	TickType_t lastWakeTime = xTaskGetTickCount();
	while(true) {

		/* process commands */
		fp_queue_command command;
		while(xQueueReceive(params->commands, &command, 0) == pdPASS) {
			switch(command.cmd) {
				case FILL_RECT:
					fp_ffill_rect(
						command.fargs.FILL_RECT.id,
						command.fargs.FILL_RECT.x,
						command.fargs.FILL_RECT.y,
						command.fargs.FILL_RECT.width,
						command.fargs.FILL_RECT.height,
						command.fargs.FILL_RECT.color
					);
					break;
				case SET_RECT:
					fp_fset_rect(
						command.fargs.SET_RECT.id,
						command.fargs.SET_RECT.x,
						command.fargs.SET_RECT.y,
						command.fargs.SET_RECT.frame
					);
					break;
				case RENDER:
					xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
					fp_render(command.fargs.RENDER.id);
					xSemaphoreGive(params->shutdownLock);
					break;
				case RENDER_VIEW:
					xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
					fp_render_view(command.fargs.RENDER_VIEW.id);
					xSemaphoreGive(params->shutdownLock);
					break;
			}
		}

		TickType_t currentTick = xTaskGetTickCount();
		/* composite the image */
		/* TODO */
		/* process each pending render by dequeuing. requeue if the render is still pending */
		/* TODO: just us a vector for this? */
		int originalPendingViewRenderCount = pendingViewRenderCount;
		for(int i = 0; i < originalPendingViewRenderCount; i++) {
			fp_pending_view_render pendingRender = fp_dequeue_render();
			if(pendingRender.tick <= currentTick) {
				fp_view* view = fp_get_view(pendingRender.view);

				if(view->type == FP_VIEW_ANIM) {
					// TODO: move this elsewhere
					if(view->data.ANIM->isPlaying) {
						view->data.ANIM->frameIndex = (view->data.ANIM->frameIndex + 1) % view->data.ANIM->frameCount;

						if(view->data.ANIM->frameIndex < view->data.ANIM->frameCount - 1 || view->data.ANIM->loop) {
							fp_queue_render(pendingRender.view, currentTick + pdMS_TO_TICKS(view->data.ANIM->frameratePeriodMs)); 
						}
						else {
							view->data.ANIM->isPlaying = false;
						}
					}
				}
				if(view->type == FP_VIEW_TRANSITION) {
					if(view->data.TRANSITION->loop != 0) {
						if(view->data.TRANSITION->loop > 0) {
							fp_transition_next(pendingRender.view);
						}
						else {
							fp_transition_prev(pendingRender.view);
						}

						// queue next transition
						fp_queue_render(pendingRender.view, currentTick + pdMS_TO_TICKS(view->data.TRANSITION->transitionPeriodMs));
					}
				}

				fp_mark_view_dirty(pendingRender.view); 
			}
			else {
				// requeue
				unsigned int nextRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
				pendingViewRenderCount++;
				pendingViewRenderPool[nextRender].view = pendingRender.view;
				pendingViewRenderPool[nextRender].tick = pendingRender.tick;
			}
		}

		fp_view* rootView = fp_get_view(params->rootView);
		if(rootView->dirty) {
			xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
			fp_render_view(params->rootView);
			xSemaphoreGive(params->shutdownLock);
		}
		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(params->refresh_period_ms));
	}
}

fp_frameid fp_create_frame(unsigned int width, unsigned int height, rgb_color color) {
	if(framePoolCount >= FP_FRAME_COUNT) {
		printf("error: fp_create_frame: frame pool full. limit: %d\n", FP_FRAME_COUNT);
		return 0;
	}

	unsigned int length = width * height;

	rgb_color* pixels = malloc(length * sizeof(rgb_color));
	if(!pixels) {
		printf("error: fp_create_frame: failed to allocate memory for pixels\n");
		return 0;
	}

	for(unsigned int i = 0; i < length; i++) {
		pixels[i] = color;
	}


	if(!createFrameLock) {
		createFrameLock = xSemaphoreCreateBinary();
		if(!createFrameLock) {
			printf("error: fp_create_frame: failed to create semaphore\n");
		}
		xSemaphoreGive(createFrameLock);
	}

	xSemaphoreTake(createFrameLock, portMAX_DELAY);
	fp_frameid id = framePoolCount++;
	xSemaphoreGive(createFrameLock);

	framePool[id].length = length;
	framePool[id].width = width;
	framePool[id].pixels = pixels;

	/* printf("create frame: %d %d %d\n", */ 
	/* 	id, */
	/* 	framePool[id].length, */
	/* 	framePool[id].width */
	/* ); */

	if(DEBUG) {
		printf("create frame %d: length: %d\n", id, length);
	}

	return id;
}

fp_frame* fp_get_frame(fp_frameid id) {
	if(id >= framePoolCount) {
		printf("error: fp_get_frame: id %d too large, max id: %d\n", id, framePoolCount-1);
		return &framePool[0];
	}

	return &framePool[id];
}

bool fp_fset_rect(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		fp_frame* frame
		) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		memcpy(
			&targetFrame->pixels[calc_index(x, y + row, targetFrame->width)],
			&frame->pixels[calc_index(0, row, frame->width)],
			fmin(frame->width, fmax(0, targetFrame->width - x)) * sizeof(((fp_frame*)0)->pixels)
		);
	}

	return true;
}

bool fp_ffill_rect(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		unsigned int width,
		unsigned int height,
		rgb_color color
		) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* frame = &framePool[id];
	for(int row = 0; row < fmin(height, fmax(0,frame->length / frame->width - y)); row++) {
		for(int col = 0; col < fmin(width, fmax(0, frame->width - x)); col++) {
			frame->pixels[calc_index(x + col, y + row, frame->width)] = color;
		}
	}

	return true;
}

/** copies a frame onto another, ignoring 0,0,0 colors */
bool fp_fset_rect_transparent(
		fp_frameid id,
		unsigned int x,
		unsigned int y,
		fp_frame* frame
		) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, fmax(0, targetFrame->length / targetFrame->width - y)); row++) {
		for(int col = 0; col < fmin(frame->width, fmax(0, targetFrame->width - x)); col++) {
			rgb_color pixel = frame->pixels[calc_index(col, row, frame->width)];
			if(pixel.fields.b == 0
				&& pixel.fields.r == 0
				&& pixel.fields.g == 0) {
				continue;
			}
			targetFrame->pixels[calc_index(x + col, y + row, targetFrame->width)] = pixel;
		}
	}

	return true;
}

bool fp_fadd_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
) {
	return fp_fblend_rect(&rgb_addb, id, 255, x, y, frame, 255);
}

bool fp_fmultiply_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	/* pointer to the frame to copy from */
	fp_frame* frame
) {
	return fp_fblend_rect(&rgb_multiplyb, id, 255, x, y, frame, 255);
}

bool fp_fblend_rect(
	blend_fn blendFn,
	fp_frameid id,
	uint8_t alphaTarget,
	unsigned int x,
	unsigned int y,
	fp_frame* frame,
	uint8_t alphaSrc
) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
		for(int col = 0; col < fmin(frame->width, targetFrame->width - x); col++) {
			rgb_color colorResult = (*blendFn)(
					frame->pixels[calc_index(col, row, frame->width)],
					alphaSrc,
					targetFrame->pixels[calc_index(x + col, y + row, targetFrame->width)],
					alphaTarget
			);
			targetFrame->pixels[calc_index(x + col, y + row, targetFrame->width)] = colorResult;
		}
	}

	return true;
}

bool fp_render(fp_frameid id) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* frame = &framePool[id];
	/* printf("render %d, %d, %d\n", id, frame->width, frame->length); */
	/* for(int row = 0; row < frame->length / frame->width; row++) { */
	/* 	for(int col = 0; col < frame->width; col++) { */
	/* 		unsigned int idx = calc_index(row, col, frame->width); */
	/* 		printf("%03d %03d %03d | ", frame->pixels[idx].fields.r, frame->pixels[idx].fields.g, frame->pixels[idx].fields.b); */
	/* 	} */
	/* 	printf("\n"); */
	/* } */
	memcpy(ledState.leds, frame->pixels, fmin(frame->length, NUM_LEDS) * sizeof(((fp_frame*)0)->pixels));
	ws2812_write_leds(ledState);

	return true;
}

fp_view* fp_get_view(fp_viewid id) {
	if(id >= viewCount) {
		printf("error: fp_get_view: id %d too large, max id: %d\n", id, viewCount-1);
		return &viewPool[0];
	}

	return &viewPool[id];
}

fp_viewid fp_create_view(fp_view_type type, fp_viewid parent, fp_view_data data) {
	if(viewCount >= FP_VIEW_COUNT) {
		printf("error: fp_create_view: view pool full. limit: %d\n", FP_VIEW_COUNT);
		return 0;
	}

	fp_viewid id = viewCount++;

	viewPool[id].type = type;
	viewPool[id].parent = parent;
	viewPool[id].dirty = true;
	viewPool[id].data = data;

	if(DEBUG) {
		printf("create view %d: type: %d\n", id, type);
	}

	return id;
}

fp_viewid fp_create_frame_view(unsigned int width, unsigned int height, rgb_color color) {
	fp_view_frame_data* frameData = malloc(sizeof(fp_view_frame_data));
	if(!frameData) {
		printf("error: fp_create_frame_view: failed to allocate memory for frameData\n");
		return 0;
	}

	frameData->frame = fp_create_frame(width, height, color);
	fp_view_data data = { .FRAME = frameData };

	return fp_create_view(FP_VIEW_FRAME, 0, data);
}

fp_viewid fp_create_screen_view(unsigned int width, unsigned int height) {
	fp_view_screen_data * screenData = malloc(sizeof(fp_view_screen_data));
	if(!screenData) {
		printf("error: fp_create_screen_view: failed to allocate memory for screenData\n");
		return 0;
	}

	screenData->frame = fp_create_frame(width, height, rgb(0,0,0));
	screenData->childView = 0;

	fp_view_data data = { .SCREEN = screenData };

	return fp_create_view(FP_VIEW_SCREEN, 0, data);
}

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

	fp_view_data data = { .ANIM = animData };
	fp_viewid id = fp_create_view(FP_VIEW_ANIM, 0, data);

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

/**
 * @param views - if NULL, a frame_view will be created for each layer.
 *					otherwise, an array of length layerCount with views to to be used for each layer. if an element is 0 a frame_view will be created.
 * @param layerWidth - width of created frames
 * @param layerHeight - height of created frames
 */
fp_viewid fp_create_layer_view(fp_viewid* views, unsigned int layerCount, unsigned int width, unsigned int height, unsigned int layerWidth, unsigned int layerHeight) {
	fp_layer* layers = malloc(layerCount * sizeof(fp_layer));
	if(!layers) {
		printf("error: fp_create_layer_view: failed to allocate memory for layers\n");
		return 0;
	}

	fp_view_layer_data* layerData = malloc(sizeof(fp_view_layer_data));
	if(!layerData) {
		printf("error: fp_create_layer_view: failed to allocate memory for layerData\n");
		free(layers);
		return 0;
	}

	layerData->layerCount = layerCount;
	layerData->layers = layers;
	layerData->frame = fp_create_frame(width, height, rgb(0,0,0));

	fp_view_data data = { .LAYER = layerData };
	
	fp_viewid id = fp_create_view(FP_VIEW_LAYER, 0, data);

	/* init layers */
	for(int i = 0; i < layerCount; i++) {

		fp_viewid layerView = 0;

		if(views == NULL || views[i] == 0) {
			layerView = fp_create_frame_view(layerWidth, layerHeight, rgb(0,0,0));
		}
		else {
			layerView = views[i];
		}

		fp_layer layer = {
			layerView,
			FP_BLEND_REPLACE,
			0,
			0,
			255
		};
		layers[i] = layer;
		(&viewPool[layer.view])->parent = id;
	}

	return id;
}


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

	fp_view_data data = { .TRANSITION = transitionData };
	
	fp_viewid id = fp_create_view(FP_VIEW_TRANSITION, 0, data);

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

bool fp_render_view(fp_viewid id) {
	if(id >= viewCount) {
		return false;
	}

	fp_view* view = &viewPool[id];
	switch(view->type) {
		case FP_VIEW_FRAME:
			break;
		case FP_VIEW_SCREEN:
			if(view->data.SCREEN->childView) {
				fp_fset_rect(
					view->data.SCREEN->frame,
					0, 0,
					fp_get_frame(fp_get_view_frame(view->data.SCREEN->childView))
				);
			}

			fp_render(view->data.SCREEN->frame);
			break;
		case FP_VIEW_ANIM:
			break;
		case FP_VIEW_LAYER:
			{
				// clear
				fp_frame* layerFrame = fp_get_frame(view->data.LAYER->frame);
				fp_ffill_rect(
					view->data.LAYER->frame,
					0, 0,
					layerFrame->width, layerFrame->length / layerFrame->width,
					rgb(0, 0, 0)
				);

				// draw higher indexed layers last
				for(int i = 0; i < view->data.LAYER->layerCount; i++) {
					switch(view->data.LAYER->layers[i].blendMode) {
						case FP_BLEND_OVERWRITE:
							fp_fset_rect(
								view->data.LAYER->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view_frame(view->data.LAYER->layers[i].view))
							);
							break;
						case FP_BLEND_REPLACE:
							fp_fset_rect_transparent(
								view->data.LAYER->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view_frame(view->data.LAYER->layers[i].view))
							);
							break;
						case FP_BLEND_ADD:
							fp_fadd_rect(
								view->data.LAYER->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view_frame(view->data.LAYER->layers[i].view))
							);
							break;
						case FP_BLEND_MULTIPLY:
							fp_fmultiply_rect(
								view->data.LAYER->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view_frame(view->data.LAYER->layers[i].view))
							);
							break;
						case FP_BLEND_ALPHA:
							{
								uint8_t srcAlpha = view->data.LAYER->layers[i].alpha;
								fp_fblend_rect(
									&rgb_alpha,
									view->data.LAYER->frame,
									255,
									view->data.LAYER->layers[i].offsetX,
									view->data.LAYER->layers[i].offsetY,
									fp_get_frame(fp_get_view_frame(view->data.LAYER->layers[i].view)),
									srcAlpha
								);
								break;
							}
					}
				}
				break;
			}
		case FP_VIEW_TRANSITION:
			// TODO: add anim_view start/stop/pause functions
			// add nextPage, previousPage, setPage, cycle functions that trigger transition animation playback
			{
				fp_frame* frame = fp_get_frame(view->data.TRANSITION->frame);
				fp_frame* frameA = fp_get_frame(fp_get_view_frame(view->data.TRANSITION->pages[view->data.TRANSITION->previousPageIndex]));
				fp_frame* frameB = fp_get_frame(fp_get_view_frame(view->data.TRANSITION->pages[view->data.TRANSITION->pageIndex]));

				fp_frame* transitionA = fp_get_frame(fp_get_view_frame(view->data.TRANSITION->transition.viewA));
				fp_frame* transitionB = fp_get_frame(fp_get_view_frame(view->data.TRANSITION->transition.viewB));
				

				for(int row = 0; row < fp_frame_height(frame); row++) {
					for(int col = 0; col < frame->width; col++) {
						rgb_color colorA = rgb(0, 0, 0);
						rgb_color colorB = rgb(0, 0, 0);
						uint8_t alphaA = 0;
						uint8_t alphaB = 0;

						if(row < fp_frame_height(transitionA) && col < transitionA->width) {
							uint16_t indexA = transitionA->pixels[calc_index(col, row, transitionA->width)].mapFields.index;
							if(indexA < frameA->length) {
								colorA = frameA->pixels[indexA];
							}
							alphaA = transitionA->pixels[calc_index(col, row, transitionA->width)].mapFields.alpha;
						}

						if(row < fp_frame_height(transitionB) && col < transitionB->width) {
							uint16_t indexB = transitionB->pixels[calc_index(col, row, transitionB->width)].mapFields.index;
							if(indexB < frameB->length) {
								colorB = frameB->pixels[indexB];
							}
							alphaB = transitionB->pixels[calc_index(col, row, transitionB->width)].mapFields.alpha;
						}
						

						frame->pixels[calc_index(col, row, frame->width)] =
							(*view->data.TRANSITION->blendFn)(colorA, alphaA, colorB, alphaB);
					}
				}
				break;
			}
	}

	view->dirty = false;

	return true;
}

void fp_mark_view_dirty(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	view->dirty = true;
	if(view->parent) {
		fp_mark_view_dirty(view->parent);
	}
}

/* can trigger re-render on dirty views */
fp_frameid fp_get_view_frame(fp_viewid id) {
	fp_view* view = fp_get_view(id);
	// TODO: handle invalid view id
	if(view->dirty) {
		fp_render_view(id);
	}
	switch(view->type) {
		case FP_VIEW_FRAME:
			return view->data.FRAME->frame;
		case FP_VIEW_SCREEN:
			return view->data.SCREEN->frame;
		case FP_VIEW_ANIM:
			return fp_get_view_frame(view->data.ANIM->frames[view->data.ANIM->frameIndex]);
		case FP_VIEW_LAYER:
			return view->data.LAYER->frame;
		case FP_VIEW_TRANSITION:
			return view->data.TRANSITION->frame;
	}

	return 0;
}

fp_transition fp_create_sliding_transition(unsigned int width, unsigned int height, unsigned int frameratePeriodMs) {
	unsigned int frameCount = width + 1;

	fp_transition transition = {
		fp_create_anim_view(NULL, frameCount, frameratePeriodMs, width, height),
		fp_create_anim_view(NULL, frameCount, frameratePeriodMs, width, height)
	};

	fp_view* transitionViewA = fp_get_view(transition.viewA);
	fp_view* transitionViewB = fp_get_view(transition.viewB);

	for(int i = 0; i < frameCount; i++) {
		fp_frame* frameA = fp_get_frame(fp_get_view_frame(transitionViewA->data.ANIM->frames[i]));
		fp_frame* frameB = fp_get_frame(fp_get_view_frame(transitionViewB->data.ANIM->frames[i]));

		for(int row = 0; row < height; row++) {
			for(int col = 0; col < width; col++) {
				// TODO: calculate indexes for the transitions
				// it might be useful to write a helper function that generates these?
				unsigned int aIndex = calc_index(col, row, width);
				unsigned int aAlpha = 0;
				if(col < (width - i)) {
					aAlpha = 255;
				}

				unsigned int bIndex = 0;
				unsigned int bAlpha = 0;
				if(col >= (width - i)) {
					bAlpha = 255;
				}

				frameA->pixels[calc_index(col, row, width)].mapFields.index = aIndex;
				frameA->pixels[calc_index(col, row, width)].mapFields.alpha = aAlpha;

				frameB->pixels[calc_index(col, row, width)].mapFields.index = bIndex;
				frameB->pixels[calc_index(col, row, width)].mapFields.alpha = bAlpha;
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

unsigned int fp_frame_height(fp_frame* frame) {
	return frame->length / frame->width;
}
