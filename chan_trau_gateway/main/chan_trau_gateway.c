#include <stdio.h>
#include "sim_a7680c.h"
#include "wifi.h"
#include "blynk.h"
#include "lora_ra01h.h"
#include "esp_log.h"
#include "gps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define TAG "MAIN"

nmea_msg gpsx;

void app_main(void)
{
    uint8_t buf[256];
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << 2),
        .pull_down_en = 0,
        .pull_up_en = 0
    };
    gpio_config(&io_conf);
    gpio_set_level(2, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(2, 1);
    // if (!sim_init())
    // {
    //     ESP_LOGE(TAG, "SIM Init Failed\n");
    //     return;
    // }

    // if (!sim_network_connect("internet"))
    // {
    //     ESP_LOGE(TAG, "SIM Network Connect Failed\n");
    // }

    if (!lora_init())
    {
        ESP_LOGE(TAG, "LORA Init Failed\n");
        return;
    }

    // NMEA_GNRMC_Init();
    lora_set_preamble_length(3);
    lora_set_frequency(868E6);
    lora_set_spreading_factor(7);
    lora_set_bandwidth(125E3);
    lora_set_coding_rate(5);
    lora_set_sync_word(0x12);

    while (1)
    {
        char msg[] = "ESP32 LoRa";
        lora_send_packet((uint8_t*)msg, strlen(msg));
        ESP_LOGI(TAG, "Send OK\n");
        gpio_set_level(2, (gpio_get_level(2) == 1) ? 0 : 1);
        vTaskDelay(pdMS_TO_TICKS(1000));

        // lora_receive();
        // if(lora_received())
        // {
        //     ESP_LOGI(TAG, "Packet received\n");
        // }
        // int len = lora_receive_packet(buf, sizeof(buf)-1);
        // if(len > 0)
        // {
        //     buf[len] = '\0';
        //     printf("Recv: %s\n", buf);
        //     gpio_set_level(2, gpio_get_level(2) ^ 1);
        // }
        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}
