#include "button.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "freertos/task.h"

unsigned int debounceTime = 20; // millsections

fp_button* fp_button_init(
	unsigned int pin,
	void (*onInput) (fp_button*),
	xQueueHandle taskQueue,
	void* data
) {
	fp_button* button = malloc(sizeof(fp_button));
	if(button == NULL) {
		// ERROR
	}

	button->pin.taskQueue = taskQueue;
	button->pin.pin_config.pin = pin;
	button->pin.pin_config.oninput = &fp_button_oninput;
	button->pin.pin_config.data = button;

	button->onInput = onInput;

	button->state = 0;
	button->data = data;

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = (1ULL<<pin);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;

	gpio_config(&io_conf);

	gpio_isr_handler_add(pin, &fp_input_isr_handler, &button->pin);

	return button;
}

bool fp_button_free(fp_button* button) {
	gpio_isr_handler_remove(button->pin.pin_config.pin);
	free(button);

	return true;
}

void fp_button_oninput(void* data) {
	fp_button* button = data;

	
	// TODO: add to fp_input the ability to queue an input event at a certain time, so we can retrigger oninput after the debounceTime
	/* TickType_t currentTick = xTaskGetTickCount(); */
	/* if(currentTick - button->lastInputTick < pdMS_TO_TICKS(debounceTime)) { */
	/* } */

	int state = !gpio_get_level(button->pin.pin_config.pin);
	if(state != button->state) {
		button->state = state;
		button->onInput(button);
	}
}
