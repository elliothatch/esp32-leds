/* Hello World Example
*/
#include <math.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#define NUM_LEDS 64
#include "ws2812_control.h"
#include "color.h"
#include "pixel.h"

#define LED_QUEUE_LENGTH 16 

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

	QueueHandle_t ledQueue = xQueueCreate(LED_QUEUE_LENGTH, sizeof(fp_queue_command));
	if(!ledQueue) {
		printf("Failed to allocate queue for led render task\n");
	}

	SemaphoreHandle_t ledShutdownLock = xSemaphoreCreateBinary();
	if(!ledShutdownLock) {
		printf("Failed to create semaphore ledShutdownLock\n");
	}
	xSemaphoreGive(ledShutdownLock);

	fp_viewid screenViewId = fp_create_screen_view(8, 8);
	
	const unsigned int layerCount = 5;

	fp_viewid animViewIds[layerCount];
	const unsigned int frameCount = 30;

	for(int layerIndex = 0; layerIndex < layerCount - 1; layerIndex++) {
		animViewIds[layerIndex] = fp_create_anim_view(frameCount, 2000/frameCount, 4, 4);
		fp_view* animView = fp_get_view(animViewIds[layerIndex]);

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
						fp_get_view_frame(animView->data.ANIM->frames[((layerIndex+1) % 2)*(frameCount - 1 - 2*i) + i]),
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
							25))
					);
				}
				else {
					fp_ffill_rect(
						fp_get_view_frame(animView->data.ANIM->frames[((layerIndex+1) % 2)*(frameCount - 1 - 2*i) + i]),
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
							25))
					);
				}
			}
		}
	}

	/* mask fades in and out */
	const unsigned int maskFrameCount = 60;
	animViewIds[4] = fp_create_anim_view(maskFrameCount, 4000/maskFrameCount, 4, 4);
	fp_view* maskAnimView = fp_get_view(animViewIds[4]);
	for(int i = 0; i < maskFrameCount; i++) {
		unsigned int brightness = 25*abs(i - maskFrameCount/2)/(maskFrameCount/2);
		fp_ffill_rect(
			fp_get_view_frame(maskAnimView->data.ANIM->frames[i]),
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


	fp_viewid layerViewId = fp_create_layer_view(layerViews, layerCount, 8, 8, 4, 4);
	fp_view* layerView = fp_get_view(layerViewId);
	/*
	printf("%d %d %d %d %d\n",
			layerView->data.LAYER->layers[0].view,
			layerView->data.LAYER->layers[1].view,
			layerView->data.LAYER->layers[2].view,
			layerView->data.LAYER->layers[3].view,
			layerView->data.LAYER->layers[4].view
	);
	*/
	layerView->parent = screenViewId;
	for(int i = 0; i < layerCount; i++) {
		fp_layer* layer = &layerView->data.LAYER->layers[i];
		if(i != layerCount - 1) {
			layer->offsetX = 4 * (i % 2);
			layer->offsetY = 4 * (i / 2);
		}
		else {
			layer->offsetX = 2;
			layer->offsetY = 2;
		/* layer->blendMode = FP_BLEND_OVERWRITE; */
		layer->blendMode = FP_BLEND_ADD;
		/* layer->blendMode = FP_BLEND_MULTIPLY; */

			/*
			fp_ffill_rect(
				fp_get_view_frame(layer->view),
				0, 0,
				4, 4,
				rgb(255, 0, 0)
			);
			*/

			/*
			fp_ffill_rect(
				fp_get_view_frame(layer->view),
				1, 1,
				2, 2,
				rgb(0, 0, 0)
			);
			*/
		}
	}

	/** animation test */
	/*
	const unsigned int frameCount = 60;
	fp_viewid animViewId = fp_create_anim_view(frameCount, 3000/frameCount, 8, 8);
	fp_view* animView = fp_get_view(animViewId);
	animView->parent = screenViewId;
	for(int i = 0; i < frameCount; i++) {
		for(int j = 0; j < 8; j++) {
			fp_ffill_rect(
				fp_get_view(animView->data.ANIM->frames[i])->data.FRAME->frame,
				0, j,
				8, 1,
				hsv_to_rgb(hsv(((255*i/frameCount)+(255*j/8))%256, 255, 25))
			);
		}
	}
	*/

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

	fp_view* screenView = fp_get_view(screenViewId);
	/* screenView->data.SCREEN->childView = animViewId; */
	screenView->data.SCREEN->childView = layerViewId;

	fp_task_render_params renderParams = { 1000/60, screenViewId, ledQueue, ledShutdownLock };

	xTaskCreate(fp_task_render, "Render LED Task", 2048*4, &renderParams, 1, NULL);

	/* xQueueSend( */

    /* fp_frameid frame2 = fp_create_frame(6, 6, rgb(0, 255, 0)); */
    /* fp_frameid frame3 = fp_create_frame(4, 4, rgb(0, 0, 255)); */


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

	while(true) {
		vTaskDelay(portMAX_DELAY);
	}
    /* for (int i = 5; i >= 0; i--) { */
    /*     printf("Restarting in %d seconds...\n", i); */
    /*     vTaskDelay(1000 / portTICK_PERIOD_MS); */
    /* } */

	xSemaphoreTake(ledShutdownLock, portMAX_DELAY);
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

