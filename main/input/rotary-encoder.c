#include "rotary-encoder.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


typedef struct {
	xQueueHandle taskQueue;
} fp_input;

typedef struct {
	unsigned int gpioPin;
	bool (*oninput) (void*);
	void* data;
} fp_input_event;

static void fp_input_task(void* options) {
	fp_input* input = options;
	fp_input_event event;
	for(;;) {
		if(xQueueReceive(input->taskQueue, &event, portMAX_DELAY)) {
			event.oninput(event.data);
		}
	}
}

static void IRAM_ATTR fp_input_isr_handler(void* arg) {
	fp_input* input = arg;
	xQueueSendFromISR(input->taskQueue, &input->event, NULL);
}

fp_rotary_encoder rre = fp_rotary_encoder_init(19, 21, 

fp_rotary_encoder fp_rotary_encoder_init(unsigned int pinA, unsigned int pinB, xQueueHandle queue) {
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = (1ULL<<pinA) | (1ULL<<pinB);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;

	gpio_config(&io_conf);

	gpio_isr_handler_add(pinA, fp_input_isr_handler, pinA);
	gpio_isr_handler_add(pinB, fp_input_isr_handler, pinB);

	return (fp_rotary_encoder){0, 0, NULL, 0, 0, 0, 0};
}

bool fp_rotary_encoder_update_state(fp_rotary_encoder* re, int a, int b, int aPrev, int bPrev) {
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
		stateDiff = 2*(re->state >= 0? 1: -1);
	}

	re->state += stateDiff;

	/* printf("%d %d [%d %d] (%d %d) (%d %d)\n", re_state, stateDiff, statePrev, state, aPrev, bPrev, a, b); */

	if(re->state > 3) {
		re->state -= 4;
		re->position++;
		return true;
	}
	else if(re->state < -3) {
		re->state += 4;
		re->position--;
		return true;
	}
	
	return false;
}

bool fp_rotary_encoder_oninput(fp_rotary_encoder* re) {
	int inputA = !gpio_get_level(re->pinA);
	int inputB = !gpio_get_level(re->pinB);

	if(fp_rotary_encoder_update_state(re, inputA, inputB, re->lastInputA, re->lastInputB)) {
		re->onPositionChange(re);
	}

	// update rotary encoder
	re->lastInputA = inputA;
	re->lastInputB = inputB;
}

void fp_rotary_encoder_oninput_printdbg(fp_rotary_encoder* re) {
	printf("RE %4d: ", re->position);
	if(re->position >= 0) {
		for(int i = 0; i < re->position; i++) {
			printf("+");
		}
	}
	else {
		for(int i = 0; i > re->position; i--) {
			printf("-");
		}
	}
	printf("\n");
}


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
					printf("RE %4d: ", re->position);
					if(re->position >= 0) {
						for(int i = 0; i < re->position; i++) {
							printf("+");
						}
					}
					else {
						for(int i = 0; i > re->position; i--) {
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
