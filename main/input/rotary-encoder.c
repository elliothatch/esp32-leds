#include "rotary-encoder.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

fp_rotary_encoder* fp_rotary_encoder_init(
	unsigned int pinA,
	unsigned int pinB,
	void (*onPositionChange) (fp_rotary_encoder*),
	xQueueHandle taskQueue,
	void* data
) {
	fp_rotary_encoder* re = malloc(sizeof(fp_rotary_encoder));
	if(re == NULL) {
		// ERROR
	}

	re->pinA.taskQueue = taskQueue;
	re->pinA.pin_config.pin = pinA;
	re->pinA.pin_config.oninput = &fp_rotary_encoder_oninput;
	re->pinA.pin_config.data = re;

	re->pinB.taskQueue = taskQueue;
	re->pinB.pin_config.pin = pinB;
	re->pinB.pin_config.oninput = &fp_rotary_encoder_oninput;
	re->pinB.pin_config.data = re;

	re->onPositionChange = onPositionChange;

	re->position = 0;
	re->state = 0;
	re->lastInputA = 0;
	re->lastInputB = 0;

	re->data = data;

	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.pin_bit_mask = (1ULL<<pinA) | (1ULL<<pinB);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;

	gpio_config(&io_conf);

	gpio_isr_handler_add(pinA, &fp_input_isr_handler, &re->pinA);
	gpio_isr_handler_add(pinB, &fp_input_isr_handler, &re->pinB);

	return re;
}

bool fp_rotary_encoder_free(fp_rotary_encoder* re) {
	gpio_isr_handler_remove(re->pinA.pin_config.pin);
	gpio_isr_handler_remove(re->pinB.pin_config.pin);
	free(re);

	return true;
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

void fp_rotary_encoder_oninput(void* data) {
	fp_rotary_encoder* re = data;
	int inputA = !gpio_get_level(re->pinA.pin_config.pin);
	int inputB = !gpio_get_level(re->pinB.pin_config.pin);

	if(fp_rotary_encoder_update_state(re, inputA, inputB, re->lastInputA, re->lastInputB)) {
		re->onPositionChange(re);
	}

	// update rotary encoder
	re->lastInputA = inputA;
	re->lastInputB = inputB;
}

void fp_rotary_encoder_on_position_change_printdbg(fp_rotary_encoder* re) {
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
