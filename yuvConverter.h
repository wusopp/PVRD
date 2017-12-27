#pragma once
static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256];
static unsigned char clp[1024];   //for clip in CCIR601   

void init_yuv420p_table();
void yuv420p_to_rgb24(unsigned char* yuvbuffer, unsigned char* rgbbuffer, int width, int height);

