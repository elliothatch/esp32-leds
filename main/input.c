#include "input.h"

/* #include <stdio.h> */

void fp_input_task(void* options) {
	QueueHandle_t taskQueue = options;

	fp_gpio_pin_config pin;
	for(;;) {
		if(xQueueReceive(taskQueue, &pin, portMAX_DELAY)) {
			/* printf("pin %d\n", pin.pin); */
			pin.oninput(pin.data);
		}
	}
}

void IRAM_ATTR fp_input_isr_handler(void* arg) {
	fp_input_isr_config* config = arg;
	xQueueSendFromISR(config->taskQueue, &config->pin_config, NULL);
}

