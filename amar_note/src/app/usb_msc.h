#pragma once

// Check at the very top of setup() — before any other init.
// If the RTC boot flag is set, this function runs the full MSC session
// (shows screen, mounts SD as USB drive, blocks until hold-REC, reboots
// back to normal) and never returns.
void usb_msc_check_boot_flag();

// Called from the menu to request a reboot into MSC mode.
// Sets the RTC flag then calls ESP.restart() — does not return.
void enterMscMode();
