#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include "pico/stdlib.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO pin dla przekaźnika KY-019
#define RELAY_PIN 5

// Funkcje publiczne
void bsp_relay_init(void);
void bsp_relay_set_state(bool active);
bool bsp_relay_get_state(void);
void bsp_relay_toggle(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_RELAY_H