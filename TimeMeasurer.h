#pragma once
#include <Windows.h>
class TimeMeasurer {
public:
    TimeMeasurer();

    void Start();

    /**
    * 返回从Start()开始到Now()之间经过的时间，以毫秒(ms)为单位
    */
    __int64 elapsedMillionSecondsSinceStart();

    /**
    * 返回从Start()开始到Now()之间经过的时间，以微秒(us)为单位
    */
    __int64 elapsedMicroSecondsSinceStart();

    /**
    * 返回func()函数执行的时间，以毫秒（ms)为单位
    */
    __int64 elapsedTimeInMillionSeconds(void(*func)());

private:
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
};
