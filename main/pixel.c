#include <math.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"

#include "pixel.h"
#include "ws2812_control.h"

#define FP_FRAME_COUNT 16

unsigned int calc_index(unsigned int x, unsigned int y, unsigned int width) {
	return y * width + x % width;
}

struct led_state ledState;
unsigned int frameCount = 1;
fp_frame frames[FP_FRAME_COUNT] = {{ 0, 0, NULL}};

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
			}
		}
		/* composite the image */
		/* TODO */
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
	fp_frameid id = frameCount++;
	xSemaphoreGive(createFrameLock);

	frames[id].length = length;
	frames[id].width = width;
	frames[id].pixels = pixels;

	return id;
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

bool fp_ffill_rect(
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
