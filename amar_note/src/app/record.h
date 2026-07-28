#pragma once
#include <stdbool.h>

extern volatile bool g_stopRecording;

bool record();
bool playWavFile(const char* path);
