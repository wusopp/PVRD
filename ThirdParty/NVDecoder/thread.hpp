//
// Sample Code
//-----------------------------------------------------------------------------
//#include "thread.h"
//
//class CTestThread
//{
//public:
//    void thread_func(LPVOID arg) {};
//};
//
//CTestThread test_thread;
//
//ThreadCallback cb = BIND_MEM_CB(&CTestThread::thread_func, &test_thread);
//ThreadPtr thread_ptr = new CThread(cb); 
//thread_ptr->start();
//
//thread_ptr->join();
//delete thread_ptr;
//-----------------------------------------------------------------------------

#pragma once

#include <windows.h>
#include <process.h>
#include "callback.hpp"

namespace Common
{
    typedef util::Callback<void (LPVOID)> ThreadCallback; 

    // 定义线程最小休眠时间
    enum {MIN_SLEEP_TIME = 100};

    //
    // 线程类
    //
    class CThread
    {
    public:
        explicit CThread(ThreadCallback cb, BOOL auto_delete = FALSE) 
            : _cb(cb)
            , _auto_delete(auto_delete)
            , _handle(NULL)
            , _argument(NULL)
        {
        }

        ~CThread(void)
        {
            if (_handle)
            {
                ::CloseHandle(_handle);
                _handle = NULL;
            }
        }

        // 等待线程结束
        void join(void)
        {
            if (is_alive())
            {
                WaitForSingleObject(_handle, INFINITE);
            }
        }

        // 线程任务处理
        void run(void)
        {
            if (_cb)
            {
                _cb(_argument);
            }
        }

        // 启动线程
        BOOL start(LPVOID argument = NULL)
        {
            _argument = argument;
            _handle = (HANDLE)_beginthreadex(NULL, 0, CThread::ThreadFunc, this, 0, NULL);
            return (_handle != NULL);
        }

        // 判断是否自动删除
        BOOL is_auto_delete(void)
        {
            return _auto_delete;
        }

        // 判断线程是否在运行
        BOOL is_alive(void)
        {
            if (_handle)
            {
                DWORD ret = ::WaitForSingleObject(_handle, 0);
                return (ret != WAIT_OBJECT_0);
            }

            return FALSE;
        }

        // 中止线程运行(Q254956不安全方式)
        //
        // 中止一个线程并不是一个好主意。停止或完全的退出一个线程反而更安全。MSDN文档在TerminateThread API中说到TerminateThread是一个很危险的函数，
        // 只能在很极端的情况下才可以使用。只有当你确切知道目标线程是做什么的，并且你可以控制所有目标线程在结束时可能会运行到的代码，这时候才可以调用TerminateThread。
        // 例如，TerminateThread会产生如下问题：
        // ·如果目标线程有一个临界区（critical section），这临界区将不会被释放。
        // ·如果目标线程在堆里分配了内存，堆锁（heap lock）将不会被释放。
        // ·如果目标线程在结束时调用了某些kernel32，这样在线程的过程中的kernel32状态会不一致。
        // ·如果目标线程正操作着一个共享库的全局状态，这个库的状态会被破坏，影响到其他的这个库的使用者。
        //
        // 也许结束一个线程最可靠的方法就是确定这个线程不休眠无限期的等待下去。一个支持线程被要求停止，它必须定期的检查看它是否被要求停止或者如果它在休眠的话看它是否能够被唤醒。
        // 支持这两个情况最简单的的方法就是使用同步对象，这样应用程序的线程和问题中的线程能互相沟通。当应用程序希望关闭线程时，只要设置这个同步对象，等待线程退出。
        // 之后，线程把这个同步对象作为它周期性监测的一部分，定期检查，
        // 或者如果这个同步对象的状态改变的话，唤醒它，还可以执行清空和退出线程函数。
        //
        // 相关英文论坛的帖子：
        // http://social.msdn.microsoft.com/Forums/en-US/vclanguage/thread/67fe193a-ba67-454c-b19d-81ee4168818c
        void terminate(DWORD exit_code = 0)
        {
            if (is_alive())
            {
                ::TerminateThread(_handle, exit_code);
            }
        }

    private:
        CThread(const CThread&);
        void operator=(const CThread&);

        // 线程运行入口
        static unsigned __stdcall ThreadFunc(void * lpParameter)
        {
            CThread* p = (CThread *)lpParameter;
            p->run();

            if (p->is_auto_delete())
            {
                delete p;
            }

            return 0;
        }

    private:
        ThreadCallback _cb; // 线程回调函数指针
        BOOL _auto_delete;  // 自动删除标志
        HANDLE _handle;     // 线程句柄
        LPVOID _argument;   // 线程运行参数
    };

    typedef CThread * ThreadPtr;
}
