/* mbed Microcontroller Library
 * Copyright (c) 2022 ARM Limited
 * SPDX-License-Identifier: Apache-2.0
 */
#include "mbed.h"

#include "main.h"
#include "leds.h"
#include "can-ids/CAN_IDS.h"


#include "SWO/SWO.h"
#include "WS2812/WS2812.h"
#include "DebounceIn/DebounceIn.h"


// Used to print debug info over SWO instead of UART
SWO_Channel swo("channel");

// CAN bus for reading RPM and sending out shift/DRS messages
CAN can(PA_11, PA_12, CAN_BAUD);

// EventQueue is used to run events from interrupts in a non-interrupt context
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;

// Periodically calls an ISR to update LEDs
Ticker led_loop;

// LED object, gets updated in a loop
LEDs leds;

// Steering wheel state
SteeringWheelState state;

// Buttons
DebounceIn upshift_btn(UPSHIFT_PIN, PullUp);
DebounceIn downshift_btn(DOWNSHIFT_PIN, PullUp);
DebounceIn drs_btn(DRS_PIN, PullUp);
DebounceIn settings_btn(SETTINGS_PIN, PullUp);
DebounceIn aux1_btn(AUX1_PIN, PullUp);
DebounceIn aux2_btn(AUX2_PIN, PullUp);

bool isTestMode = false;

void steering_wheel_received(CANMessage received) {
    int rpm = (received.data[ECU_RPM_BYTE + 1] << 8) + received.data[ECU_RPM_BYTE];
    {
        ScopedLock <Mutex> lock(state.mutex);
        state.rpm = rpm;
        state.canReceived = true;
    }

    printf("RPM: %d\n", rpm);
}

// Handle received can frames
void can_received() {
    CANMessage received;
    // Read newly received CAN messages until there aren't any
    while(can.read(received)){

        if(received.id == ECU1_ID) { // TODO Fix CAN ID
            printf("received RPM CAN frame\n");
            steering_wheel_received(received);
        }
    }
}


// ISR function which schedules handling of received CAN frames
void can_received_handler() {
    queue.call(can_received);
}

// Update LEDS in non-ISR context
void update_leds() {
    leds.updateLEDs(&state);
}

// ISR handler for LED loop
void led_loop_handler(){
    queue.call(update_leds);
}

// ISR callback functions for button test mode
void upshift_test_cb_r(){ state.buttons[0] = false;  }
void upshift_test_cb_f(){ state.buttons[0] = true; }
void downshift_test_cb_r(){ state.buttons[1] = false;  }
void downshift_test_cb_f(){ state.buttons[1] = true; }
void drs_test_cb_r(){ state.buttons[2] = false;  }
void drs_test_cb_f(){ state.buttons[2] = true; }
void settings_test_cb_r(){ state.buttons[3] = false;  }
void settings_test_cb_f(){ state.buttons[3] = true; }
void aux1_test_cb_r(){ state.buttons[4] = false;  }
void aux1_test_cb_f(){ state.buttons[4] = true; }
void aux2_test_cb_r(){ state.buttons[5] = false;  }
void aux2_test_cb_f(){ state.buttons[5] = true; }

// Enter into button test mode and configure IRS callbacks for buttons
void test_mode() {
    printf("Entering button test mode.\n");
    isTestMode = true;
    leds.updateState(ButtonTest);
    upshift_btn.rise(callback(upshift_test_cb_r));
    upshift_btn.fall(callback(upshift_test_cb_f));

    downshift_btn.rise(callback(downshift_test_cb_r));
    downshift_btn.fall(callback(downshift_test_cb_f));

    drs_btn.rise(callback(drs_test_cb_r));
    drs_btn.fall(callback(drs_test_cb_f));

    settings_btn.rise(callback(settings_test_cb_r));
    settings_btn.fall(callback(settings_test_cb_f));

    aux1_btn.rise(callback(aux1_test_cb_r));
    aux1_btn.fall(callback(aux1_test_cb_f));

    aux2_btn.rise(callback(aux2_test_cb_r));
    aux2_btn.fall(callback(aux2_test_cb_f));
}

// Goes into button test mode
void test_mode_enable_handler() {
    queue.call(test_mode);
}


void send_can_message(bool drs, bool upshift, bool downshift) {
    unsigned char buffer[3] = {0, 0, 0};
    if(drs){
        buffer[0] = 1;
    }

    if(upshift){
        buffer[1] = 1;
    }

    if(downshift){
        buffer[2] = 1;
    }
    CANMessage msg(STEERING_WHEEL_ID, buffer, 3);

    if(can.write(msg)){
        return;
    } else{
        printf("Failed to send CAN message :(\n");
    }
}


// Send upshift/downshift messages over CAN
void upshift() {
    {
        ScopedLock <Mutex> lock(state.mutex); // Make sure the state isn't being otherwise modified or read while updating

        send_can_message(state.drsEngaged, true, false);
    }

    printf("Upshift message sent\n");
}
void downshift() {
    {
        ScopedLock <Mutex> lock(state.mutex); // Make sure the state isn't being otherwise modified or read while updating
        if(state.rpm > MONEY_SHIFT_THRESHOLD) {
            printf("Downshift canceled because RPM was above threshold. RPM: %d, threshold: %d\n", state.rpm, MONEY_SHIFT_THRESHOLD);
            return;
        }

        send_can_message(state.drsEngaged, false, true);
    }

    printf("Downshift message sent\n");
}

// enable/disable DRS and send a can message to update
void drs_on() {
    {
        ScopedLock <Mutex> lock(state.mutex); // Make sure the state isn't being otherwise modified or read while updating
        state.drsEngaged = true;
    }

    send_can_message(true, false, false);

    printf("DRS engaged\n");
}
void drs_off() {
    {
        ScopedLock <Mutex> lock(state.mutex); // Make sure the state isn't being otherwise modified or read while updating
        state.drsEngaged = false;
    }
    send_can_message(false, false, false);

    printf("DRS disengaged\n");
}

// Detects when the settings button is held for 3 seconds
Timeout change_brightness;

void dim_leds() {
    {
        ScopedLock <Mutex> lock(
                state.mutex); // Make sure the state isn't being otherwise modified or read while updating
        if (state.brightness == DEFAULT_BRIGHTNESS) {
            state.brightness = LOW_BRIGTNESS;
        } else {
            state.brightness = DEFAULT_BRIGHTNESS;
        }
    }

    printf("LED brightness setting changed.\n");
}

void dim_handler() { queue.call(dim_leds); }

void settings_pressed() {
    change_brightness.attach(dim_handler, 3.0f);
}

void settings_released() {
    change_brightness.detach();
}



// ISR handler functions for button presses during normal operation
void upshift_handler() { queue.call(upshift); }
void downshift_handler() { queue.call(downshift); }
void upshift_off_handler() { /*queue.call(upshift_off);*/ }
void downshift_off_handler() { /*queue.call(downshift_off);*/ }

void drs_on_handler() { queue.call(drs_on); }
void drs_off_handler() { queue.call(drs_off); }

void settings_pressed_handler() { queue.call(settings_pressed); } // TODO Dimming
void settings_released_handler() { queue.call(settings_released); }

int main()
{
#ifdef DEBUG_SWO
    // Print debug info over SWO
    swo.claim();
    printf("Claimed SWO output\n");
#endif //DEBUG_SWO

    // Calculate loop wait time for main thread loop
    auto loop_time = (1000ms / LED_RATE);

    // set up initial state
    state.brightness = DEFAULT_BRIGHTNESS;
    state.drsEngaged = false;
    state.rpm = 0;

    // Start event queue thread
    printf("Starting event queue thread...\n");
    t.start(callback(&queue, &EventQueue::dispatch_forever));
    printf("Event queue thread running.\n");

    // Enable CAN callback
    printf("Starting CAN Listener...\n");
    can.attach(can_received_handler);
    printf("CAN Listener running\n");

    // Start LED update loop
    printf("Starting LED callback...\n");
    led_loop.attach(led_loop_handler, loop_time);
    printf("LED callback running, playing startup animation.\n");


    // Listen for settings btn press to enable button test mode
    settings_btn.rise(callback(test_mode_enable_handler));
    ThisThread::sleep_for(STARTUP_ANIMATION_TIME);

    // Run test mode if it was enabled
    while(isTestMode){
        ThisThread::sleep_for(50ms);
        // Exit button test mode when upshift and downshift pressed simultaneously
        if(!upshift_btn.read() && !downshift_btn.read()){
            isTestMode = false;
        }
    }

    // Disable listening for settings button
    settings_btn.rise(nullptr);

    // Enable button callbacks for normal operation
    upshift_btn.fall(callback(upshift_handler));
    downshift_btn.fall(callback(downshift_handler));
    upshift_btn.rise(callback(upshift_off_handler));
    downshift_btn.rise(callback(downshift_off_handler));

    drs_btn.fall(callback(drs_on_handler));
    drs_btn.rise(callback(drs_off_handler));

    settings_btn.fall(callback(settings_pressed_handler));
    settings_btn.rise(callback(settings_released_handler));

    // Enter into normal tachometer operation
    printf("Startup animation finished.\n");
    leds.updateState(Tachometer);

    while(true){
	    sleep();
    }
}
