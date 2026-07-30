#pragma once

// Nekogotchi — a small offline virtual-pet cat.
// Entered from the main menu (STATE_TAMAGOTCHI).

void petEnter();            // called once when STATE_TAMAGOTCHI is entered: load, age, draw
void petLoop();             // called every loop() iteration while state == STATE_TAMAGOTCHI
void petRedraw();           // redraw current view (used e.g. after a battery warning)

// Touch entry points — call from handleTouch() when state == STATE_TAMAGOTCHI.
//   petTouchAction(sel)  — tap on one of the 4 action pills (0=Feed,1=Play,2=Pet,3=Stats)
//   petTouchBack()       — tap on the back strip (top ~22 px)
void petTouchAction(int sel);
void petTouchBack();
