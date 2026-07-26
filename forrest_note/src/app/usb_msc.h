#pragma once

// enterMscMode() — full-screen USB mass storage takeover.
// Unmounts SD_MMC, presents it as a USB drive, shows the MSC screen,
// and blocks until the user holds BTN_REC. Re-mounts SD_MMC on exit.
void enterMscMode();
