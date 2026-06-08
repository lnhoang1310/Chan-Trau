#include <stdio.h>
#include "gps.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "GPS"

// #define GPS_MAX_SEND_LEN 600
// #define GPS_MAX_RECV_LEN 600
#define GPS_UART_NUM 2
#define GPS_UART_TX_PIN 17
#define GPS_UART_RX_PIN 16

static Data gps_data;

#define GPS_LINE_BUFFER_SIZE 128

static void gps_uart_handler_task(void *pvParameters)
{
    nmea_msg gpsx;

    uint8_t rx_char;
    char line_buf[GPS_LINE_BUFFER_SIZE];

    int index = 0;

    while (1)
    {
        int len = uart_read_bytes( GPS_UART_NUM, &rx_char, 1, pdMS_TO_TICKS(100));

        if (len > 0)
        {
            if (rx_char == '\r')
                continue;
            
            if (rx_char == '\n')
            {
                line_buf[index] = '\0';
                ESP_LOGI(TAG, "RAW: %s", line_buf);

                if (strstr(line_buf, "$GNRMC") != NULL)
                {
                    NMEA_GNRMC_Analysis(&gpsx, (uint8_t *)line_buf);

                    gps_data.latitude = (float)(gpsx.latitude / 100000.0f);
                    gps_data.longitude = (float)(gpsx.longitude / 100000.0f);
                    gps_data.N_S = (char)gpsx.nshemi;
                    gps_data.E_W = (char)gpsx.ewhemi;
                    ESP_LOGI(TAG, "Latitude: %f %c, Longitude: %f %c", gps_data.latitude, gps_data.N_S, gps_data.longitude, gps_data.E_W);
                }
                index = 0;
            }
            else
            {
                if (index < (GPS_LINE_BUFFER_SIZE - 1))
                {
                    line_buf[index++] = rx_char;
                }
                else
                {
                    index = 0;
                }
            }
        }
    }
}

uint8_t NMEA_Comma_Pos(uint8_t *buf, uint8_t cx)
{
    uint8_t *p = buf;
    while (cx)
    {
        if (*buf == '*' || *buf < ' ' || *buf > 'z')
            return 0XFF;
        if (*buf == ',')
            cx--;
        buf++;
    }
    return buf - p;
}

uint32_t NMEA_Pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
        result *= m;
    return result;
}

int NMEA_Str2num(uint8_t *buf, uint8_t *dx)
{
    uint8_t *p = buf;
    uint32_t ires = 0, fres = 0;
    uint8_t ilen = 0, flen = 0, i;
    uint8_t mask = 0;
    int res;
    while (1)
    {
        if (*p == '-')
        {
            mask |= 0X02;
            p++;
        }
        if (*p == ',' || (*p == '*'))
            break;
        if (*p == '.')
        {
            mask |= 0X01;
            p++;
        }
        else if (*p > '9' || (*p < '0'))
        {
            ilen = 0;
            flen = 0;
            break;
        }
        if (mask & 0X01)
            flen++;
        else
            ilen++;
        p++;
    }
    if (mask & 0X02)
        buf++;
    for (i = 0; i < ilen; i++)
    {
        ires += NMEA_Pow(10, ilen - 1 - i) * (buf[i] - '0');
    }
    if (flen > 5)
        flen = 5;
    *dx = flen;
    for (i = 0; i < flen; i++)
    {
        fres += NMEA_Pow(10, flen - 1 - i) * (buf[ilen + 1 + i] - '0');
    }
    res = ires * NMEA_Pow(10, flen) + fres;
    if (mask & 0X02)
        res = -res;
    return res;
}

void NMEA_GNRMC_Init(nmea_msg *gpsx)
{
    uart_config_t gps_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(GPS_UART_NUM, &gps_config);
    uart_set_pin(GPS_UART_NUM, GPS_UART_TX_PIN, GPS_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GPS_UART_NUM, 1024, 0, 0, NULL, 0);
    xTaskCreate(gps_uart_handler_task, "gps_uart_handler_task", 4096, NULL, 10, NULL);
}

void NMEA_GNRMC_Analysis(nmea_msg *gpsx, uint8_t *buf)
{
    uint8_t *p1, dx;
    uint8_t posx;
    uint32_t temp;
    float rs;
    p1 = (uint8_t *)strstr((const char *)buf, "$GNRMC");
    if (p1 == NULL)
    {
        return;
    }
    posx = NMEA_Comma_Pos(p1, 3);
    if (posx != 0XFF)
    {
        temp = NMEA_Str2num(p1 + posx, &dx);
        gpsx->latitude = temp / NMEA_Pow(10, dx + 2);
        rs = temp % NMEA_Pow(10, dx + 2);
        gpsx->latitude = gpsx->latitude * NMEA_Pow(10, 5) + (rs * NMEA_Pow(10, 5 - dx)) / 60;
    }

    posx = NMEA_Comma_Pos(p1, 4);
    if (posx != 0XFF)
        gpsx->nshemi = *(p1 + posx);
    posx = NMEA_Comma_Pos(p1, 5);
    if (posx != 0XFF)
    {
        temp = NMEA_Str2num(p1 + posx, &dx);
        gpsx->longitude = temp / NMEA_Pow(10, dx + 2);
        rs = temp % NMEA_Pow(10, dx + 2);
        gpsx->longitude = gpsx->longitude * NMEA_Pow(10, 5) + (rs * NMEA_Pow(10, 5 - dx)) / 60;
    }
    posx = NMEA_Comma_Pos(p1, 6);
    if (posx != 0XFF)
        gpsx->ewhemi = *(p1 + posx);
}
