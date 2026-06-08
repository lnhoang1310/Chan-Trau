#include "blynk.h"
#include "sim_a7680c.h"

#include <stdio.h>
#include <string.h>

static char g_token[64];

bool blynk_begin(const char *token)
{
    if(token == NULL)
        return false;

    memset(g_token,0,sizeof(g_token));

    strncpy(
        g_token,
        token,
        sizeof(g_token)-1);

    return true;
}

bool blynk_virtual_write_int(
        uint8_t pin,
        int value)
{
    char url[256];

    sprintf(url,
            "https://blynk.cloud/external/api/update?token=%s&V%d=%d",
            g_token,
            pin,
            value);

    return sim_http_get(url);
}

bool blynk_virtual_write_float(
        uint8_t pin,
        float value)
{
    char url[256];

    sprintf(url,
            "https://blynk.cloud/external/api/update?token=%s&V%d=%.2f",
            g_token,
            pin,
            value);

    return sim_http_get(url);
}

bool blynk_virtual_write_string(
        uint8_t pin,
        const char *value)
{
    char url[256];

    sprintf(url,
            "https://blynk.cloud/external/api/update?token=%s&V%d=%s",
            g_token,
            pin,
            value);

    return sim_http_get(url);
}

bool blynk_location(
        float latitude,
        float longitude)
{
    char url[256];

    sprintf(url,
            "https://blynk.cloud/external/api/update?token=%s&V0=%.6f,%.6f",
            g_token,
            latitude,
            longitude);

    return sim_http_get(url);
}