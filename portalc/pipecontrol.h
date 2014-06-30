
#ifndef _PIPECONTROL_H 
#define _PIPECONTROL_H

void pipecontrol_setup(void);
void pipecontrol_cleanup(void);

void iptables_set(int video_send, int audio_send,int video_rec, int audio_rec);
void iptables_update(int *time);

void omxplayer_start(int filename);
void omxplayer_kill();

void cmus_remote_play(const char *filename);//filename full path string

void ahrs_command(int number); //numerical command for what state to be in

//8 and 9 is closed
//1 open a closed blue portal 
//0 open a open blue portal

//6 open a open orange portal
//7 open a closed orange portal




#endif