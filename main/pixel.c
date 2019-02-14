#include <math.h>
#include <stdio.h>
#include <string.h>

#include "pixel.h"
#include "ws2812_control.h"

#define FP_FRAME_COUNT 16

unsigned int calc_index(unsigned int x, unsigned int y, unsigned int width) {
	return y * width + x % width;
}

struct led_state ledState;
unsigned int frameCount = 1;
fp_frame frames[FP_FRAME_COUNT] = {{ 0, 0, NULL}};

void fp_task_render(void *pvParameters) {
	/* const char* taskName = "Render LED Task"; */
	TickType_t lastWakeTime = xTaskGetTickCount();
	while(true) {
		printf("Rendering\n");
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000/60));
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

	frames[frameCount].length = length;
	frames[frameCount].width = width;
	frames[frameCount].pixels = pixels;

	return frameCount++;
}

fp_frame fp_get_frame(fp_frameid id) {
	if(id >= frameCount) {
		return frames[0];
	}

	return frames[id];
}

bool fp_fset_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	fp_frame* frame
) {
	if(id >= frameCount) {
		return false;
	}

	fp_frame* targetFrame = &frames[id];

	for(int row = 0; row < frame->length / frame->width; row++) {
		memcpy(
			&targetFrame->pixels[calc_index(x, y + row, targetFrame->width)],
			&frame->pixels[calc_index(0, row, frame->width)],
			frame->width * sizeof(((fp_frame*)0)->pixels)
		);
	}

	return true;
}

bool fp_fill_rect(
	fp_frameid id,
	unsigned int x,
	unsigned int y,
	unsigned int width,
	unsigned int height,
	rgb_color color
) {
	if(id >= frameCount) {
		return false;
	}

	fp_frame* frame = &frames[id];
	for(int row = 0; row < height; row++) {
		for(int col = 0; col < width; col++) {
			frame->pixels[calc_index(x + col, y + row, frame->width)] = color;
		}
	}

	return true;
}

bool fp_render(fp_frameid id) {
	if(id >= frameCount) {
		return false;
	}

	fp_frame* frame = &frames[id];
	memcpy(ledState.leds, frame->pixels, fmin(frame->length, NUM_LEDS) * sizeof(((fp_frame*)0)->pixels));
    ws2812_write_leds(ledState);

    return true;
}
