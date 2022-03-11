#ifndef LEDS_H
#define LEDS_H

#include "mbed.h"
#include "WS2812/WS2812.h"
#include "main.h"

#define LED_RATE 60 // TODO: Change to 144 fps for better gaming experience


// Stores pixel data readable by neopixel library
struct Pixel {
    uint8_t b, g, r, i;
};

// Create a pixel from hue, saturation, and value
Pixel hsv(uint8_t h, uint8_t s, uint8_t v);

enum LEDState {
    StartupAnim, StartupSweep,
    Tachometer,
    ButtonTest,
};

class LEDs {
private:
    WS2812 main_neopixels;
    Pixel pixels[MAIN_LED_COUNT];

    WS2812 aux_neopixels;
    Pixel aux_pixels[AUX_LED_COUNT];

    float state_timer = 0.0f;

    LEDState led_state;

    void auxLeds(SteeringWheelState *state);
    void tachLeds(int rpm, float brightness);
public:
    LEDs();
    void updateLEDs(SteeringWheelState *state);
    void updateState(LEDState newState) {
        state_timer = 0.0f;
        led_state = newState;
    }
};

#endif //LEDS_H
