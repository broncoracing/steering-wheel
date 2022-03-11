
#ifndef MAIN_H
#define MAIN_H

// Button pins
#define UPSHIFT_PIN PB_5
#define DOWNSHIFT_PIN PB_6
#define DRS_PIN PB_7
#define SETTINGS_PIN PB_8
#define AUX1_PIN PB_0
#define AUX2_PIN PB_1


// LED Brightness settings
#define DEFAULT_BRIGHTNESS 1.0f
#define LOW_BRIGTNESS 0.10f

// TODO Reasonable values
#define MIN_RPM 5800
#define FLASH_RPM 13800 // Above this RPM the tachometer will flash
#define FLASH_TIME_S 0.15 // On time for the flashes

#define MONEY_SHIFT_THRESHOLD 10200


#define MAIN_LED_PIN PA_7
#define MAIN_LED_COUNT 16
#define AUX_LED_PIN PB_15 // TODO figure out which pin
#define AUX_LED_COUNT 4

#define STARTUP_ANIMATION_TIME 5s

// LED Timings
#define H0 3
#define L0 8
#define H1 8
#define L1 15

// CAN ID for RPM
//TODO Put this into CAN_IDs repo and choose the actual ID
#define ECU_RPM_ID 0x10
#define ECU_RPM_BYTE 0


// Debug info over SWO instead of mbed's default for the target.
#define DEBUG_SWO


struct SteeringWheelState {
    Mutex mutex;        // Mutex keeps the state from being modified while it's being used
    bool canReceived;   // Has a CAN message for RPM been received?
    bool drsEngaged;    // Whether DRS is engaged
    int rpm;            // Engine RPM for tachometer
    float brightness;   // Current brightness setting
    bool buttons[6];    // State of the six buttons (For button test mode only)
};


#endif //MAIN_H
