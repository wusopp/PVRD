#include "readtoMemory.h"

int readtoMemory(const char *url,char* pBuf,int bufsize)
{
    /*CHAR szUrl[] = "http://localhost/HLS/s10.ts";
    CHAR szAgent[] = "";
    DWORD t = timeGetTime();*/
    HINTERNET hInternet1 = 
        InternetOpen(NULL,INTERNET_OPEN_TYPE_PRECONFIG,NULL,NULL,NULL);
  
    if (NULL == hInternet1)
     {
        InternetCloseHandle(hInternet1);
        return -1;
     }
    HINTERNET hInternet2 = 
        InternetOpenUrl(hInternet1,url,NULL,NULL,INTERNET_FLAG_NO_CACHE_WRITE,NULL);
      //printf("%d\n",timeGetTime()-t);
    if (NULL == hInternet2)
     {
        InternetCloseHandle(hInternet2);
        InternetCloseHandle(hInternet1);
        return -2;
     }

   // pBuf = (PBYTE)malloc(500*sizeof(TCHAR));
    if (NULL == pBuf)
     {
        InternetCloseHandle(hInternet2);
        InternetCloseHandle(hInternet1);
        return -3;
     }
    DWORD dwReadDataLength = NULL;
    BOOL bRet = TRUE;
    do 
    {
        bRet = InternetReadFile(hInternet2,pBuf,bufsize,&dwReadDataLength);

     } while (NULL != dwReadDataLength);

    
    return 0;
} 