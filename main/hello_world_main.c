/* Hello World Example
*/
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

	fp_task_render_params renderParams = { 1000/60, ledQueue, ledShutdownLock };

	fp_viewid screenViewId = fp_create_screen_view(8, 8);
	
	const unsigned int layerCount = 5;
	fp_viewid layerViewId = fp_create_layer_view(layerCount, 4, 4);
	fp_view* layerView = fp_get_view(layerViewId);
	layerView->parent = screenViewId;
	for(int i = 0; i < layerCount; i++) {
		fp_layer* layer = &layerView->data.LAYER->layers[i];
		layer->blendMode = FP_BLEND_ADD;
		if(i != layerCount - 1) {
			layer->offsetX = 4 * (i % 2);
			layer->offsetY = 4 * (i / 2);
		}
		else {
			layer->offsetX = 2;
			layer->offsetY = 2;
		}

		fp_ffill_rect(
			fp_get_view(layer->view)->data.FRAME->frame,
			0, 0,
			4, 4,
			hsv_to_rgb(hsv(255*i/layerCount, 255, 25))
		);
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

	fp_queue_command renderCommand = {
		RENDER_VIEW,
		{.RENDER_VIEW = { layerViewId }}
	};

	if(xQueueSend(ledQueue, &renderCommand, 0) != pdPASS) {
		printf("failed to send render command to led queue\n");
	}

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

