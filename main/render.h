#ifndef RENDER_H
#define RENDER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "color.h"
#include "frame.h"

/* fp: fresh pixel */

#define FP_PENDING_VIEW_RENDER_COUNT 64

bool fp_render(fp_frameid id);

typedef enum {
	SET_RECT,
	FILL_RECT,
	RENDER,
	RENDER_VIEW
} fp_command;

typedef unsigned int fp_viewid;

typedef union {
	struct {
		fp_frameid id;
		unsigned int x;
		unsigned int y;
		fp_frame* frame;
	} SET_RECT;
	struct {
		fp_frameid id;
		unsigned int x;
		unsigned int y;
		unsigned int width;
		unsigned int height;
		rgb_color color;
	} FILL_RECT;
	struct {
		fp_frameid id;
	} RENDER;
	struct {
		fp_viewid id;
	} RENDER_VIEW;
} fp_fargs;

typedef struct {
	fp_command cmd;
	fp_fargs fargs;
} fp_queue_command;

typedef struct {
	fp_viewid view;
	TickType_t tick; /* the view will be as soon as possible after this tick */
} fp_pending_view_render;


/** freeRTOS task that constantly renders at given framerate */
typedef struct {
	int refresh_period_ms;
	fp_viewid rootView;
	QueueHandle_t commands;
	/* prevents shutting down the chip while rendering, which can cause bright flashes */
	SemaphoreHandle_t shutdownLock;
} fp_task_render_params;

bool fp_queue_render(fp_viewid view, TickType_t tick);
fp_pending_view_render fp_dequeue_render();

void fp_task_render(void *pvParameters);

#endif /* RENDER_H */
