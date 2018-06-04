#pragma once
#include <Windows.h>
class TimeMeasurer {
public:
    TimeMeasurer();

    void Start();

    /**
    * ���ش�Start()��ʼ��Now()֮�侭����ʱ�䣬�Ժ���(ms)Ϊ��λ
    */
    __int64 elapsedMillionSecondsSinceStart();

    /**
    * ���ش�Start()��ʼ��Now()֮�侭����ʱ�䣬��΢��(us)Ϊ��λ
    */
    __int64 elapsedMicroSecondsSinceStart();

    /**
    * ����func()����ִ�е�ʱ�䣬�Ժ��루ms)Ϊ��λ
    */
    __int64 elapsedTimeInMillionSeconds(void(*func)());

private:
    LARGE_INTEGER freq;
    LARGE_INTEGER start;
};
