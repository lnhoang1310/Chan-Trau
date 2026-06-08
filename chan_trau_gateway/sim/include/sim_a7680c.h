#ifndef SIM_A7680C_H
#define SIM_A7680C_H

#include <stdbool.h>

bool sim_init(void);

bool sim_network_connect(const char *apn);

bool sim_http_get(const char *url);

bool sim_http_post(
        const char *url,
        const char *data);

#endif