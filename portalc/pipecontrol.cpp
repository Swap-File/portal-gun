#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "pipecontrol.h"
#define NUM_PIPES          2
#define PARENT_WRITE_PIPE  0
#define PARENT_READ_PIPE   1



int video_send_state = 1;
int audio_send_state = 1;
int video_rec_state = 1;
int audio_rec_state = 1;

int video_send_state_request = 1;
int audio_send_state_request = 1;
int video_rec_state_request = 1;
int audio_rec_state_request = 1;

int video_send_state_request_previous = 1;
int audio_send_state_request_previous = 1;
int video_rec_state_request_previous = 1;
int audio_rec_state_request_previous = 1;

int video_send_update_time = 0;
int audio_send_update_time = 0;
int video_rec_update_time = 0;
int audio_rec_update_time = 0;

int camera_PID;
int audio_send_PID;

FILE *bash_fp;
FILE *cmus_remote_fp;
FILE *ahrs_fp;
FILE *pidof_fp;

void pipecontrol_cleanup(void){
	printf("KILLING OLD PROCESSES\n");
	system("sudo pkill cmus*");
	system("sudo pkill gst-launch*");
	system("sudo pkill raspivid");
	system("sudo pkill omxplayer");
	system("sudo pkill minimu9-ahrs");
	system("sudo pkill ahrs-visualizer");
	printf("FLUSHING IPTABLES RULES\n");
	system("/usr/bin/sudo iptables -F");
}

void pipecontrol_setup(void){

	
	pipecontrol_cleanup();
	
	printf("BASH_Control: LOADING\n");
	bash_fp = popen("bash", "w");
	//int bash_d_int = fileno(bash_fp);
	//fcntl(bash_d_int, F_SETFL, O_NONBLOCK);
	printf("BASH_Control: READY\n");
	
	//dont lock down loopback so we can display camera loopback during bootup
	iptables_set(1,1,1,1);
	
	printf("CMUS: LOADING\n");
	//cmus doesnt play nice, has to use screen
	system("screen -d -m cmus");
	printf("CMUS: READY\n");
	

	system("./launch.sh");

	//get pids here 
	char tempbuf[100];
	char tempbuf2[100];
	
	printf("PID Lookup: LOADING\n");
	pidof_fp = popen("pidof -s raspivid", "r");
	fgets(tempbuf, 100, pidof_fp);
	sscanf(tempbuf,"%d\n",&camera_PID);
	printf("Video_Send PID: %d\n",camera_PID);
	fclose(pidof_fp);
	
	printf("PID Lookup: LOADING\n");
	pidof_fp = popen("pidof -s gst-launch-0.10", "r");
	fgets(tempbuf2, 100, pidof_fp);
	sscanf(tempbuf2,"%d\n",&audio_send_PID);
	printf("Audio_Send PID: %d\n",audio_send_PID);
	fclose(pidof_fp);
	
	system("./launch2.sh");

	printf("AHRS_Control_Pipe: LOADING\n");
	ahrs_fp = fopen("/tmp/FIFO_PIPE", "w");
	int ahrs_fp_int = fileno(ahrs_fp);
	fcntl(ahrs_fp_int, F_SETFL, O_NONBLOCK);
	printf("AHRS_Control_Pipe: READY\n");
	
	
	printf("CMUS_Remote: LOADING\n");
	cmus_remote_fp = popen("cmus-remote", "w");
	//int cmus_remote_d_int = fileno(bash_fp);
	//fcntl(cmus_remote_d_int, F_SETFL, O_NONBLOCK);
	printf("CMUS_Remote: READY\n");
	
	//lock down all ports
	iptables_set(0,0,0,0);
	
	cmus_remote_play("/home/pi/physcannon/physcannon_charge1.wav"); 
	
	sleep(2);
	
	printf("Audio Fix: ModPRobe\n");
	fprintf(bash_fp, "sudo modprobe snd-bcm2835\n");
	fflush(bash_fp);	
}



void ahrs_command(int number){
	fprintf(ahrs_fp, "%d\n",number);
	fflush(ahrs_fp);
}		


void cmus_remote_play(const char *filename){
printf("player-play %s\n",filename);
	fprintf(cmus_remote_fp, "player-play %s\n",filename);
	fflush(cmus_remote_fp);
}

void omxplayer_start(int filename){
	fprintf(bash_fp, "screen -d -m  omxplayer /home/pi/movies/%i.mp4 --orientation 90\n",filename);
	fflush(bash_fp);	
}


void omxplayer_kill(){
	fprintf(bash_fp, "sudo pkill omxplayer\n");
	fflush(bash_fp);
}


void iptables_set(int video_send, int audio_send,int video_rec, int audio_rec){

	video_send_state_request = video_send;
	audio_send_state_request = audio_send;
	video_rec_state_request = video_rec;
	audio_rec_state_request = audio_rec;
}

void iptables_update(int *time){
	int changes = 0;

	if(video_send_state_request != video_send_state_request_previous&& *time-video_send_update_time > 1000){
		video_send_update_time = *time;
		 video_send_state_request_previous = video_send_state_request;
		changes++;
		if( video_send_state_request == 1){
			if(video_send_state == 0){
				video_send_state=1;
				kill(camera_PID,SIGUSR1);
				fprintf(bash_fp, "sudo iptables -D OUTPUT -p udp --destination-port 9000 -j DROP\n");			
			}
		}else{
			if(video_send_state == 1){
				video_send_state=0;
				kill(camera_PID,SIGUSR2);
				fprintf(bash_fp, "sudo iptables -A OUTPUT -p udp --destination-port 9000 -j DROP\n");
			}
		}
	}
	
	if(audio_send_state_request != audio_send_state_request_previous&& *time-audio_send_update_time > 1000){
		audio_send_update_time = *time;
		changes++;
		audio_send_state_request_previous =audio_send_state_request  ;
		if( audio_send_state_request == 1){
			if(audio_send_state == 0){
				audio_send_state=1;
				kill(audio_send_PID,SIGUSR1);
				fprintf(bash_fp, "sudo iptables -D OUTPUT -p udp --destination-port 5000 -j DROP\n");
			}
		}else{
			if(audio_send_state == 1){
				audio_send_state = 0;
				kill(audio_send_PID,SIGUSR2);
				fprintf(bash_fp, "sudo iptables -A OUTPUT -p udp --destination-port 5000 -j DROP\n");
			}		
		}
	}
	
	if(audio_rec_state_request != audio_rec_state_request_previous && *time-audio_rec_update_time > 1000){
		audio_rec_update_time = *time;
		changes++;
		 audio_rec_state_request_previous = audio_rec_state_request;
		if( audio_rec_state_request == 1){
			if (audio_rec_state == 0){
				audio_rec_state=1;
				fprintf(bash_fp, "sudo iptables -D INPUT -p udp --destination-port 5000 -j DROP\n");
			}
		}else{
			if (audio_rec_state ==1){
				audio_rec_state=0;
				fprintf(bash_fp, "sudo iptables -A INPUT -p udp --destination-port 5000 -j DROP\n");			
			}
		}
	}
	
	if (video_rec_state_request != video_rec_state_request_previous && *time-video_rec_update_time > 1000 ){
		video_rec_update_time = *time;
		changes++;
		video_rec_state_request_previous = video_rec_state_request;
		if( video_rec_state_request == 1){
			if (video_rec_state == 0){
				video_rec_state=1;
				fprintf(bash_fp, "sudo iptables -D INPUT -p udp --destination-port 9000 -j DROP\n");		
			}
		}else{
			if (video_rec_state == 1){
				video_rec_state=0;
				fprintf(bash_fp, "sudo iptables -A INPUT -p udp --destination-port 9000 -j DROP\n");
			}
		}
	}
	if (changes > 0){
		fflush(bash_fp);
	}
}


