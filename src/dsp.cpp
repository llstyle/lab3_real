// =====================================================================
//  Обработка сигнала: медиана и скользящее среднее из L2 без изменений
// =====================================================================
#include "dsp.h"

// Проверка достоверности: −1 (нет эха) и значения вне рабочего
// диапазона датчика физически недостоверны.
bool checkSample(float cm)
{
    return cm >= VALID_MIN_CM && cm <= VALID_MAX_CM;
}

void medianInit(MedianFilter &f)
{
    f.idx = 0;
    f.count = 0;
    for (int i = 0; i < MEDIAN_WINDOW; ++i)
        f.buf[i] = 0;
}

int32_t medianFilter(MedianFilter &f, int32_t x)
{
    f.buf[f.idx] = x;
    f.idx = (f.idx + 1) % MEDIAN_WINDOW;
    if (f.count < MEDIAN_WINDOW)
        f.count++;

    int32_t tmp[MEDIAN_WINDOW];
    for (int i = 0; i < f.count; ++i)
        tmp[i] = f.buf[i];
    for (int i = 1; i < f.count; ++i)
    {
        int32_t key = tmp[i];
        int j = i - 1;
        while (j >= 0 && tmp[j] > key)
        {
            tmp[j + 1] = tmp[j];
            --j;
        }
        tmp[j + 1] = key;
    }
    return tmp[f.count / 2];
}

void avgInit(MovingAverage &f)
{
    f.sum = 0;
    f.idx = 0;
    f.count = 0;
    for (int i = 0; i < AVG_WINDOW; ++i)
        f.buf[i] = 0;
}

float movingAverage(MovingAverage &f, int32_t x)
{
    f.sum -= f.buf[f.idx];
    f.buf[f.idx] = x;
    f.sum += x;
    f.idx = (f.idx + 1) % AVG_WINDOW;
    if (f.count < AVG_WINDOW)
        f.count++;
    return (float)f.sum / (float)f.count;
}

