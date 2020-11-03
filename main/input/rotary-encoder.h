#ifndef ROTARY_ENCODER_H
#define ROTARY_ENCODER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "../input.h"


/** detect a full step when the encoder has passed through all 4 states
 * and returned to the OFF state, completing a cycle.
 * negative values represent negative rotation */
/* #define RE_STATE_OFF 0 */
/* #define RE_STATE_CW_START 1 */
/* #define RE_STATE_ON 2 */
/* #define RE_STATE_CW_END 3 */

typedef struct fp_rotary_encoder {
	fp_input_isr_config pinA;
	fp_input_isr_config pinB;
	void (*onPositionChange) (struct fp_rotary_encoder*);

	/** number of ticks forward/backward from starting position */
	int position;
	/** sub-position of rotary encoder; used to detect a position change */
	int state;

	int lastInputA;
	int lastInputB;

	void* data;
} fp_rotary_encoder;


fp_rotary_encoder* fp_rotary_encoder_init(
	unsigned int pinA,
	unsigned int pinB,
	void (*onPositionChange) (fp_rotary_encoder*),
	xQueueHandle taskQueue,
	void* data
);

bool fp_rotary_encoder_free(fp_rotary_encoder* re);

bool fp_rotary_encoder_update_state(fp_rotary_encoder* re, int a, int b, int aPrev, int bPrev);

void fp_rotary_encoder_oninput(void* data);
void fp_rotary_encoder_on_position_change_printdbg(fp_rotary_encoder* re);

#endif /* ROTARY_ENCODER_H */
