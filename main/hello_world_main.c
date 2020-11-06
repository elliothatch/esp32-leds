/* Hello World Example
*/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

/* #include "nvs_flash.h" */
/* #include "nvs.h" */
#include "esp_spiffs.h"

#include "esp_err.h"

#include "driver/gpio.h"

#define IMAGE_NAMESPACE "image"

#define NUM_LEDS 64
#include "ws2812_control.h"
#include "color.h"
#include "frame.h"
#include "render.h"
#include "ppm.h"

#include "input.h"
#include "input/button.h"
#include "input/rotary-encoder.h"

#include "view.h"
#include "views/frame-view.h"
#include "views/ws2812-view.h"
#include "views/anim-view.h"
#include "views/layer-view.h"
#include "views/transition-view.h"
#include "views/dynamic-view.h"

#define LED_QUEUE_LENGTH 16 

#define SCREEN_WIDTH 8
#define SCREEN_HEIGHT 8

#define GPIO_INPUT_PIN_0 19
#define GPIO_INPUT_PIN_1 21
#define GPIO_INPUT_PIN_2 3

#define GPIO_INPUT_PIN_MASK ((1ULL<<GPIO_INPUT_PIN_0) | (1ULL<<GPIO_INPUT_PIN_1) | (1ULL<<GPIO_INPUT_PIN_2))
#define ESP_INTR_FLAG_DEFAULT 0

void init_gpio_test();

typedef struct {
	fp_viewid (*init_mode) (void**);
	bool (*free_mode) (fp_view*, void**);
	fp_viewid view;
	void* data;
} demo_mode;

fp_viewid frame_view_demo_init(void** data) {
	fp_viewid frameView = fp_frame_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, rgb(0, 0, 0));
	fp_frameid frameId = fp_view_get_frame(frameView);
	for(int i = 0; i < SCREEN_WIDTH; i++) {
		for(int j = 0; j < SCREEN_HEIGHT; j++) {
			fp_fset(frameId, i, j, rgb(
				255 - ((i+j)*255/(SCREEN_WIDTH+SCREEN_HEIGHT)),
				i*255/SCREEN_WIDTH,
				j*255/SCREEN_HEIGHT)
		   );
		}
	}

	return frameView;
}

bool frame_view_demo_free(fp_view* view, void** data) {
	return true;
}

bool dynamic_view_demo_render(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	fp_frame* frame = fp_frame_get(dynamicData->frame);

	for(int i = 0; i < SCREEN_WIDTH; i++) {
		for(int j = 0; j < SCREEN_HEIGHT; j++) {
			rgb_color color = frame->pixels[fp_fcalc_index(i, j, frame->width)];
			color.fields.r = fmax(color.fields.r-1, 0);
			color.fields.g = fmax(color.fields.g-1, 0); 
			color.fields.b = fmax(color.fields.b-1, 0);
			/*
			if(color.fields.r == 0
				&& color.fields.g == 0
				&& color.fields.b == 0) {
				color.bits = esp_random();
			}
			*/

			frame->pixels[fp_fcalc_index(i, j, frame->width)] = color;
		}
	}

	/** raindrop */
	TickType_t* lastDrop = dynamicData->data;
	TickType_t currentTick = xTaskGetTickCount();
	if(currentTick > *lastDrop + pdMS_TO_TICKS(100)) {
		rgb_color color = {.bits = esp_random()};
		frame->pixels[esp_random() % frame->length] = color;
		*lastDrop = currentTick;
	}

	return true;

	/* fp_frame* frame = fp_frame_get(dynamicData->frame); */
	/* frame->pixels[0] */
}

fp_viewid dynamic_view_demo_init(void** data) {
	TickType_t* lastDrop = malloc(sizeof(TickType_t));
	*lastDrop = xTaskGetTickCount();
	fp_viewid dynamicView = fp_dynamic_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, dynamic_view_demo_render, NULL, lastDrop);
	/* fp_frameid frameId = fp_view_get_frame(dynamicView); */

	/** init with noisy pattern */
	/*
	for(int i = 0; i < SCREEN_WIDTH; i++) {
		for(int j = 0; j < SCREEN_HEIGHT; j++) {
			rgb_color color = {.bits = esp_random()};
			fp_fset(frameId, i, j, color);
		}
	}
	*/

	return dynamicView;
}

bool dynamic_view_demo_free(fp_view* view, void** data) {
	fp_dynamic_view_data* viewData = view->data;
	TickType_t* lastDrop = viewData->data;
	free(lastDrop);
	return true;
}

fp_viewid animation_view_demo_init(void** data) {
	const unsigned int frameCount = 60;

	fp_viewid animViewId = fp_anim_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, frameCount, 3000/frameCount);
	fp_view* animView = fp_view_get(animViewId);
	fp_anim_view_data* animData = animView->data;

	for(int i = 0; i < frameCount; i++) {
		for(int j = 0; j < 8; j++) {
			fp_ffill_rect(
				((fp_frame_view_data*)fp_view_get(animData->frames[i])->data)->frame,
				0, j,
				8, 1,
				hsv_to_rgb(hsv(((255*i/frameCount)+(255*j/8))%256, 255, 255))
			);
		}
	}

	fp_anim_play(animViewId);

	return animViewId;
}

bool animation_view_demo_free(fp_view* view, void** data) {
	return true;
}

fp_viewid layer_view_demo_init(void** data) {
	fp_viewid layerViewId = fp_layer_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, 6, 6, 3);

	fp_view* layerView = fp_view_get(layerViewId);

	fp_layer_view_data* layerData = layerView->data;

	layerData->layers[0].blendMode = FP_BLEND_ADD;
	layerData->layers[0].offsetX = 1;
	layerData->layers[0].offsetY = 0;
	fp_ffill_rect(fp_view_get_frame(layerData->layers[0].view), 0, 0, 6, 5, rgb(255, 0, 0));

	layerData->layers[1].blendMode = FP_BLEND_ADD;
	layerData->layers[1].offsetX = 0;
	layerData->layers[1].offsetY = 3;
	fp_ffill_rect(fp_view_get_frame(layerData->layers[1].view), 0, 0, 5, 4, rgb(0, 255, 0));

	layerData->layers[2].blendMode = FP_BLEND_ADD;
	layerData->layers[2].offsetX = 3;
	layerData->layers[2].offsetY = 3;
	fp_ffill_rect(fp_view_get_frame(layerData->layers[2].view), 0, 0, 5, 4, rgb(0, 0, 255));

	return layerViewId;
}

bool layer_view_demo_free(fp_view* view, void** data) {
	return true;
}

fp_viewid layer_view_alpha_demo_init(void** data) {
	const unsigned int layerCount = 4;

	fp_viewid layerViewId = fp_layer_view_create(8, 8, 5, 5, layerCount);
	fp_view* layerView = fp_view_get(layerViewId);
	fp_layer_view_data* layerData = layerView->data;

	fp_layer* layers[layerCount];
	layers[0] = &layerData->layers[0];
	layers[1] = &layerData->layers[1];
	layers[2] = &layerData->layers[2];
	layers[3] = &layerData->layers[3];

	layers[0]->blendMode = FP_BLEND_ALPHA;
	layers[0]->alpha = 255/4;
	fp_ffill_rect(
		fp_view_get_frame(layers[0]->view),
		0, 0,
		5, 5,
		rgb(255, 0, 0)
	);

	layers[1]->offsetX = 3;
	layers[1]->blendMode = FP_BLEND_ALPHA;
	layers[1]->alpha = 255/4;
	fp_ffill_rect(
		fp_view_get_frame(layers[1]->view),
		0, 0,
		5, 5,
		rgb(0, 255, 0)
	);

	layers[2]->offsetX = 0;
	layers[2]->offsetY = 3;
	layers[2]->blendMode = FP_BLEND_ALPHA;
	layers[2]->alpha = 255/4;
	fp_ffill_rect(
		fp_view_get_frame(layers[2]->view),
		0, 0,
		5, 5,
		rgb(0, 0, 255)
	);

	layers[3]->offsetX = 4;
	layers[3]->offsetY = 4;
	layers[3]->blendMode = FP_BLEND_ALPHA;
	layers[3]->alpha = 255*7/8;
	fp_ffill_rect(
		fp_view_get_frame(layers[3]->view),
		0, 0,
		5, 5,
		rgb(0, 0, 0)
	);
	return layerViewId;
}

bool layer_view_alpha_demo_free(fp_view* view, void** data) {
	return true;
}

void draw_arc_filled(fp_frameid id, int centerX, int centerY, int radius, float startTheta, float endTheta, rgb_color color) {
	if(endTheta - startTheta >= M_PI) {
		// can't deal with large angles
		float centerAngle = (endTheta - startTheta)/2.0f + startTheta;
		draw_arc_filled(id, centerX, centerY, radius, startTheta, centerAngle, color);
		draw_arc_filled(id, centerX, centerY, radius, centerAngle, endTheta, color);
		return;
	}

	/* normal vectors for each angle */
	float startNormalX = radius * sin(startTheta + M_PI_2);
	float startNormalY = radius * cos(startTheta + M_PI_2);

	float endNormalX = radius * sin(endTheta - M_PI_2);
	float endNormalY = radius * cos(endTheta - M_PI_2);

	/* reduce the radius threshold to make the circle appear rounder for small values */
	float radiusModifier = 0.7f;

	/* just check every pixel in the enclosing rectangle and set the appropriate pixels */
	for(int y = -radius; y <= radius; y++) {
		for(int x = -radius; x <= radius; x++) {
			if(x*x + y*y < radius*radius * radiusModifier) {
				// in circle
				if(// check if clockwise from start
					-startNormalX*y + startNormalY*x > 0 &&
					// check counterclockwise from end
					-endNormalX*y + endNormalY*x < 0) {
					/* true) { */

					fp_fset(id, centerX + x, centerY + y, color);
				}
			}
		}
	}
}

fp_viewid spinning_ball_demo_init(void** data) {
	rgb_color colors[] = {
		rgb(255, 0, 0),
		rgb(0, 255, 0),
		rgb(0, 0, 255),
		rgb(255, 255, 0),
		rgb(255, 0, 255),
	};
	float angle = 2.0*M_PI/5.0;

	unsigned int frameCount = 30;
	fp_viewid animViewId = fp_anim_view_create(8, 8, frameCount, 1000/30);
	fp_view* animView = fp_view_get(animViewId);
	fp_anim_view_data* animData = animView->data;

	float angleOffset = 2.0f*M_PI / animData->frameCount;

	for(int i = 0; i < animData->frameCount; i++) {
		for(int j = 0; j < 5; j++) {
			draw_arc_filled(fp_view_get_frame(animData->frames[i]), 3, 3, 4, j*angle + i*angleOffset, (j+1)*angle + i*angleOffset, colors[j]);
		}
	}

	fp_viewid layers[] = {
		animViewId,
		fp_frame_view_create(3, 1, rgb(255, 0, 0)), // replace
		fp_frame_view_create(1, 3, rgb(255, 0, 0)), // add
		fp_frame_view_create(3, 1, rgb(255, 0, 0)), // multiply
		fp_frame_view_create(1, 3, rgb(255, 0, 0))  // alpha
	};

	fp_viewid layerViewId = fp_layer_view_create_composite(8, 8, layers, 5);
	fp_view* layerView = fp_view_get(layerViewId);

	fp_layer_view_data* layerData = layerView->data;

	layerData->layers[1].blendMode = FP_BLEND_REPLACE;
	layerData->layers[1].offsetX = 4;
	layerData->layers[1].offsetY = 3;

	layerData->layers[2].blendMode = FP_BLEND_ADD;
	layerData->layers[2].offsetX = 3;
	layerData->layers[2].offsetY = 0;

	layerData->layers[3].blendMode = FP_BLEND_MULTIPLY;
	layerData->layers[3].offsetX = 0;
	layerData->layers[3].offsetY = 3;

	layerData->layers[4].blendMode = FP_BLEND_ALPHA;
	layerData->layers[4].offsetX = 3;
	layerData->layers[4].offsetY = 4;
	layerData->layers[4].alpha = 255/2;

	fp_anim_play(animViewId);
	return layerViewId;
}

bool spinning_ball_demo_free(fp_view* view, void** data) {
	fp_layer_view_data* layerData = view->data;
	for(int i = 0; i < layerData->layerCount; i++) {
		fp_view_free(layerData->layers[i].view);
	}
	return true;
}

fp_viewid animated_layer_view_demo_init(void** data) {
	const unsigned int layerCount = 5;

	fp_viewid animViewIds[layerCount];
	const unsigned int frameCount = 60;

	for(int layerIndex = 0; layerIndex < layerCount - 1; layerIndex++) {
		animViewIds[layerIndex] = fp_anim_view_create(4, 4, frameCount, 2000/frameCount);
		fp_view* animView = fp_view_get(animViewIds[layerIndex]);
		fp_anim_view_data* animData = animView->data;

		for(int i = 0; i < frameCount; i++) {
			for(int j = 0; j < 4; j++) {
				int hueOffset = 0;
				if(layerIndex == 1) {
					hueOffset = 1;
				}
				if(layerIndex == 2) {
					hueOffset = 1;
				}
				if(layerIndex == 3) {
					hueOffset = 2;
				}
				if(layerIndex == 0 || layerIndex == 3) {
					fp_ffill_rect(
						fp_view_get_frame(animData->frames[((layerIndex+1) % 2)*(frameCount - 1 - 2*i) + i]),
						0, j,
						4, 1,
						hsv_to_rgb(hsv(
							(
								 255 * hueOffset / (layerCount - 1)
								+ (255*i/frameCount + 255*j/frameCount)
							)% 256,
							/* 255 * layerIndex / layerCount */
							/* + (((255*i/frameCount + 255*j/4) % 256) */
							/*   / layerCount), */
							255,
							255))
					);
				}
				else {
					fp_ffill_rect(
						fp_view_get_frame(animData->frames[((layerIndex+1) % 2)*(frameCount - 1 - 2*i) + i]),
						j, 0,
						1, 4,
						hsv_to_rgb(hsv(
							(
								 255 * hueOffset / (layerCount - 1)
								+ (255*i/frameCount + 255*j/frameCount)
							)% 256,
							/* 255 * layerIndex / layerCount */
							/* + (((255*i/frameCount + 255*j/4) % 256) */
							/*   / layerCount), */
							255,
							255))
					);
				}
			}
		}
	}

	/* mask fades in and out */
	const unsigned int maskFrameCount = 60;
	animViewIds[4] = fp_anim_view_create(4, 4, maskFrameCount, 4000/maskFrameCount);
	fp_view* maskAnimView = fp_view_get(animViewIds[4]);
	fp_anim_view_data* maskAnimData = maskAnimView->data;

	for(int i = 0; i < maskFrameCount; i++) {
		unsigned int brightness = 255*abs(i - (int)maskFrameCount/2)/(maskFrameCount/2);
		fp_ffill_rect(
			fp_view_get_frame(maskAnimData->frames[i]),
			0, 0,
			4, 4,
			rgb(brightness, brightness, brightness)
		);
	}



	fp_viewid layerViews[] = {
		animViewIds[0],
		animViewIds[1],
		animViewIds[2],
		animViewIds[3],
		animViewIds[4],
	};

	fp_anim_play(animViewIds[0]);
	fp_anim_play(animViewIds[1]);
	fp_anim_play(animViewIds[2]);
	fp_anim_play(animViewIds[3]);
	fp_anim_play(animViewIds[4]);


	fp_viewid layerViewId = fp_layer_view_create_composite(8, 8, layerViews, layerCount);

	fp_view* layerView = fp_view_get(layerViewId);
	fp_layer_view_data* layerData = layerView->data;

	for(int i = 0; i < layerCount; i++) {
		fp_layer* layer = &layerData->layers[i];
		if(i != layerCount - 1) {
			layer->offsetX = 4 * (i % 2);
			layer->offsetY = 4 * (i / 2);
		}
		else {
			layer->offsetX = 2;
			layer->offsetY = 2;
			layer->blendMode = FP_BLEND_ADD;
		}
	}

	return layerViewId;

}

bool animated_layer_view_demo_free(fp_view* view, void** data) {
	fp_layer_view_data* layerData = view->data;
	for(int i = 0; i < layerData->layerCount; i++) {
		fp_view_free(layerData->layers[i].view);
	}
	return true;
}

fp_viewid transition_view_demo_init(void** data) {
	unsigned int pageCount = 3;

	fp_transition transition = fp_create_sliding_transition(8, 8, 1000/8);
	fp_viewid transitionViewId = fp_create_transition_view(8, 8, pageCount, transition, 2000);
	fp_view* transitionView = fp_view_get(transitionViewId);
	fp_transition_view_data* transitionData = transitionView->data;

	rgb_color colors[] = {
		rgb(255, 0, 0),
		rgb(0, 255, 0),
		rgb(0, 0, 255)
	};

	for(int i = 0; i < pageCount; i++) {
		fp_ffill_rect(
			fp_view_get_frame(transitionData->pages[i]),
			0, 0,
			8, 8,
			colors[i]);
	}

	fp_transition_loop(transitionViewId, false);

	return transitionViewId;
}

bool transition_view_demo_free(fp_view* view, void** data) {
	fp_transition_view_data* transitionData = view->data;
	fp_view_free(transitionData->transition.viewA);
	fp_view_free(transitionData->transition.viewB);
	return true;
}

fp_viewid animated_transition_view_demo_init(void** data) {
	unsigned int pageCount = 3;

	fp_viewid animViewIds[pageCount];
	const unsigned int frameCount = 60;

	for(int pageIndex = 0; pageIndex < pageCount; pageIndex++) {
		animViewIds[pageIndex] = fp_anim_view_create(8, 8, frameCount, 1000/frameCount);
		fp_view* animView = fp_view_get(animViewIds[pageIndex]);
		fp_anim_view_data* animData = animView->data;

		for(int i = 0; i < frameCount; i++) {
			fp_ffill_rect(
				fp_view_get_frame(animData->frames[i]),
				0, 0,
				8, 8,
				hsv_to_rgb(hsv(
					255*pageIndex/pageCount
					+ (uint8_t)(255.0/(pageCount*4) * (cos(2.0*M_PI * (double)i/(double)frameCount)+1.0) / 2),
					255,
					255))
			);
		}

		fp_anim_play(animViewIds[pageIndex]);
	}

	fp_viewid pageViews[] = {
		animViewIds[0],
		animViewIds[1],
		animViewIds[2]
	};


	fp_transition transition = fp_create_sliding_transition(8, 8, 1000/8);
	fp_viewid transitionViewId = fp_create_transition_view_composite(8, 8, pageViews, pageCount, transition, 2000);

	fp_transition_loop(transitionViewId, false);

	return transitionViewId;
}

bool animated_transition_view_demo_free(fp_view* view, void** data) {
	fp_transition_view_data* transitionData = view->data;

	fp_view_free(transitionData->transition.viewA);
	fp_view_free(transitionData->transition.viewB);

	for(int i = 0; i < transitionData->pageCount; i++) {
		fp_view_free(transitionData->pages[i]);
	}

	return true;
}

fp_viewid ppm_image_demo_init(void** data) {
	fp_frameid frame = fp_ppm_load_image("/spiffs/test-pat.ppm");
	fp_viewid view = fp_frame_view_create_composite(frame);
	fp_view_get(view)->composite = false; // when the view is free it will automatically free the frame

	return view;
}
	
bool ppm_image_demo_free(fp_view* view, void** data) {
	return true;
}

#define MAZE_N 1
#define MAZE_E 1<<1
#define MAZE_S 1<<2
#define MAZE_W 1<<3


int maze_dx(unsigned int direction) {
	return (((direction & MAZE_E) >> 1) * 1) | (((direction & MAZE_W) >> 3) * -1);
}

int maze_dy(unsigned int direction) {
	return ((direction & MAZE_N) * 1) | (((direction & MAZE_S) >> 2) * -1);
}

unsigned int maze_rotate_cw(unsigned int direction) {
	return ((direction << 1) | (direction >> 3)) & 0xf;
}

unsigned int maze_rotate_ccw(unsigned int direction) {
	return ((direction >> 1) | (direction << 3)) & 0xf;
}

unsigned int maze_opposite(unsigned int direction) {
	/* swap N/S and E/W values */
	return ((direction & MAZE_N) << 2) | ((direction & MAZE_S) >> 2) |
		((direction & MAZE_E) << 2) | ((direction & MAZE_W) >> 2);
}

unsigned int MAZE_DIRECTIONS[] = { MAZE_N, MAZE_S, MAZE_E, MAZE_W };

void maze_carve_passages(fp_frame* frame, unsigned int x,unsigned int y) {

	unsigned int directions[] = { MAZE_N, MAZE_S, MAZE_E, MAZE_W };
	// shuffle
	for(int i = 3; i >= 0; i--) {
		unsigned int target = esp_random() % (i+1); /* not very random */

		unsigned int temp = directions[i];
		directions[i] = directions[target];
		directions[target] = temp;
	}

	for(int i = 0; i < 4; i++) {
		unsigned int direction = directions[i];
		unsigned int oppositeDirection = maze_opposite(direction);

		int nextX = maze_dx(direction) + x;
		int nextY = maze_dy(direction) + y;
		unsigned int nextIndex = fp_fcalc_index(nextX, nextY, frame->width);

		if(fp_frame_has_point(frame, nextX, nextY) && frame->pixels[nextIndex].bits == 0) {
			frame->pixels[fp_fcalc_index(x, y, frame->width)].bits |= direction;
			frame->pixels[nextIndex].bits |= oppositeDirection;

			maze_carve_passages(frame, nextX, nextY);
		}
	}
}

typedef struct {
	unsigned int x;
	unsigned int y;
	fp_frameid maze;

	unsigned int lastDirection;
	TickType_t lastStepTick;
} maze_state;

bool maze_render(fp_view* view) {
	fp_dynamic_view_data* viewData = view->data;
	fp_frame* viewFrame = fp_frame_get(viewData->frame);

	maze_state* maze = viewData->data;
	fp_frame* mazeFrame = fp_frame_get(maze->maze);

	TickType_t lastStepTick = maze->lastStepTick;
	TickType_t currentTick = xTaskGetTickCount();

	if(currentTick > lastStepTick + pdMS_TO_TICKS(500)) {
		// try to step to the right
		unsigned int direction = maze_rotate_cw(maze->lastDirection);
		int index = fp_fcalc_index(maze->x, maze->y, mazeFrame->width);

		do {
			unsigned int oppositeDirection = maze_opposite(direction);

			int nextX = maze_dx(direction) + maze->x;
			int nextY = maze_dy(direction) + maze->y;
			unsigned int nextIndex = fp_fcalc_index(nextX, nextY, mazeFrame->width);

			if(fp_frame_has_point(mazeFrame, nextX, nextY)
				&& (mazeFrame->pixels[index].bits & direction) != 0
				&& (mazeFrame->pixels[nextIndex].bits & oppositeDirection) != 0) {
				maze->x = nextX;
				maze->y = nextY;
				maze->lastDirection = direction;
				break;
			}

			direction = maze_rotate_ccw(direction);
		} while(direction != maze_rotate_cw(maze->lastDirection));

		maze->lastStepTick = currentTick;
	}


	fp_ffill_rect(viewData->frame, 0, 0, viewFrame->width, fp_frame_height(viewFrame), rgb(0,0,0));

	fp_fset(viewData->frame, maze->x, maze->y, rgb(255, 255, 0));

	/* cast rays in 4 directions to find and illuminate walls */
	for(int i = 0; i < 4; i++) {
		unsigned int direction = MAZE_DIRECTIONS[i];
		unsigned int oppositeDirection = maze_opposite(direction);
		unsigned int leftDirection = maze_rotate_ccw(direction);
		unsigned int rightDirection = maze_rotate_cw(direction);

		int x = maze->x;
		int y = maze->y;

		do {
			unsigned int index = fp_fcalc_index(x, y, mazeFrame->width);

			int nextX = x + maze_dx(direction);
			int nextY = y + maze_dy(direction);
			unsigned int nextIndex = fp_fcalc_index(nextX, nextY, mazeFrame->width);

			if(!fp_frame_has_point(mazeFrame, nextX, nextY)) {
				// outside frame
				break;
			}

			if((mazeFrame->pixels[index].bits & direction) == 0
				|| (mazeFrame->pixels[nextIndex].bits & oppositeDirection) == 0) {
				// draw a wall and stop
				viewFrame->pixels[nextIndex] = rgb(255, 255, 255);
				break;
			}
			else {
				// draw floor
				viewFrame->pixels[nextIndex] = rgb(255, 0, 0);
			}


			int leftX = nextX + maze_dx(leftDirection);
			int leftY = nextY + maze_dy(leftDirection);
			unsigned int leftIndex = fp_fcalc_index(leftX, leftY, mazeFrame->width);

			int rightX = nextX + maze_dx(rightDirection);
			int rightY = nextY + maze_dy(rightDirection);
			unsigned int rightIndex = fp_fcalc_index(rightX, rightY, mazeFrame->width);
			

			if(fp_frame_has_point(mazeFrame, leftX, leftY) && (
				(mazeFrame->pixels[nextIndex].bits & leftDirection) == 0
				|| (mazeFrame->pixels[leftIndex].bits & rightDirection) == 0)
			) {
				// draw a wall
				viewFrame->pixels[leftIndex] = rgb(255, 255, 255);
			}

			if(fp_frame_has_point(mazeFrame, rightX, rightY) && (
				(mazeFrame->pixels[nextIndex].bits & rightDirection) == 0
				&& (mazeFrame->pixels[rightIndex].bits & leftDirection) == 0)
			) {
				// draw a wall
				viewFrame->pixels[rightIndex] = rgb(255, 255, 255);
			}

			x = nextX;
			y = nextY;
			

		} while(true);
	}

	return true;
}

fp_viewid maze_demo_init(void** data) {
	fp_frameid maze = fp_frame_create(SCREEN_WIDTH, SCREEN_HEIGHT, rgb(0, 0, 0));
	fp_frame* mazeFrame = fp_frame_get(maze);
	maze_carve_passages(mazeFrame, 0, 0);

	maze_state* state = malloc(sizeof(maze_state));

	state->x = mazeFrame->width / 2;
	state->y = fp_frame_height(mazeFrame) / 2;
	state->maze = maze;
	state->lastDirection = MAZE_N;
	state->lastStepTick = xTaskGetTickCount();

	fp_viewid view = fp_dynamic_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, &maze_render, NULL, state);

	return view;

}
	
bool maze_demo_free(fp_view* view, void** data) {
	fp_dynamic_view_data* viewData = view->data;
	maze_state* maze = viewData->data;
	fp_frame_free(maze->maze);
	free(maze);
	return true;
}

demo_mode demos[] = {{
	&frame_view_demo_init,
	&frame_view_demo_free,
	0,
	NULL
}, {
	&dynamic_view_demo_init,
	&dynamic_view_demo_free,
	0,
	NULL
}, {
	&animation_view_demo_init,
	&animation_view_demo_free,
	0,
	NULL
}, {
	&layer_view_demo_init,
	&layer_view_demo_free,
	0,
	NULL
}, {
	&layer_view_alpha_demo_init,
	&layer_view_alpha_demo_free,
	0,
	NULL
}, {
	&spinning_ball_demo_init,
	&spinning_ball_demo_free,
	0,
	NULL
}, {
	&animated_layer_view_demo_init,
	&animated_layer_view_demo_free,
	0,
	NULL
}, {
	&transition_view_demo_init,
	&transition_view_demo_free,
	0,
	NULL
}, {
	&animated_transition_view_demo_init,
	&animated_transition_view_demo_free,
	0,
	NULL
}, {
	&ppm_image_demo_init,
	&ppm_image_demo_free,
	0,
	NULL
}, {
	&maze_demo_init,
	&maze_demo_free,
	0,
	NULL
}};

const unsigned int DEMO_COUNT = sizeof(demos) / sizeof(demo_mode);

demo_mode* currentDemo = NULL;
unsigned int demoIndex = 0;

bool demo_select_render(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	fp_frame* frame = fp_frame_get(dynamicData->frame);

	if(dynamicData->data == false && currentDemo != NULL && currentDemo->view != 0) {
		fp_fset_rect(dynamicData->frame, 0, 0, fp_frame_get(fp_view_get_frame(currentDemo->view)));
		return true;
	}

	/* draw static */
	for(int i = 0; i < SCREEN_WIDTH; i++) {
		for(int j = 0; j < SCREEN_HEIGHT; j++) {
			/* uint8_t value = fp_fcalc_index(i, j, frame->width) * 255 / frame->length; */
			uint8_t value = gamma8[(uint8_t)esp_random()];// % 100;
			frame->pixels[fp_fcalc_index(i, j, frame->width)] = rgb(value, value, value);
		}
	}
	return true;
}

bool demo_select_onnext_render(fp_view* view) {
	fp_dynamic_view_data* dynamicData = view->data;
	dynamicData->data = false; /* don't show static */

	return true;
}

SemaphoreHandle_t ledRenderLock = NULL;

fp_viewid play_demo(fp_viewid selectViewId, demo_mode* demo) {
	fp_view* selectView = fp_view_get(selectViewId);
	fp_dynamic_view_data* dynamicData = selectView->data;
	dynamicData->data = (void*)true;


	TickType_t startTick = xTaskGetTickCount();

	if(currentDemo != NULL) {
		xSemaphoreTake(ledRenderLock, portMAX_DELAY);

		/* reset any pending renders initialized by the previous demo */
		fp_queue_reset();

		demo_mode* lastDemo = currentDemo;
		/* clear current demo to prevent trying to render from it while it's freeing */
		currentDemo = NULL;

		xSemaphoreGive(ledRenderLock);
		/* the last render should be finished, so we can free memory now that the current demo is cleared */

		lastDemo->free_mode(fp_view_get(lastDemo->view), &lastDemo->data);
		if(lastDemo->view != 0) {
			if(fp_view_free(lastDemo->view)) {
				lastDemo->view = 0;
			}
		}
	}

	fp_viewid view = demo->init_mode(&demo->data);
	fp_view* demoView = fp_view_get(view);

	demoView->parent = selectViewId;

	currentDemo = demo;
	currentDemo->view = view;



	TickType_t loadedTick = xTaskGetTickCount();
	fp_queue_render(selectViewId, fmax(startTick + pdMS_TO_TICKS(300), loadedTick));

	return view;
}

static xQueueHandle demoQueue = NULL;

void select_demo(fp_rotary_encoder* re) {

	/* return fp_queue_render(animView, currentTick + pdMS_TO_TICKS(animData->frameratePeriodMs)); */ 
	/*
	fp_viewid selectViewId = (unsigned int)re->data;
	fp_view* selectView = fp_view_get(selectViewId);
	fp_viewid screenViewId = selectView->parent;

	fp_ws2812_view_data* screenViewData = fp_view_get(screenViewId)->data;
	screenViewData->brightness = re->position / 20.0f;
	printf("brightness %f\n", screenViewData->brightness);
	fp_view_mark_dirty(screenViewId);
	*/

	unsigned int index;
	if(re->position < 0) {
		index = DEMO_COUNT - ((-re->position) % DEMO_COUNT) ;
	}
	else {
		index = abs(re->position) % DEMO_COUNT;
	}

	/* queue the demo set on the main task, since free/init may take awhile */
	if(xQueueSend(demoQueue, &index, 0) != pdPASS) {
			printf("failed to queue next demo\n");
	}
}

void change_brightness(fp_button* button) {
	printf("button state %d\n", button->state);

	if(button->state == 1) {
		 /* only trigger on release */
		return;
	}

	fp_viewid screenViewId = (fp_viewid)button->data;
	fp_view* screenView = fp_view_get(screenViewId);
	fp_ws2812_view_data* screenData = screenView->data;

	float brightness = screenData->brightness + 0.2;
	if(brightness >= 1.0f) {
		brightness -= 1.0f;
	}

	screenData->brightness = brightness;
}

static xQueueHandle gpio_evt_queue = NULL;

void app_main()
{
	/*BaseType_t taskResult = */

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

	/* init spiffs to load images from filesystem */
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = false
	};

	esp_err_t err = esp_vfs_spiffs_register(&conf);

	if(err != ESP_OK) {
		printf("Error (%s) initializing spiffs\n", esp_err_to_name(err));
		ESP_ERROR_CHECK(err);
	}

	QueueHandle_t ledQueue = xQueueCreate(LED_QUEUE_LENGTH, sizeof(fp_queue_command));
	if(!ledQueue) {
		printf("Failed to allocate queue for led render task\n");
	}

	ledRenderLock = xSemaphoreCreateBinary();

	if(!ledRenderLock) {
		printf("Failed to create semaphore ledRenderLock\n");
	}
	xSemaphoreGive(ledRenderLock);

	/* init_gpio_test(); */

	fp_frame_init(512);
	fp_view_init(512);

	ws2812_control_init();

	fp_view_register_type(FP_VIEW_FRAME, fp_frame_view_register_data);
	fp_view_register_type(FP_VIEW_WS2812, fp_ws2812_view_register_data);
	fp_view_register_type(FP_VIEW_ANIM, fp_anim_view_register_data);
	fp_view_register_type(FP_VIEW_LAYER, fp_layer_view_register_data);
	fp_view_register_type(FP_VIEW_TRANSITION, fp_transition_view_register_data);
	fp_view_register_type(FP_VIEW_DYNAMIC, fp_dynamic_view_register_data);

	fp_viewid screenViewId = fp_create_ws2812_view(SCREEN_WIDTH, SCREEN_HEIGHT);
	fp_viewid mainViewId = fp_dynamic_view_create(SCREEN_WIDTH, SCREEN_HEIGHT, &demo_select_render, demo_select_onnext_render, (void*)true);

	play_demo(mainViewId, &demos[0]);

	fp_ws2812_view_set_child(screenViewId, mainViewId);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

	gpio_evt_queue = xQueueCreate(10, sizeof(fp_gpio_pin_config));
	xTaskCreate(fp_input_task, "fp_input_task", 4096, gpio_evt_queue, 10, NULL);

	demoQueue = xQueueCreate(10, sizeof(demoIndex));

	fp_rotary_encoder* re = fp_rotary_encoder_init(19, 21, &select_demo, gpio_evt_queue, (void*)mainViewId);

	fp_button* button = fp_button_init(3, &change_brightness, gpio_evt_queue, (void*)screenViewId);

	/* fp_rotary_encoder* re = fp_rotary_encoder_init(19, 21, &fp_rotary_encoder_on_position_change_printdbg, gpio_evt_queue); */

	/* bootloader_random_enable(); */

	fp_task_render_params renderParams = { 1000/60, screenViewId, ledQueue, ledRenderLock };

	vTaskPrioritySet(NULL, 1);
	xTaskCreate(fp_task_render, "Render LED Task", 2048*4, &renderParams, 5, NULL);

	unsigned int selecteDemoIndex;
	while(xQueueReceive(demoQueue, &selecteDemoIndex, portMAX_DELAY) == pdPASS) {
		if(uxQueueMessagesWaiting(demoQueue) != 0) {
			// we've already selected a new demo, skip this one
			continue;
		}

		demoIndex = selecteDemoIndex;
		printf("demo %d\n", demoIndex);
		demo_mode* demo = &demos[demoIndex];
		play_demo(mainViewId, demo);
	}

	fp_rotary_encoder_free(re);
	fp_button_free(button);

	xSemaphoreTake(ledRenderLock, portMAX_DELAY);
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();

	/*
	fp_frameid frame1 = fp_create_frame(8, 8, rgb(0, 0, 0));
	for(int i = 0; i < 8; i++) {
		fp_queue_command command = {
			FILL_RECT,
			{ .FILL_RECT = { frame1, 0, i, 8, 1, hsv_to_rgb(hsv(i*255/8, 255, 10))}}
		};
		if(xQueueSend(ledQueue, &command, 0) != pdPASS) {
			printf("failed to send command to led queue\n");
		}
	}
	*/

	/*
	fp_queue_command renderCommand = {
		RENDER_VIEW,
		{.RENDER_VIEW = { layerViewId }}
	};

	if(xQueueSend(ledQueue, &renderCommand, 0) != pdPASS) {
		printf("failed to send render command to led queue\n");
	}
	*/


	/* xQueueSend( */

    /* fp_frameid frame2 = fp_create_frame(6, 6, rgb(0, 255, 0)); */
    /* fp_frameid frame3 = fp_frame_create(4, 4, rgb(0, 0, 255)); */


    /* struct led_state new_state; */
    /* for(int i = 0; i < NUM_LEDS; i++) { */
    /*     rgb_color color = hsv_to_rgb(hsv((uint8_t)((float)i/(float)NUM_LEDS * 255.0), 255, 255)); */
    /*     new_state.leds[i] = color.bits; */
    /* } */

    /* new_state.leds[0] = rgb(255, 0, 0).bits; */
    /* new_state.leds[7] = rgb(0, 255, 0).bits; */
    /* new_state.leds[63-7] = rgb(0, 0, 255).bits; */
    /* new_state.leds[63] = rgb(255, 255, 255).bits; */

    /* ws2812_write_leds(new_state); */

    /* for (int i = 5; i >= 0; i--) { */
    /*     printf("Restarting in %d seconds...\n", i); */
    /*     vTaskDelay(1000 / portTICK_PERIOD_MS); */
    /* } */
}
