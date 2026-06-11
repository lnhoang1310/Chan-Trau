#include <stdio.h>
#include "sim_a7680c.h"
#include "wifi.h"
#include "blynk.h"
#include "lora_ra01h.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "MAIN"

void app_main(void)
{
    if (!sim_init())
    {
        ESP_LOGE(TAG, "SIM Init Failed\n");
        return;
    }

    if (!sim_network_connect("internet"))
    {
        ESP_LOGE(TAG, "SIM Network Connect Failed\n");
        return;
    }

    // if(!lora_init()){
    //     ESP_LOGE(TAG, "LORA Init Failed\n");
    //     return;
    // }

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
