/**
 * pixel.c
 * Manages a pool of "frames" and "views" to support complex displays and animations
 *  - frame: Rectangle of RGB pixels
 *  - view: Contains one or more frames and data to display those frames. If a view has a parent, the parent is rendered immediately after rendering the child
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

#define FP_FRAME_COUNT 64
#define FP_VIEW_COUNT 64
#define FP_PENDING_VIEW_RENDER_COUNT 16

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
fp_view viewPool[FP_VIEW_COUNT] = {{ FP_VIEW_FRAME, 0, {.FRAME = NULL}}};

/* circular buffer */
unsigned int pendingViewRenderCount = 0;
unsigned int pendingViewRenderIndex = 0;
fp_pending_view_render pendingViewRenderPool[FP_PENDING_VIEW_RENDER_COUNT];

/** locks fs_create_frame. allows multiple tasks to safely create frames */
SemaphoreHandle_t createFrameLock = NULL;

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
		for(int i = 0; i < pendingViewRenderCount; i++) {
			fp_pending_view_render pendingRender = pendingViewRenderPool[(i + pendingViewRenderIndex) % FP_PENDING_VIEW_RENDER_COUNT];
			if(pendingRender.tick <= currentTick) {
				fp_view* view = fp_get_view(pendingRender.view);

				if(view->type == FP_VIEW_ANIM) {
					// TODO: move this elsewhere
					// NOTE: advances frame BEFORE render, so intermediate calls to render on this frame won't change until the next pending render */
					view->data.ANIM->frameIndex = (view->data.ANIM->frameIndex + 1) % view->data.ANIM->frameCount;
					// queue up the next frame
					unsigned int nextRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
					pendingViewRenderCount++;
					pendingViewRenderPool[nextRender].view = pendingRender.view;
					pendingViewRenderPool[nextRender].tick = currentTick + pdMS_TO_TICKS(view->data.ANIM->frameratePeriodMs);
				}

				xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
				fp_render_view(pendingRender.view); 
				xSemaphoreGive(params->shutdownLock);
				// dequeue pending render
				pendingViewRenderIndex = (pendingViewRenderIndex + 1) % FP_PENDING_VIEW_RENDER_COUNT;
				pendingViewRenderCount--;
			}
		}
		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(params->refresh_period_ms));
	}
}

fp_frameid fp_create_frame(unsigned int width, unsigned int height, rgb_color color) {
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

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
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
	for(int row = 0; row < height; row++) {
		for(int col = 0; col < width; col++) {
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

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
		for(int col = 0; col < fmin(frame->width, targetFrame->width - x); col++) {
			rgb_color pixel = frame->pixels[calc_index(0, row, frame->width)];
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
		fp_frame* frame
		) {
	if(id >= framePoolCount) {
		return false;
	}

	fp_frame* targetFrame = &framePool[id];

	for(int row = 0; row < fmin(frame->length / frame->width, (targetFrame->length / targetFrame->width) - y); row++) {
		for(int col = 0; col < fmin(frame->width, targetFrame->width - x); col++) {
			rgb_color colorSum = rgb_add(
					frame->pixels[calc_index(0, row, frame->width)],
					targetFrame->pixels[calc_index(x + col, y + row, targetFrame->width)]
			);
			targetFrame->pixels[calc_index(x + col, y + row, targetFrame->width)] = colorSum;
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
	fp_viewid id = viewCount++;

	viewPool[id].type = type;
	viewPool[id].parent = parent;
	viewPool[id].data = data;

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
	fp_view_data data = { .SCREEN = screenData };

	return fp_create_view(FP_VIEW_SCREEN, 0, data);
}

fp_viewid fp_create_anim_view(unsigned int frameCount, unsigned int frameratePeriodMs, unsigned int width, unsigned int height) {
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

	fp_view_data data = { .ANIM = animData };
	fp_viewid id = fp_create_view(FP_VIEW_ANIM, 0, data);

	/* init frames */
	for(int i = 0; i < frameCount; i++) {
		frames[i] = fp_create_frame_view(width, height, rgb(0,0,0));
		(&viewPool[frames[i]])->parent = id;
	}

	// for now, immediately queue the animation for render
	unsigned int pendingRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
	pendingViewRenderCount++;
	pendingViewRenderPool[pendingRender].view = id;
	pendingViewRenderPool[pendingRender].tick = xTaskGetTickCount() + pdMS_TO_TICKS(frameratePeriodMs);

	return id;
}

fp_viewid fp_create_layer_view(unsigned int layerCount, unsigned int width, unsigned int height) {
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

	fp_view_data data = { .LAYER = layerData };
	
	fp_viewid id = fp_create_view(FP_VIEW_LAYER, 0, data);

	/* init layers */
	for(int i = 0; i < layerCount; i++) {
		fp_layer layer = {
			fp_create_frame_view(width, height, rgb(0,0,0)),
			FP_BLEND_REPLACE,
			0,
			0
		};
		layers[i] = layer;
		(&viewPool[layer.view])->parent = id;
	}

	return id;
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
			fp_render(view->data.SCREEN->frame);
			break;
		case FP_VIEW_ANIM:
			// todo: make this generic with drawable interface
			if(view->parent && fp_get_view(view->parent)->type == FP_VIEW_SCREEN) {
				fp_view* screenView = fp_get_view(view->parent);
				fp_fset_rect(
					screenView->data.SCREEN->frame,
					0, 0,
					fp_get_frame(fp_get_view(view->data.ANIM->frames[view->data.ANIM->frameIndex])->data.FRAME->frame)
				);

				/*
				fp_viewid frameViewId = view->data.ANIM->frames[view->data.ANIM->frameIndex];
				fp_view* frameView = fp_get_view(frameViewId);
				fp_frame* frame = fp_get_frame(frameView->data.FRAME->frame);
				printf("%d, %d, %d: %d %d %d\n",
						frameView->type,
						frame->length,
						frame->width,
						frame->pixels[0].fields.r,
						frame->pixels[0].fields.g,
						frame->pixels[0].fields.b
				  );
				  */
			}
			break;
		case FP_VIEW_LAYER:
			// todo: make this generic with drawable interface
			if(view->parent && fp_get_view(view->parent)->type == FP_VIEW_SCREEN) {
				fp_view* screenView = fp_get_view(view->parent);
				// draw higher indexed layers last
				for(int i = 0; i < view->data.LAYER->layerCount; i++) {
					switch(view->data.LAYER->layers[i].blendMode) {
						case FP_BLEND_OVERWRITE:
							fp_fset_rect(
								screenView->data.SCREEN->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view(view->data.LAYER->layers[i].view)->data.FRAME->frame)
							);
							break;
						case FP_BLEND_REPLACE:
							fp_fset_rect_transparent(
								screenView->data.SCREEN->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view(view->data.LAYER->layers[i].view)->data.FRAME->frame)
							);
							break;
						case FP_BLEND_ADD:
							fp_fadd_rect(
								screenView->data.SCREEN->frame,
								view->data.LAYER->layers[i].offsetX,
								view->data.LAYER->layers[i].offsetY,
								fp_get_frame(fp_get_view(view->data.LAYER->layers[i].view)->data.FRAME->frame)
							);
							break;
					}
				}
			}
			break;
	}

	if(view->parent) {
		return fp_render_view(view->parent);
	}

	return true;
}
