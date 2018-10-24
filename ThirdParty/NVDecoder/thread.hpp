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

    // �����߳���С����ʱ��
    enum {MIN_SLEEP_TIME = 100};

    //
    // �߳���
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

        // �ȴ��߳̽���
        void join(void)
        {
            if (is_alive())
            {
                WaitForSingleObject(_handle, INFINITE);
            }
        }

        // �߳�������
        void run(void)
        {
            if (_cb)
            {
                _cb(_argument);
            }
        }

        // �����߳�
        BOOL start(LPVOID argument = NULL)
        {
            _argument = argument;
            _handle = (HANDLE)_beginthreadex(NULL, 0, CThread::ThreadFunc, this, 0, NULL);
            return (_handle != NULL);
        }

        // �ж��Ƿ��Զ�ɾ��
        BOOL is_auto_delete(void)
        {
            return _auto_delete;
        }

        // �ж��߳��Ƿ�������
        BOOL is_alive(void)
        {
            if (_handle)
            {
                DWORD ret = ::WaitForSingleObject(_handle, 0);
                return (ret != WAIT_OBJECT_0);
            }

            return FALSE;
        }

        // ��ֹ�߳�����(Q254956����ȫ��ʽ)
        //
        // ��ֹһ���̲߳�����һ�������⡣ֹͣ����ȫ���˳�һ���̷߳�������ȫ��MSDN�ĵ���TerminateThread API��˵��TerminateThread��һ����Σ�յĺ�����
        // ֻ���ںܼ��˵�����²ſ���ʹ�á�ֻ�е���ȷ��֪��Ŀ���߳�����ʲô�ģ���������Կ�������Ŀ���߳��ڽ���ʱ���ܻ����е��Ĵ��룬��ʱ��ſ��Ե���TerminateThread��
        // ���磬TerminateThread������������⣺
        // �����Ŀ���߳���һ���ٽ�����critical section�������ٽ��������ᱻ�ͷš�
        // �����Ŀ���߳��ڶ���������ڴ棬������heap lock�������ᱻ�ͷš�
        // �����Ŀ���߳��ڽ���ʱ������ĳЩkernel32���������̵߳Ĺ����е�kernel32״̬�᲻һ�¡�
        // �����Ŀ���߳���������һ��������ȫ��״̬��������״̬�ᱻ�ƻ���Ӱ�쵽������������ʹ���ߡ�
        //
        // Ҳ�����һ���߳���ɿ��ķ�������ȷ������̲߳����������ڵĵȴ���ȥ��һ��֧���̱߳�Ҫ��ֹͣ�������붨�ڵļ�鿴���Ƿ�Ҫ��ֹͣ��������������ߵĻ������Ƿ��ܹ������ѡ�
        // ֧�������������򵥵ĵķ�������ʹ��ͬ����������Ӧ�ó�����̺߳������е��߳��ܻ��๵ͨ����Ӧ�ó���ϣ���ر��߳�ʱ��ֻҪ�������ͬ�����󣬵ȴ��߳��˳���
        // ֮���̰߳����ͬ��������Ϊ�������Լ���һ���֣����ڼ�飬
        // ����������ͬ�������״̬�ı�Ļ�����������������ִ����պ��˳��̺߳�����
        //
        // ���Ӣ����̳�����ӣ�
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

        // �߳��������
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
        ThreadCallback _cb; // �̻߳ص�����ָ��
        BOOL _auto_delete;  // �Զ�ɾ����־
        HANDLE _handle;     // �߳̾��
        LPVOID _argument;   // �߳����в���
    };

    typedef CThread * ThreadPtr;
}
