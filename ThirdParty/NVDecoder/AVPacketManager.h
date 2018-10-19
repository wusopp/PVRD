#ifndef AVPACKETMANAGER_H
#define AVPACKETMANAGER_H
#include "prefix.h"


struct PacketNode {
    UINT8 *bitstream;
    int size;
    UINT64 PTS;
    UINT8 used_flag;
    struct PacketNode *next;
    PacketNode():size(0),PTS(0),used_flag(0) {
        bitstream = NULL;
        next = NULL;
    }
    ~PacketNode() {
        free(bitstream);
        bitstream = NULL;
    }
};

struct AVPktList {
    MEDIAType type;
    int streamID;
    int tileID;
    struct PacketNode *pVHead;
    struct PacketNode *pVTail;
    struct PacketNode *present;
    struct AVPktList *next;
    AVPktList():streamID(0),tileID(0) {
        type = MEDIA_NONE;
        pVHead = new PacketNode();
        pVTail = new PacketNode();
        present = new PacketNode();
        next = NULL;
    }
    ~AVPktList() {
        PacketNode *node = pVHead;
        PacketNode *ptr = node;
        while(node != NULL) {
            ptr = node->next;
            delete(node);
            node = node->next;
        }
        pVHead = NULL;
        pVTail = NULL;
        present = NULL;
    }
};

struct AVPacketManager {
    int StreamNum;
    struct AVPktList *pVHead;
    struct AVPktList *pVTail;
    AVPacketManager() :StreamNum(0) {
        pVHead = new AVPktList();
        pVTail = new AVPktList();
    }
    ~AVPacketManager() {
        delete pVHead;
        delete pVTail;
        pVHead = NULL;
        pVTail = NULL;
    }
};

int create_AVPacketManager(AVPacketManager*& AVPacketMag, int StreamNum);

int create_packetNode(PacketNode*& pNode);

int get_packetNode(PacketNode *pNode,AVPktList *pList);

int add_packetNode(AVPktList *pList,PacketNode *pNode,HANDLE &mutex);

int update_AVPktList(AVPktList *pList,HANDLE &mutex);

int delete_AVPacketManager(AVPacketManager *AVPacketMag,HANDLE &mutex);

#endif