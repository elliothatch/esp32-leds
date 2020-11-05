#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "../input.h"

typedef struct fp_button {
	fp_input_isr_config pin;
	void (*onInput) (struct fp_button*);
	void* data;

	bool state;
	TickType_t lastInputTick; // used to debounce
} fp_button;


fp_button* fp_button_init(
	unsigned int pin,
	void (*onInput) (fp_button*),
	xQueueHandle taskQueue,
	void* data
);

bool fp_button_free(fp_button* button);
void fp_button_oninput(void* data);

#endif /* BUTTON_H */
