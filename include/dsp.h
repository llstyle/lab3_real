// =====================================================================
//  Обработка сигнала из L2: проверка достоверности, медиана, скользящее среднее
//  (значения в миллиметрах, целые)
// =====================================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "types.h"

struct MedianFilter
{
    int32_t buf[MEDIAN_WINDOW];
    uint8_t idx;
    uint8_t count;
};

struct MovingAverage
{
    int32_t buf[AVG_WINDOW];
    int32_t sum;
    uint8_t idx;
    uint8_t count;
};

bool checkSample(float cm);                  // эхо есть и в рабочем диапазоне 2…400 см
void medianInit(MedianFilter &f);
int32_t medianFilter(MedianFilter &f, int32_t x);
void avgInit(MovingAverage &f);
float movingAverage(MovingAverage &f, int32_t x);
