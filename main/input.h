#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
	unsigned int pin;
	void (*oninput) (void*);
	void* data;
} fp_gpio_pin_config;

typedef struct {
	QueueHandle_t taskQueue;
	fp_gpio_pin_config pin_config;
} fp_input_isr_config;


void fp_input_task(void* options);
void IRAM_ATTR fp_input_isr_handler(void* arg);


#endif /* INPUT_H */
