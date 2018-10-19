#include "AVFrameManager.h"



//************************************
// Method:    ����AVFrameManager�ṹ�壬�����ṹ���е�����AVFrameList��
//            �Լ�subPicManager��subPicManager�µ�AVFrameList
// FullName:  create_AVFrameManager 
// Access:    public 
// Returns:   int -1ʧ�� 0 �ɹ�
// Qualifier:
// Parameter: AVFrameManager * AVFrameMag ��Ž���������ǰ������Ƶ�����ַ
// Parameter: int subpicNum ��ǰ�ڵ�����Tile����
//************************************
int create_AVFrameManager(AVFrameManager*& AVFrameMag, int subpicNum) {
    AVFrameMag = new struct AVFrameManager();
    if(! AVFrameMag) {
        return -1;
    }
    AVFrameMag->psourceAList = new struct AVFrameList();
    AVFrameMag->psourceVList = new struct AVFrameList();
    AVFrameMag->pProjectList = new struct AVFrameList();
    AVFrameMag->pSPM = new struct subPicManager();
    if(AVFrameMag->pProjectList == NULL || AVFrameMag->psourceAList == NULL 
        || AVFrameMag->psourceVList == NULL || AVFrameMag->pSPM == NULL) {
            return -1;
    }
    AVFrameMag->pSPM->subpicNum = subpicNum;
    return 0;
}

//************************************
// Method:    ����AVFrameNode�ڵ㣬������Ϊ�գ��Ա�ÿ���߳��Լ����
// FullName:  create_AVFrameNode
// Access:    public 
// Returns:   int -1ʧ�� 0�ɹ�
// Qualifier:
// Parameter: AVFrameNode * pNode �����Ľڵ�ṹ��
//************************************
int create_AVFrameNode(AVFrameNode*& pNode) {
    //pNode = new AVFrameNode();
    //if(pNode) {
    //    return 0;
    //} else {
    //    return -1;
    //}
    return -1;
}

//************************************
// Method:    ��ȡAVFrameList��present��ָ��Ľڵ�
// FullName:  get_AVFrameNode
// Access:    public 
// Returns:   int -1ʧ�� 0�ɹ�
// Qualifier:
// Parameter: AVFrameNode * pNode ��õ�AVFrame�ڵ�
// Parameter: AVFrameList * pList Ѱ�ҽڵ������
//************************************
int get_AVFrameNode(AVFrameNode*& pNode, AVFrameList *pList) {
    if(pList == NULL) {
        return -1;
    } 
    pNode = pList->present;
    if(pNode == NULL) {
        return -1;
    }else{
        pList->present = pNode->next;
    }
    return 0;
}

//************************************
// Method:    ��ָ����pList֡���������һ��AVframeNode�ڵ㣬�ڵ���used_flag��Ϊ0�������δ������ʹ��
// FullName:  add_AVFrameNode
// Access:    public 
// Returns:   int -1ʧ�� 0�ɹ�
// Qualifier:
// Parameter: AVFrameList * pList ָ������Ҫ����֡�ڵ��֡����
// Parameter: AVFrameNode * pNode �����ڵ�ָ��
// Parameter: HANDLE * mutex �����������߳�һ��ֻ����һ������
//************************************
int add_AVFrameNode(AVFrameList *pList,AVFrameNode *pNode,HANDLE &mutex) {
    //int result=WaitForSingleObject(mutex,INFINITE);//�����ʱ�������ӽڵ�
    //if (result!=0){
    //    printf("T Wait failed! %d %d\n",result,GetLastError());
    //}

    //ERROR_INVALID_HANDLE
    //     6 (0x6)
    //    The handle is invalid.
    WaitForSingleObject(mutex,INFINITE);
    if(pList == NULL) {
        return -1;
    }
    if (pList->present == NULL){
        pList->present = pNode;
    }
    if (pList->pVhead == NULL){
        pList->pVhead = pNode;
    }
    if (pList->pTail != NULL){
        pList->pTail->next = pNode;
    }
    pList->pTail = pNode;
    pNode->next = NULL;
    pNode->used_flag = 0;
    ReleaseMutex(mutex);//��һ���߳̿��Ի�ȡ��
    return 0;
}

//************************************
// Method:    ͨ��λ������ж�֡������֡�ڵ��Ƿ����ɾ������used_mask��Ӧλ��flagΪ1�����ɾ��
// FullName:  update_AVFrameList
// Access:    public 
// Returns:   int -1ʧ�� 0�ɹ�
// Qualifier:
// Parameter: AVFrameList * pList ɾ���ڵ�ǰ������ָ��
// Parameter: UINT8 used_mask λ���룬ѡ����Ҫ�ж�AVFrame�ڵ���used_mask��Ա��Ӧλ�Ƿ�����ɾ���ڵ�Ҫ��
// Parameter: HANDLE * mutex ɾ���ýڵ�ǰȷ���ýڵ�δ�������߳�ʹ��
//************************************
int update_AVFrameList( AVFrameList *pList,UINT8 used_mask,HANDLE &mutex )
{
   // WaitForSingleObject(mutex,INFINITE); �����߳��Ѿ��õ�����������Ҫ������
    if(pList == NULL) {
        return -1;
    }
    
    AVFrameNode *pNode = pList->pVhead;
    while(pNode != pList->present && pNode->used_flag) {
        pList->pVhead = pNode->next;
        delete pNode;
        pNode = pList->pVhead;
    }
    if (pList->pVhead == NULL)
        pList->pTail = NULL;
    //delete pNode;
   // ReleaseMutex(mutex);
    return 0;
}

//************************************
// Method:    �ͷ�AVFrameManager�е������ڴ�ռ�
// FullName:  delete_AVFrameManager
// Access:    public 
// Returns:   int -1ʧ�� 0�ɹ�
// Qualifier:
// Parameter: AVFrameManager * AVFrameMag �����ٵ�����ָ��
// Parameter: HANDLE * mutex ȷ����֡��������δ������ʹ��
//************************************
int delete_AVFrameManager(AVFrameManager*& AVFrameMag, HANDLE &mutex)
{
   
    WaitForSingleObject(mutex,INFINITE);
    delete AVFrameMag;
    AVFrameMag = NULL;
    ReleaseMutex(mutex);  
    return 0;
}


//TEST_CASE("AVFrameManager.", "[AVFrameManager]") {
//    AVFrameManager *manager = NULL;
//    SECTION("create_AVFrameManager") {
//        create_AVFrameManager(manager, 0);
//        REQUIRE(manager != NULL);
//        REQUIRE(manager->pProjectList != NULL);
//        REQUIRE(manager->pSPM != NULL);
//        REQUIRE(manager->psourceAList != NULL);
//    }
//    SECTION("add_AVFrameNode") {
//        create_AVFrameManager(manager, 0);
//        AVFrameNode *node = NULL;
//        create_AVFrameNode(node);
//        add_AVFrameNode(manager->pProjectList, node, NULL);
//        REQUIRE(node != NULL);
//    }
//    SECTION("get_AVFrameNode") {
//
//        create_AVFrameManager(manager, 0);
//        AVFrameNode *node = NULL;
//        create_AVFrameNode(node);
//        add_AVFrameNode(manager->pProjectList, node, NULL);
//        AVFrameNode *node2 = NULL;
//        get_AVFrameNode(node2, manager->pProjectList);
//        REQUIRE(node2 != NULL);
//    }
//    SECTION("delete_AVFrameManager") {
//        create_AVFrameManager(manager, 0);
//        REQUIRE(manager != NULL);
//        delete_AVFrameManager(manager, NULL);
//        REQUIRE(manager == NULL);
//    }
//}