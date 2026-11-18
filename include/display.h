// =====================================================================
//  Встроенный OLED Heltec: статус устройства
//  Вызывается только из задачи коммуникации (раз в секунду).
// =====================================================================
#pragma once
#include <Arduino.h>
#include "types.h"

bool displayBegin();
void displayShow(const SharedState &s, bool linkUp, bool linkTest, uint32_t evtWaiting);
