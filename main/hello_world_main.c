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
#include "pixel.h"
#include "ppm.h"

#define LED_QUEUE_LENGTH 16 

#define GPIO_INPUT_PIN_0 19
#define GPIO_INPUT_PIN_1 21
#define GPIO_INPUT_PIN_2 3

#define GPIO_INPUT_PIN_MASK ((1ULL<<GPIO_INPUT_PIN_0) | (1ULL<<GPIO_INPUT_PIN_1) | (1ULL<<GPIO_INPUT_PIN_2))
#define ESP_INTR_FLAG_DEFAULT 0


fp_viewid create_animation_test();
fp_viewid create_animated_layer_test();
fp_viewid create_layer_alpha_test();
fp_viewid create_transition_test();
fp_viewid create_animated_transition_test();
fp_viewid create_nvs_image_test();

void init_gpio_test();

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

	fp_viewid screenViewId = fp_create_screen_view(8, 8);

	/* fp_viewid mainViewId = create_animation_test(); */
	/* fp_viewid mainViewId = create_animated_layer_test(); */
	/* fp_viewid mainViewId = create_layer_alpha_test(); */
	/* fp_viewid mainViewId = create_transition_test(); */
	/* fp_viewid mainViewId = create_animated_transition_test(); */
	fp_viewid mainViewId = create_nvs_image_test();


	fp_view* screenView = fp_get_view(screenViewId);
	fp_view* mainView = fp_get_view(mainViewId);

	mainView->parent = screenViewId;
	screenView->data.SCREEN->childView = mainViewId;

	fp_task_render_params renderParams = { 1000/60, screenViewId, ledQueue, ledShutdownLock };

	xTaskCreate(fp_task_render, "Render LED Task", 2048*4, &renderParams, 1, NULL);


	init_gpio_test();

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

fp_viewid create_animation_test(fp_viewid screenView) {
	const unsigned int frameCount = 60;

	fp_viewid animViewId = fp_create_anim_view(NULL, frameCount, 3000/frameCount, 8, 8);
	fp_view* animView = fp_get_view(animViewId);

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

	fp_play_anim(animViewId);

	return animViewId;
}

fp_viewid create_animated_layer_test(fp_viewid screenView) {
	const unsigned int layerCount = 5;

	fp_viewid animViewIds[layerCount];
	const unsigned int frameCount = 60;

	for(int layerIndex = 0; layerIndex < layerCount - 1; layerIndex++) {
		animViewIds[layerIndex] = fp_create_anim_view(NULL, frameCount, 2000/frameCount, 4, 4);
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
	animViewIds[4] = fp_create_anim_view(NULL, maskFrameCount, 4000/maskFrameCount, 4, 4);
	fp_view* maskAnimView = fp_get_view(animViewIds[4]);
	for(int i = 0; i < maskFrameCount; i++) {
		unsigned int brightness = 50*abs(i - (int)maskFrameCount/2)/(maskFrameCount/2);
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
			/* layer->blendMode = FP_BLEND_ALPHA; */
			/* layer->alpha = 255/4 */

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

	return layerViewId;
}

fp_viewid create_layer_alpha_test() {

	const unsigned int layerCount = 4;

	fp_viewid layerViewId = fp_create_layer_view(NULL, layerCount, 8, 8, 5, 5);
	fp_view* layerView = fp_get_view(layerViewId);

	fp_layer* layers[layerCount];
	layers[0] = &layerView->data.LAYER->layers[0];
	layers[1] = &layerView->data.LAYER->layers[1];
	layers[2] = &layerView->data.LAYER->layers[2];
	layers[3] = &layerView->data.LAYER->layers[3];

	layers[0]->blendMode = FP_BLEND_ALPHA;
	layers[0]->alpha = 255/4;
	fp_ffill_rect(
		fp_get_view_frame(layers[0]->view),
		0, 0,
		5, 5,
		rgb(255, 0, 0)
	);

	layers[1]->offsetX = 3;
	layers[1]->blendMode = FP_BLEND_ALPHA;
	layers[1]->alpha = 255/4;
	fp_ffill_rect(
		fp_get_view_frame(layers[1]->view),
		0, 0,
		5, 5,
		rgb(0, 255, 0)
	);

	layers[2]->offsetX = 0;
	layers[2]->offsetY = 3;
	layers[2]->blendMode = FP_BLEND_ALPHA;
	layers[2]->alpha = 255/4;
	fp_ffill_rect(
		fp_get_view_frame(layers[2]->view),
		0, 0,
		5, 5,
		rgb(0, 0, 255)
	);

	layers[3]->offsetX = 4;
	layers[3]->offsetY = 4;
	layers[3]->blendMode = FP_BLEND_ALPHA;
	layers[3]->alpha = 255*7/8;
	fp_ffill_rect(
		fp_get_view_frame(layers[3]->view),
		0, 0,
		5, 5,
		rgb(0, 0, 0)
	);

	/*
	rgb_color colors[] = {
		{{255, 0, 0}},
		{{0, 255, 0}},
		{{0, 0, 255}},
	};

	uint8_t alphas[] = {
		255,
		255/8,
		255/4
	};
	for(int i = 0; i < layerCount; i++) {
		unsigned int width = 5;
		unsigned int height = 5;
		fp_layer* layer = &layerView->data.LAYER->layers[i];
		layer->offsetX = 2*i;
		layer->offsetY = 2*i;

		if(i == 1) {
			width = 4;
			height = 4;
		}

		if(i == 2) {
			layer->offsetX -= 1;
			layer->offsetY -= 1;
		}
		// layer->blendMode = FP_BLEND_OVERWRITE;
		// layer->blendMode = FP_BLEND_ADD;
		// layer->blendMode = FP_BLEND_MULTIPLY;
		layer->blendMode = FP_BLEND_ALPHA;
		layer->alpha = alphas[i];

		fp_ffill_rect(
			fp_get_view_frame(layer->view),
			0, 0,
			width, height,
			colors[i]
		);
	}
	*/

	return layerViewId;
}

fp_viewid create_transition_test() {
	unsigned int pageCount = 3;

	fp_transition transition = fp_create_sliding_transition(8, 8, 1000/8);
	fp_viewid transitionViewId = fp_create_transition_view(NULL, pageCount, transition, 2000, 8, 8);
	fp_view* transitionView = fp_get_view(transitionViewId);

	rgb_color colors[] = {
		rgb(100, 0, 0),
		rgb(0, 100, 0),
		rgb(0, 0, 100)
	};

	for(int i = 0; i < pageCount; i++) {
		fp_ffill_rect(
			fp_get_view_frame(transitionView->data.TRANSITION->pages[i]),
			0, 0,
			8, 8,
			colors[i]);
	}

	fp_transition_loop(transitionViewId, false);

	return transitionViewId;
}

fp_viewid create_animated_transition_test() {
	unsigned int pageCount = 3;

	fp_viewid animViewIds[pageCount];
	const unsigned int frameCount = 60;

	for(int pageIndex = 0; pageIndex < pageCount; pageIndex++) {
		animViewIds[pageIndex] = fp_create_anim_view(NULL, frameCount, 1000/frameCount, 8, 8);
		fp_view* animView = fp_get_view(animViewIds[pageIndex]);

		for(int i = 0; i < frameCount; i++) {
			fp_ffill_rect(
				fp_get_view_frame(animView->data.ANIM->frames[i]),
				0, 0,
				8, 8,
				hsv_to_rgb(hsv(
					255*pageIndex/pageCount
					+ (uint8_t)(255.0/(pageCount*4) * (cos(2.0*M_PI * (double)i/(double)frameCount)+1.0) / 2),
					255,
					25))
			);
		}

		fp_play_anim(animViewIds[pageIndex]);
	}

	fp_viewid pageViews[] = {
		animViewIds[0],
		animViewIds[1],
		animViewIds[2]
	};


	fp_transition transition = fp_create_sliding_transition(8, 8, 1000/8);
	fp_viewid transitionViewId = fp_create_transition_view(pageViews, pageCount, transition, 2000, 8, 8);

	fp_transition_loop(transitionViewId, false);

	return transitionViewId;
}

fp_viewid create_nvs_image_test() {
	/*
	esp_err_t err = nvs_flash_init_partition("storage");
	ESP_ERROR_CHECK(err);

	nvs_handle_t rainImageHandle;

	err = nvs_open(IMAGE_NAMESPACE, NVS_READONLY, &rainImageHandle);
	if(err != ESP_OK) {
		printf("Error (%s) opening NVS namespace!\n", esp_err_to_name(err));
		ESP_ERROR_CHECK(err);
	}

	size_t imageSize = 0;
	err = nvs_get_blob(rainImageHandle, "rain", NULL, &imageSize);
	if(err != ESP_OK) {
		printf("Error (%s) getting blob size!\n", esp_err_to_name(err));
		ESP_ERROR_CHECK(err);
	}

	char* imageBuffer = malloc(imageSize);
	err = nvs_get_blob(rainImageHandle, "rain", imageBuffer, &imageSize);
	if(err != ESP_OK) {
		printf("Error (%s) getting blob data!\n", esp_err_to_name(err));
		free(imageBuffer);
		ESP_ERROR_CHECK(err);
	}

	*/

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

	FILE* file = fopen("/spiffs/test-pat.ppm", "rb");
	if(!file) {
		printf("Error (%s) opening file\n", esp_err_to_name(err));
		abort();
	}

	fseek(file, 0, SEEK_END);
	size_t fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	printf("file is %d bytes\n", fileSize);
	char* fileBuffer = malloc(fileSize + 1);
	fread(fileBuffer, 1, fileSize, file);
	fclose(file);

	fp_frameid frameid = fp_ppm_create_frame(fileBuffer, fileSize);
	fp_viewid viewid = fp_create_frame_view_composite(frameid);

	free(fileBuffer);

	return viewid;
}

/** detect a full step when the encoder has passed through all 4 states
 * and returned to the OFF state, completing a cycle.
 * negative values represent negative rotation */
#define RE_STATE_OFF 0
#define RE_STATE_CW_START 1
#define RE_STATE_ON 2
#define RE_STATE_CW_END 3

/* number of ticks forward/backward from starting position */
static int re_position = 0;
/* sub-position of rotary encoder; used to detect a position change */
static int re_state = RE_STATE_OFF;

static bool rotary_encoder_update_state(int a, int b, int aPrev, int bPrev) {
	/* state truth table
	 * a b o
	 * 0 0 0
	 * 0 1 1
	 * 1 0 3
	 * 1 1 2
	 */
	int state = 2*a + b - a*b + a*!b;
	int statePrev = 2*aPrev + bPrev - aPrev*bPrev + aPrev*!bPrev;

	int stateDiff = state - statePrev;

	if(stateDiff == -3) {
		stateDiff = 1;
	}
	else if(stateDiff == 3) {
		stateDiff = -1;
	}

	if(stateDiff == 0) {
		return false;
	}

	/* if 2, continue in same direction as before, or default CW */
	if(stateDiff == 2 || stateDiff == -2) {
		stateDiff = 2*(re_state >= 0? 1: -1);
	}

	re_state += stateDiff;

	/* printf("%d %d [%d %d] (%d %d) (%d %d)\n", re_state, stateDiff, statePrev, state, aPrev, bPrev, a, b); */

	if(re_state > 3) {
		re_state -= 4;
		re_position++;
		return true;
	}
	else if(re_state < -3) {
		re_state += 4;
		re_position--;
		return true;
	}
	
	return false;
}

static int input0_prev = 0;
static int input1_prev = 0;
static int input0 = 0;
static int input1 = 0;

/* static int input2 = 0; */

static xQueueHandle gpio_evt_queue = NULL;
static void gpio_handle_input_task(void* arg) {
	uint32_t io_num;
	for(;;) {
		if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
			/* printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num)); */
			/*
			switch(io_num) {
				case GPIO_INPUT_PIN_0:
					input0 = gpio_get_level(io_num);
					break;
				case GPIO_INPUT_PIN_1:
					input1 = gpio_get_level(io_num);
					break;
				case GPIO_INPUT_PIN_2:
					input2 = gpio_get_level(io_num);
					break;
			}

			if(io_num == GPIO_INPUT_PIN_0 && input0 == 1) {
				if(input1 == 0) {
					// clockwise
					counter++;

				}
				else {
					// counter-clockwise
					counter--;
				}
				if(counter >= 0) {
					for(int i = 0; i < counter; i++) {
						printf("+");
					}
				}
				else {
					for(int i = 0; i > counter; i--) {
						printf("-");
					}
				}
				printf("\n");
			}
			*/

			if(io_num == GPIO_INPUT_PIN_0 || io_num == GPIO_INPUT_PIN_1) {
				// update rotary encoder
				input0_prev = input0;
				input1_prev = input1;

				input0 = !gpio_get_level(GPIO_INPUT_PIN_0);
				input1 = !gpio_get_level(GPIO_INPUT_PIN_1);
				if(rotary_encoder_update_state(input0, input1, input0_prev, input1_prev)) {
					printf("RE %4d: ", re_position);
					if(re_position >= 0) {
						for(int i = 0; i < re_position; i++) {
							printf("+");
						}
					}
					else {
						for(int i = 0; i > re_position; i--) {
							printf("-");
						}
					}
					printf("\n");
				}
			}
			else {
				printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
			}
		}
	}
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void re_poll_test() {
	for(;;) {
		// update rotary encoder
		input0_prev = input0;
		input1_prev = input1;

		input0 = !gpio_get_level(GPIO_INPUT_PIN_0);
		input1 = !gpio_get_level(GPIO_INPUT_PIN_1);
		if(rotary_encoder_update_state(input0, input1, input0_prev, input1_prev)) {
			if(re_position >= 0) {
				for(int i = 0; i < re_position; i++) {
					printf("+");
				}
			}
			else {
				for(int i = 0; i > re_position; i--) {
					printf("-");
				}
			}
			printf("\n");
		}
		vTaskDelay(1);
	}
}


void init_gpio_test() {
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_MASK;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;

	gpio_config(&io_conf);

	/* xTaskCreate(re_poll_test, "re_poll_test", 2048, NULL, 10, NULL); */
	
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	xTaskCreate(gpio_handle_input_task, "gpio_handle_input_task", 2048, NULL, 10, NULL);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(GPIO_INPUT_PIN_0, gpio_isr_handler, (void*) GPIO_INPUT_PIN_0);
	gpio_isr_handler_add(GPIO_INPUT_PIN_1, gpio_isr_handler, (void*) GPIO_INPUT_PIN_1);
	gpio_isr_handler_add(GPIO_INPUT_PIN_2, gpio_isr_handler, (void*) GPIO_INPUT_PIN_2);
}
