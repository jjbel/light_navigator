#include "camera.h"

void setup()
{
    // This is not necessary and has no effect for ATMEGA based Arduinos.
    // WAVGAT Nano has slower clock rate by default. We want to reset it to maximum speed
    CLKPR = 0x80; // enter clock rate change mode
    CLKPR = 0;    // set prescaler to 0. WAVGAT MCU has it 3 by default.

    initializeScreenAndCamera();
    pinMode(10, OUTPUT);
    pinMode(9, OUTPUT);
    // Serial.begin(115200);
}

void loop()
{
    processFrame();
    // Serial.println(String(millis()));
    // delay(300);
}
