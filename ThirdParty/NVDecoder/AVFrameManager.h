#ifndef AVFRAMEMANAGER_H
#define AVFRAMEMANAGER_H
#include "prefix.h"
#include <iostream>
struct AVFrameNode {
    UINT8 *Yframe;
    UINT8 *Uframe;
    UINT8 *Vframe;
    int width;
    int height;
    UINT8 used_flag;

    //这8bit的数据按照如下的格式进行安排
    //备用-备用-备用-备用-码控完成flag-全局映射完成flag-Tile划分完成flag-编码完成flag
    UINT8 used_mask;

    struct AVFrameNode *next;
    AVFrameNode(int w,int h,int size):width(w),height(h),used_flag(0),used_mask(0){
        if(w!=0 && h!=0)//video
        {
            Yframe = new UINT8 [w*h*3/2];
            if(Yframe == NULL)
                std::cout<<"error"<<std::endl;
            Uframe = Yframe + w*h;
            Vframe = Uframe + w*h/4;
            next = NULL;
        }
        else if(size!=0)//audio
        {
            Yframe = new UINT8 [size];
            Uframe = NULL;
            Vframe = NULL;
        }
    }
    ~AVFrameNode() {
        delete Yframe;
        Yframe = NULL;
        Uframe = NULL;
        Vframe = NULL;
    }
};

struct AVFrameList {
    MEDIAType type;
    int tileID;
    struct AVFrameNode *pVhead;
    struct AVFrameNode *pTail;
    struct AVFrameNode *present; 
    struct AVFrameList *next;
    AVFrameList():tileID(0) {
        type = MEDIA_NONE;
        pVhead = NULL;
        pTail = pVhead;
        present = pVhead;
        //pTail->next = NULL;
        next = NULL;
    }
    ~AVFrameList() {
        AVFrameNode *node = pVhead;
        AVFrameNode *ptr = node;
        while (node != NULL) {
            ptr = node->next;
            delete (node);
            node = ptr;
        }
        pVhead = NULL;
        pTail = NULL;
        present = NULL;
    }
};


struct subPicManager {
    int subpicNum;
    struct AVFrameList *pVHead;
    struct AVFrameList *pVTail;
    subPicManager():subpicNum(0) {
        pVHead = new AVFrameList();
        pVTail = new AVFrameList();
    }
    ~subPicManager() {
        delete pVHead;
        delete pVTail;
        pVTail = NULL;
        pVHead = NULL;
    }
};

struct AVFrameManager {
    struct AVFrameList *psourceVList;
    struct AVFrameList *psourceAList;
    struct AVFrameList *pProjectList;
    struct subPicManager *pSPM;

    AVFrameManager() {
        psourceVList = new AVFrameList();
        psourceAList = new AVFrameList();
        pProjectList = new AVFrameList();
        pSPM = new subPicManager();
    }
    ~AVFrameManager() {
        delete pSPM;
        delete psourceAList;
        delete psourceVList;
        delete pProjectList;
        pSPM = NULL;
        psourceVList = NULL;
        psourceAList = NULL;
        pProjectList = NULL;
    }
};

int create_AVFrameManager(AVFrameManager*& AVFrameMag, int subpicNum);

int create_AVFrameNode(AVFrameNode*& pNode);

int get_AVFrameNode(AVFrameNode*& pNode, AVFrameList *pList);

int add_AVFrameNode(AVFrameList *pList,AVFrameNode *pNode,HANDLE &mutex);

int update_AVFrameList(AVFrameList *pList,UINT8 used_mask,HANDLE &mutex);

int delete_AVFrameManager(AVFrameManager*& AVFrameMag, HANDLE &mutex);
#endif