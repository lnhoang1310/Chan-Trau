#include "sim_a7680c.h"
#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define SIM_UART UART_NUM_1
#define SIM_TX_PIN GPIO_NUM_5
#define SIM_RX_PIN GPIO_NUM_4

#define RX_BUF_SIZE 4096
#define EVENT_QUEUE_LEN 20

static const char *TAG = "SIM7680";

static QueueHandle_t sim_uart_queue;

static char sim_rx_buffer[RX_BUF_SIZE];
static volatile uint32_t sim_rx_len = 0;

static void sim_uart_event_task(void *arg)
{
    uart_event_t event;
    uint8_t data[256];
    while (1)
    {
        if (xQueueReceive(sim_uart_queue, &event, portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                int len = uart_read_bytes(SIM_UART, data, event.size, portMAX_DELAY);
                if (len > 0)
                {
                    if ((sim_rx_len + len) < RX_BUF_SIZE)
                    {
                        memcpy(&sim_rx_buffer[sim_rx_len], data, len);
                        sim_rx_len += len;
                        sim_rx_buffer[sim_rx_len] = 0;
                    }

                    ESP_LOGI(TAG, "RX[%d]: %.*s", len, len, data);
                }

                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART_FIFO_OVF");
                uart_flush_input(SIM_UART);
                xQueueReset(sim_uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART_BUFFER_FULL");
                uart_flush_input(SIM_UART);
                xQueueReset(sim_uart_queue);
                break;
            default:
                break;
            }
        }
    }
}

static void sim_clear_rx(void)
{
    sim_rx_len = 0;
    memset(sim_rx_buffer, 0, sizeof(sim_rx_buffer));
    uart_flush_input(SIM_UART);
}

static void sim_send(const char *cmd)
{
    sim_clear_rx();
    char log_cmd[128];
    sprintf(log_cmd, "%s\r\n", cmd);
    uart_write_bytes(SIM_UART, log_cmd, strlen(log_cmd));
    ESP_LOGI(TAG, "TX: %s", cmd);
}

static bool sim_wait(const char *expect, uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();

    while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms))
    {
        if (strstr(sim_rx_buffer, expect))
        {
            ESP_LOGI(TAG, "FOUND: %s", expect);
            return true;
        }

        if (strstr(sim_rx_buffer, "ERROR"))
        {
            ESP_LOGE(TAG, "ERROR RESPONSE:\n%s", sim_rx_buffer);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGE(TAG, "TIMEOUT WAITING %s", expect);
    ESP_LOGE(TAG, "RX:\n%s", sim_rx_buffer);

    return false;
}

static bool sim_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    sim_send(cmd);
    return sim_wait(expect, timeout_ms);
}

bool sim_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT};

    ESP_ERROR_CHECK(uart_param_config(SIM_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(SIM_UART, SIM_TX_PIN, SIM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(SIM_UART, RX_BUF_SIZE, RX_BUF_SIZE, EVENT_QUEUE_LEN, &sim_uart_queue, 0));

    xTaskCreate(sim_uart_event_task, "sim_uart_task", 4096, NULL, 10, NULL);
    vTaskDelay(pdMS_TO_TICKS(3000));

    bool ready = false;

    for (int i = 0; i < 10; i++)
    {
        if (sim_cmd("AT", "OK", 2000))
        {
            ready = true;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!ready)
    {
        ESP_LOGE(TAG, "AT FAILED");
        return false;
    }

    sim_cmd("ATE0", "OK", 3000);
    sim_cmd("AT+CPIN?", "READY", 5000);

    return true;
}

bool sim_network_connect(const char *apn)
{
    char cmd[128];

    if (!sim_cmd("AT+CGATT=1", "OK", 10000))
        return false;

    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", apn);

    if (!sim_cmd(cmd, "OK", 5000))
        return false;

    if (!sim_cmd("AT+CGACT=1,1", "OK", 15000))
        return false;

    sim_cmd("AT+CGPADDR=1", "OK", 3000);
    ESP_LOGI(TAG, "NETWORK CONNECTED");

    return true;
}

bool sim_http_get(const char *url)
{
    char cmd[512];

    sim_cmd("AT+HTTPTERM", "OK", 2000);

    if (!sim_cmd("AT+HTTPINIT", "OK", 5000))
        return false;

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);

    if (!sim_cmd(cmd, "OK", 5000))
        return false;

    if (!sim_cmd("AT+HTTPACTION=0", "+HTTPACTION:", 30000))
        return false;

    sim_send("AT+HTTPREAD");

    if (!sim_wait("OK", 10000))
        return false;

    sim_cmd("AT+HTTPTERM", "OK", 3000);

    return true;
}

bool sim_http_post(const char *url, const char *data)
{
    char cmd[512];

    sim_cmd("AT+HTTPTERM", "OK", 2000);

    if (!sim_cmd("AT+HTTPINIT", "OK", 3000))
        return false;

    snprintf(cmd, sizeof(cmd), "AT+HTTPPARA=\"URL\",\"%s\"", url);

    if (!sim_cmd(cmd, "OK", 3000))
        return false;

    if (!sim_cmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK", 3000))
        return false;

    snprintf(cmd, sizeof(cmd), "AT+HTTPDATA=%d,10000", strlen(data));

    sim_send(cmd);

    if (!sim_wait("DOWNLOAD", 5000))
        return false;

    uart_write_bytes(SIM_UART, data, strlen(data));

    if (!sim_wait("OK", 10000))
        return false;

    if (!sim_cmd("AT+HTTPACTION=1", "+HTTPACTION:", 30000))
        return false;

    sim_cmd("AT+HTTPTERM", "OK", 3000);

    return true;
}