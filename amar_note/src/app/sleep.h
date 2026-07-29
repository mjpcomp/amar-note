#pragma once

void resetActivity();
void enterUltraSleep();

// Call once per loop() tick from STATE_IDLE:
void checkBatteryWarning();   // samples ADC, shows/clears low-bat overlay
void checkAutoSleep();        // triggers enterUltraSleep() after ULTRA_SLEEP_MS of inactivity
