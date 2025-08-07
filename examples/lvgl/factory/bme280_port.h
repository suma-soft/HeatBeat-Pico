#ifndef BME280_PORT_H
#define BME280_PORT_H

#include "bme280/bme280.h"


// Inicjalizacja BME280 (magistrala + konfiguracja)
int8_t bme280_init_default(void);

// Globalna struktura urządzenia
extern struct bme280_dev bme;

// 🔧 Deklaracja funkcji do odczytu danych (to dodaj!)
int8_t bme280_read_data(struct bme280_data *comp_data);

#endif
