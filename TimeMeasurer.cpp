#include "TimeMeasurer.h"
TimeMeasurer::TimeMeasurer() {
    QueryPerformanceFrequency(&freq);
}

void TimeMeasurer::Start() {
    QueryPerformanceCounter(&start);
}

__int64 TimeMeasurer::elapsedMillionSecondsSinceStart() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
}

__int64 TimeMeasurer::elapsedMicroSecondsSinceStart() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return ((now.QuadPart - start.QuadPart) * 1000000 / freq.QuadPart);
}

__int64 TimeMeasurer::elapsedTimeInMillionSeconds(void(*func)()){
    QueryPerformanceCounter(&start);
    func();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (((now.QuadPart - start.QuadPart) * 1000) / freq.QuadPart);
}