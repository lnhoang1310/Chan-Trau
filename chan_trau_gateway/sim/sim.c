#include "sim_a7680c.h"

#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#define SIM_UART       UART_NUM_1

#define SIM_TX         GPIO_NUM_17
#define SIM_RX         GPIO_NUM_16
#define SIM_PWRKEY     GPIO_NUM_4

static const char *TAG = "SIM";

/*=========================================================
 * UART AT COMMAND
 *========================================================*/
static void sim_send(const char *cmd)
{
    uart_write_bytes(
        SIM_UART,
        cmd,
        strlen(cmd));

    uart_write_bytes(
        SIM_UART,
        "\r\n",
        2);

    ESP_LOGI(TAG,"TX: %s",cmd);
}

/*=========================================================
 * WAIT RESPONSE
 *========================================================*/
static bool sim_wait(
        const char *expect,
        uint32_t timeout_ms)
{
    uint8_t buf[1024];

    int len =
        uart_read_bytes(
            SIM_UART,
            buf,
            sizeof(buf)-1,
            pdMS_TO_TICKS(timeout_ms));

    if(len <= 0)
    {
        ESP_LOGE(TAG,"Timeout");
        return false;
    }

    buf[len] = 0;

    ESP_LOGI(TAG,"RX:\n%s",(char*)buf);

    return strstr(
            (char*)buf,
            expect) != NULL;
}

/*=========================================================
 * SEND COMMAND + CHECK
 *========================================================*/
static bool sim_cmd(
        const char *cmd,
        const char *expect,
        uint32_t timeout_ms)
{
    sim_send(cmd);

    return sim_wait(
            expect,
            timeout_ms);
}

/*=========================================================
 * POWER ON
 *========================================================*/
static void sim_power_on(void)
{
    gpio_set_level(
            SIM_PWRKEY,
            0);

    vTaskDelay(
        pdMS_TO_TICKS(100));

    gpio_set_level(
            SIM_PWRKEY,
            1);

    vTaskDelay(
        pdMS_TO_TICKS(1500));

    gpio_set_level(
            SIM_PWRKEY,
            0);

    vTaskDelay(
        pdMS_TO_TICKS(8000));
}

/*=========================================================
 * INIT
 *========================================================*/
bool sim_init(void)
{
    uart_config_t cfg =
    {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_driver_install(
            SIM_UART,
            4096,
            4096,
            0,
            NULL,
            0);

    uart_param_config(
            SIM_UART,
            &cfg);

    uart_set_pin(
            SIM_UART,
            SIM_TX,
            SIM_RX,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE);

    gpio_reset_pin(
            SIM_PWRKEY);

    gpio_set_direction(
            SIM_PWRKEY,
            GPIO_MODE_OUTPUT);

    sim_power_on();

    if(!sim_cmd(
            "AT",
            "OK",
            3000))
        return false;

    sim_cmd(
        "ATE0",
        "OK",
        3000);

    if(!sim_cmd(
            "AT+CPIN?",
            "READY",
            5000))
        return false;

    sim_cmd(
        "AT+CSQ",
        "OK",
        3000);

    ESP_LOGI(TAG,"SIM Ready");

    return true;
}

/*=========================================================
 * NETWORK CONNECT
 *========================================================*/
bool sim_network_connect(
        const char *apn)
{
    char cmd[128];

    if(!sim_cmd(
            "AT+CGATT=1",
            "OK",
            10000))
    {
        ESP_LOGE(TAG,"CGATT Failed");
        return false;
    }

    sprintf(
        cmd,
        "AT+CGDCONT=1,\"IP\",\"%s\"",
        apn);

    if(!sim_cmd(
            cmd,
            "OK",
            5000))
    {
        ESP_LOGE(TAG,"CGDCONT Failed");
        return false;
    }

    if(!sim_cmd(
            "AT+CGACT=1,1",
            "OK",
            15000))
    {
        ESP_LOGE(TAG,"CGACT Failed");
        return false;
    }

    sim_cmd(
        "AT+CGPADDR=1",
        "OK",
        3000);

    ESP_LOGI(TAG,"Network Connected");

    return true;
}

/*=========================================================
 * HTTP GET
 *========================================================*/
bool sim_http_get(
        const char *url)
{
    char cmd[512];

    sim_send("AT+HTTPTERM");
    vTaskDelay(pdMS_TO_TICKS(200));

    if(!sim_cmd(
            "AT+HTTPINIT",
            "OK",
            5000))
        return false;

    sprintf(
        cmd,
        "AT+HTTPPARA=\"URL\",\"%s\"",
        url);

    if(!sim_cmd(
            cmd,
            "OK",
            5000))
        return false;

    if(!sim_cmd(
            "AT+HTTPACTION=0",
            "+HTTPACTION:",
            30000))
        return false;

    sim_send("AT+HTTPREAD");

    sim_wait(
        "OK",
        10000);

    sim_send("AT+HTTPTERM");

    sim_wait(
        "OK",
        3000);

    return true;
}

/*=========================================================
 * HTTP POST
 *========================================================*/
bool sim_http_post(
        const char *url,
        const char *data)
{
    char cmd[512];

    sim_send("AT+HTTPTERM");
    vTaskDelay(pdMS_TO_TICKS(200));

    if(!sim_cmd(
            "AT+HTTPINIT",
            "OK",
            3000))
        return false;

    sprintf(
        cmd,
        "AT+HTTPPARA=\"URL\",\"%s\"",
        url);

    if(!sim_cmd(
            cmd,
            "OK",
            3000))
        return false;

    if(!sim_cmd(
            "AT+HTTPPARA=\"CONTENT\",\"application/json\"",
            "OK",
            3000))
        return false;

    sprintf(
        cmd,
        "AT+HTTPDATA=%d,10000",
        (int)strlen(data));

    sim_send(cmd);

    if(!sim_wait(
            "DOWNLOAD",
            3000))
        return false;

    uart_write_bytes(
            SIM_UART,
            data,
            strlen(data));

    if(!sim_wait(
            "OK",
            10000))
        return false;

    if(!sim_cmd(
            "AT+HTTPACTION=1",
            "+HTTPACTION:",
            30000))
        return false;

    sim_send("AT+HTTPTERM");

    sim_wait(
        "OK",
        3000);

    return true;
}