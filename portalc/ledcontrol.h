
#ifndef _LEDCONTROL_H 
#define _LEDCONTROL_H
 
void ledcontrol_setup(void);
float ledcontrol_update(int color_temp,int width_temp,int width_speed_temp,int overlay_temp, int total_offset);
#endif