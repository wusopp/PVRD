#ifndef READTOMEMORY_H
#define READTOMEMORY_H

#include <stdio.h>
#include <windows.h>
#include <wininet.h>
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"Wininet.lib")

int readtoMemory(const char *url, char* pBuf,int bufsize);

#endif