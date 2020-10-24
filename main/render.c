#include "render.h"

#include "freertos/FreeRTOS.h"

#include "view.h"
/* TODO: this is a bad include, rework this */
#include "views/ws2812-view.h"

/* circular buffer */
unsigned int pendingViewRenderCount = 0;
unsigned int pendingViewRenderIndex = 0;
fp_pending_view_render pendingViewRenderPool[FP_PENDING_VIEW_RENDER_COUNT];

bool fp_queue_render(fp_viewid view, TickType_t tick) {
	if(pendingViewRenderCount >= FP_PENDING_VIEW_RENDER_COUNT) {
		printf("error: fp_queue_render: pending render pool full. limit: %d\n", FP_PENDING_VIEW_RENDER_COUNT);
		return false;
	}

	unsigned int nextRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
	pendingViewRenderCount++;
	pendingViewRenderPool[nextRender].view = view;
	pendingViewRenderPool[nextRender].tick = tick;

	return true;
}

fp_pending_view_render fp_dequeue_render() {

	fp_pending_view_render render = pendingViewRenderPool[pendingViewRenderIndex];
	if(pendingViewRenderCount == 0) {
		render.view = 0;
		render.tick = 0;
		return render;
	}

	pendingViewRenderIndex = (pendingViewRenderIndex + 1) % FP_PENDING_VIEW_RENDER_COUNT;
	pendingViewRenderCount--;
	
	return render;
}

void fp_task_render(void *pvParameters) {
	const fp_task_render_params* params = (const fp_task_render_params*) pvParameters;
	/* ws2812_control_init(); */

	/* uint8_t brightness = 1; */
	/* fp_frameid frame1 = fp_create_frame(8, 8, rgb(brightness, 0, 0)); */
	/* fp_ffill_rect(frame1, 0, 0, 6, 6, rgb(0, brightness, 0)); */
	/* fp_ffill_rect(frame1, 0, 0, 4, 4, rgb(0, 0, brightness)); */


	TickType_t lastWakeTime = xTaskGetTickCount();
	while(true) {

		/* process commands */
		fp_queue_command command;
		while(xQueueReceive(params->commands, &command, 0) == pdPASS) {
			switch(command.cmd) {
				case FILL_RECT:
					fp_ffill_rect(
						command.fargs.FILL_RECT.id,
						command.fargs.FILL_RECT.x,
						command.fargs.FILL_RECT.y,
						command.fargs.FILL_RECT.width,
						command.fargs.FILL_RECT.height,
						command.fargs.FILL_RECT.color
					);
					break;
				case SET_RECT:
					fp_fset_rect(
						command.fargs.SET_RECT.id,
						command.fargs.SET_RECT.x,
						command.fargs.SET_RECT.y,
						command.fargs.SET_RECT.frame
					);
					break;
				case RENDER:
					xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
					fp_render_leds_ws2812(command.fargs.RENDER.id);
					xSemaphoreGive(params->shutdownLock);
					break;
				case RENDER_VIEW:
					xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
					fp_render_view(command.fargs.RENDER_VIEW.id);
					xSemaphoreGive(params->shutdownLock);
					break;
			}
		}

		TickType_t currentTick = xTaskGetTickCount();
		/* composite the image */
		/* TODO */
		/* process each pending render by dequeuing. requeue if the render is still pending */
		/* TODO: just us a vector for this? */
		int originalPendingViewRenderCount = pendingViewRenderCount;
		for(int i = 0; i < originalPendingViewRenderCount; i++) {
			fp_pending_view_render pendingRender = fp_dequeue_render();
			if(pendingRender.tick <= currentTick) {
				fp_onnext_render(pendingRender.view);
				fp_mark_view_dirty(pendingRender.view); 
			}
			else {
				// requeue
				unsigned int nextRender = (pendingViewRenderIndex + pendingViewRenderCount) % FP_PENDING_VIEW_RENDER_COUNT;
				pendingViewRenderCount++;
				pendingViewRenderPool[nextRender].view = pendingRender.view;
				pendingViewRenderPool[nextRender].tick = pendingRender.tick;
			}
		}

		fp_view* rootView = fp_get_view(params->rootView);
		if(rootView->dirty) {
			xSemaphoreTake(params->shutdownLock, portMAX_DELAY);
			fp_render_view(params->rootView);
			xSemaphoreGive(params->shutdownLock);
		}
		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(params->refresh_period_ms));
	}
}
