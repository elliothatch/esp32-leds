/* Hello World Example
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#define NUM_LEDS 64
#include "ws2812_control.h"
#include "color.h"

#define RED   0xFF0000
#define GREEN 0x00FF00
#define BLUE  0x0000FF
#define WHITE 0xFFFFFF
#define BLACK 0x000000

void app_main()
{
    printf("Hello world!\n");

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

    ws2812_control_init();

    struct led_state new_state;
    for(int i = 0; i < NUM_LEDS; i++) {
        rgb_color color = hsv_to_rgb(hsv((uint8_t)((float)i/(float)NUM_LEDS * 255.0), 255, 255));
        new_state.leds[i] = color.bits;
    }

    new_state.leds[0] = RED;
    new_state.leds[7] = GREEN;
    new_state.leds[63-7] = BLUE;
    new_state.leds[63] = WHITE;

    ws2812_write_leds(new_state);

    for (int i = 5; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}

