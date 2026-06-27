#pragma once

// ELEGOO ESP-32 Super Starter Kit — pin layout (lesson 2.14)
#define PIN_OLED_SDA 21
#define PIN_OLED_SCL 22
#define PIN_LED_RAIN 4  // rain alert LED (anode → resistor → GPIO)

// Open-Meteo location — default: Tokyo
// Override in secrets.h with #define LATITUDE / #define LONGITUDE
#ifndef LATITUDE
#define LATITUDE 35.6812
#endif
#ifndef LONGITUDE
#define LONGITUDE 139.7671
#endif

// How often to refresh weather (milliseconds)
#define WEATHER_REFRESH_MS (3UL * 60UL * 60UL * 1000UL)  // 3 hours
