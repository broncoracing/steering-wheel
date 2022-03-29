#include "leds.h"


Pixel hsv(uint8_t h, uint8_t s, uint8_t v) {
    Pixel rgb;
    rgb.i = 255;
    unsigned char region, remainder, p, q, t;

    if (s == 0) {
        rgb.r = v;
        rgb.g = v;
        rgb.b = v;
        return rgb;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            rgb.r = v;
            rgb.g = t;
            rgb.b = p;
            break;
        case 1:
            rgb.r = q;
            rgb.g = v;
            rgb.b = p;
            break;
        case 2:
            rgb.r = p;
            rgb.g = v;
            rgb.b = t;
            break;
        case 3:
            rgb.r = p;
            rgb.g = q;
            rgb.b = v;
            break;
        case 4:
            rgb.r = t;
            rgb.g = p;
            rgb.b = v;
            break;
        default:
            rgb.r = v;
            rgb.g = p;
            rgb.b = q;
            break;
    }

    return rgb;
}


LEDs::LEDs() :
        main_neopixels(MAIN_LED_PIN, MAIN_LED_COUNT, H0, L0, H1, L1),
        aux_neopixels(AUX_LED_PIN, AUX_LED_COUNT, H0, L0, H1, L1) {}

void LEDs::tachLeds(int rpm, float brightness) {
    if(rpm > FLASH_RPM) {
        Pixel color;
        if(fmod(state_timer, FLASH_TIME_S) > FLASH_TIME_S / 2.0f){
            color.r = 255 * brightness;
            color.g = 150 * brightness;
        }
        for (int i = 0; i < MAIN_LED_COUNT; i++) {
            pixels[i] = color;
        }

    } else {
        float num_lit_leds = (float)(rpm - MIN_RPM) / (float)(FLASH_RPM - MIN_RPM) * (float) MAIN_LED_COUNT;
        for (int i = 0; i < MAIN_LED_COUNT; i++) {
            float power = num_lit_leds - (float) (16 - i);
            if(power > 1.0f) {
                pixels[i] = hsv((uint8_t)(i * 4), 255, 255 * brightness);
            } else if (power > 0.0f) {
                pixels[i] = hsv((uint8_t)(i * 4), 255, 255 * brightness * power);
            } else {
                pixels[i] = Pixel();
            }
        }
    }
}

void LEDs::tachLedsRainbow(int rpm, float brightness, int hue_shift) {
    if(rpm > FLASH_RPM) {
        if(fmod(state_timer, FLASH_TIME_S) > FLASH_TIME_S / 2.0f){
            for (int i = 0; i < MAIN_LED_COUNT; i++) {
                pixels[i] = hsv((uint8_t)(i * 16 + hue_shift), 255, 255 * brightness);
            }
        } else {
            Pixel off;
            for (int i = 0; i < MAIN_LED_COUNT; i++) {
                pixels[i] = off;
            }
        }
    } else {
        float num_lit_leds = (float)(rpm - MIN_RPM) / (float)(FLASH_RPM - MIN_RPM) * (float) MAIN_LED_COUNT;
        for (int i = 0; i < MAIN_LED_COUNT; i++) {
            float power = num_lit_leds - (float) (16 - i);
            if(power > 1.0f) {
                pixels[i] = hsv((uint8_t)(i * 16 + hue_shift), 255, 255 * brightness);
            } else if (power > 0.0f) {
                pixels[i] = hsv((uint8_t)(i * 16 + hue_shift), 255, 255 * brightness * power);
            } else {
                pixels[i] = hsv((uint8_t)(i * 16 + hue_shift), 255, 255 * brightness * 0.1);
            }
        }
    }
}

void LEDs::updateLEDs(SteeringWheelState *state) {
    state_timer += (1.0f / LED_RATE);

    {
        ScopedLock <Mutex> lock(state->mutex);
        switch(led_state) {
            case StartupAnim:
            {
                int hue_shift = (int)(state_timer * 400.0f);
                float brightness = min(state_timer, 1.0f);
                brightness = brightness * brightness;
                uint8_t pixel_value = (uint8_t)(255.0f * brightness * state->brightness);
                for (int i = 0; i < MAIN_LED_COUNT; i++) {
                    pixels[i] = hsv((uint8_t)(i * 16 + hue_shift), 255, pixel_value);
                }
                for (int i = 0; i < AUX_LED_COUNT; i++) {
                    aux_pixels[i] = hsv((uint8_t)(i * 32 + 16 + hue_shift), 255, pixel_value);
                }
                if(state_timer > 3.0) {
                    updateState(StartupSweep);
                }
            }
                break;
            case StartupSweep:
            {
                tachLeds(state_timer * FLASH_RPM * 0.6, state->brightness);
                for (int i = 0; i < AUX_LED_COUNT; i++) {
                    aux_pixels[i] = Pixel();
                }
            }
                break;
            case Tachometer:
            {
                if(state->canReceived){
                    tachLeds(state->rpm, state->brightness);
                } else {
                    float brightness = sin(state_timer * 6);
                    brightness = brightness * brightness;
                    uint8_t pixel_value = (uint8_t)(255.0f * brightness * state->brightness);
                    Pixel pixel_color{0, pixel_value, pixel_value, 0};
                    for (int i = 0; i < MAIN_LED_COUNT; i++) {
                        pixels[i] = pixel_color;
                    }
                }
                auxLeds(state);
            }
                break;
            case RainbowRoad:
            {
                int hue_shift = (int)(state_timer * 400.0f);
                if(state->canReceived){
                    tachLedsRainbow(state->rpm, state->brightness, hue_shift);
                } else {
                    float brightness = sin(state_timer * 6);
                    brightness = brightness * brightness;
                    uint8_t pixel_value = (uint8_t)(255.0f * brightness * state->brightness);
                    Pixel pixel_color = hsv(hue_shift, 255, pixel_value);
                    for (int i = 0; i < MAIN_LED_COUNT; i++) {
                        pixels[i] = pixel_color;
                    }
                }

                auxLeds(state);
            }
                break;
            case ButtonTest:
            {
                uint8_t pixel_value = (uint8_t)(255.0f * state->brightness);
                Pixel on_color{0, pixel_value, pixel_value, 0};
                for (int i = 0; i < MAIN_LED_COUNT; i++) {
                    pixels[i] = Pixel();
                }
                // Turn on leds when their respective button pin is on
                for(int i = 0; i < 6; i++){
                    if(state->buttons[i]){
                        pixels[i] = on_color;
                    }
                }

                for (int i = 0; i < AUX_LED_COUNT; i++) {
                    aux_pixels[i] = Pixel();
                }
            }
                break;
        }

    }

    main_neopixels.write(reinterpret_cast<int *>(pixels));
    aux_neopixels.write(reinterpret_cast<int *>(aux_pixels));
}

void LEDs::auxLeds(SteeringWheelState *state) {
    // TODO Aux LED stuff
}
