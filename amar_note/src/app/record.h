#pragma once
#include <stdbool.h>

extern volatile bool g_stopRecording;
extern bool g_lastRecTooShort;   // set true when a recording was discarded for being too short

bool record();
bool playWavFile(const char* path);
