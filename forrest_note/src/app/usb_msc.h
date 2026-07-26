#pragma once

// Call once from setup(), before USB.begin(), to register the MSC class.
// mediaPresent is initially false so the host sees no media at boot.
void usb_msc_init();

// Block until hold-REC: unmounts SD, connects MSC to host, then on exit
// disconnects MSC and re-mounts SD. Caller must call loadIndex() after.
void enterMscMode();
