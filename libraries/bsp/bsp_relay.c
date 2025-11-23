#include "bsp_relay.h"
#include <stdio.h>

static bool relay_state = false;

void bsp_relay_init(void) {
    // Inicjalizuj GPIO jako output
    gpio_init(RELAY_PIN);
    gpio_set_dir(RELAY_PIN, GPIO_OUT);
    
    // Domyślnie wyłączony (stan niski)
    gpio_put(RELAY_PIN, false);
    relay_state = false;
    
    printf("[RELAY] Initialized on GPIO %d, state: OFF\n", RELAY_PIN);
}

void bsp_relay_set_state(bool active) {
    gpio_put(RELAY_PIN, active);
    relay_state = active;
    
    printf("[RELAY] State changed to: %s\n", active ? "ON (zawór otwarty)" : "OFF (zawór zamknięty)");
}

bool bsp_relay_get_state(void) {
    return relay_state;
}

void bsp_relay_toggle(void) {
    bsp_relay_set_state(!relay_state);
}