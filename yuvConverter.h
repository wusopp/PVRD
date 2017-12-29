#pragma once
   //for clip in CCIR601   

void init_yuv420p_table();
void yuv420p_to_rgb24(unsigned char* yuvbuffer, unsigned char* rgbbuffer, int width, int height);

