#include "udpcontrol.h"
#include "ledcontrol.h"
#include "pipecontrol.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <signal.h>

void     INThandler(int);

// Use GPIO Pin 17, which is Pin 0 for wiringPi library
#define BLUE_LED 2
#define ORANGE_LED 3
#define BLUE_BUTTON_PIN 7
#define ORANGE_BUTTON_PIN 0
#define PWM_LED 1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define MAX_BUF 100
#define MAX_STATIONS 100
#define STATION_EXPIRE 1000 //expire a station in 1 second
#define LONG_PRESS_TIME 1000 //how long is a long press?
#define BUTTON_ACK_BLINK 100 //how long to blink button to signify detecting a press

//used internal to the ISRS
volatile int OrangePressTime = 0;
volatile int BluePressTime = 0;
volatile int supress_blue = 0;
volatile int supress_orange = 0;

//used by main loop to change state machine
volatile int blue_short = 0;
volatile int blue_long = 0;
volatile int orange_short = 0;
volatile int orange_long = 0;
volatile int both_long_orange = 0;
volatile int both_long_blue = 0;

//globally read to check status of buttons
volatile int blue = 0;
volatile int orange = 0;

int blue_led_previous = 0;
int orange_led_previous = 0;

int video_length[12] = {24467,16767,12067,82000,30434,22900,19067,70000,94000,53167,184000,140000} ;
int current_video = 1;
void    INThandler(int);

void SetLEDs(int * time,int * state) {

	int bluetemp = 0;
	int orangetemp = 0;
	
	if(blue == 0 && orange == 1){ //if blue is pressed
		if ((*time - BluePressTime < BUTTON_ACK_BLINK) || ((*time - BluePressTime < (BUTTON_ACK_BLINK + LONG_PRESS_TIME)) && (*time - BluePressTime > LONG_PRESS_TIME))){
			bluetemp = 0;
		}else{
			bluetemp = 1;
		}
	}
	else if ( orange == 0 && blue == 1) {  //if orange is pressed
		if((*time - OrangePressTime < BUTTON_ACK_BLINK) || ((*time - OrangePressTime < (BUTTON_ACK_BLINK + LONG_PRESS_TIME)) && (*time - OrangePressTime > LONG_PRESS_TIME))){
			orangetemp = 0;
		}else{
			orangetemp = 1;
		}
	}
	else if ( orange == 0 && blue == 0) { // if both are pressed
		int OldestPress = MIN(OrangePressTime,BluePressTime); //find what was pressed first and use that
		if((*time - OldestPress < BUTTON_ACK_BLINK) || ((*time - OldestPress < (BUTTON_ACK_BLINK + LONG_PRESS_TIME)) && (*time - OldestPress > LONG_PRESS_TIME))){
			bluetemp = 0;
			orangetemp = 0;
		}else{
			bluetemp = 1;
			orangetemp = 1;
		}
	}
	else if ( orange == 1 && blue == 1) { // if neither is pressed
		bluetemp = 1;
		orangetemp = 1;
	}
	
	//blank the button not being used
	if (*state > 0){
	bluetemp = 0;
	}else if(*state < 0){
	 orangetemp = 0;
	}
	
	if (blue_led_previous != bluetemp){
		blue_led_previous = bluetemp;
		if(bluetemp == 1){
			digitalWrite (BLUE_LED,1); //turn on
		}else{
			digitalWrite (BLUE_LED,0); //turn off
		}
	}
	if (orange_led_previous != orangetemp){
		orange_led_previous = orangetemp;
		if(orangetemp == 1){
			digitalWrite (ORANGE_LED,1); //turn on
		}else{
			digitalWrite (ORANGE_LED,0); //turn off
		}
	}
}
// -------------------------------------------------------------------------
// myInterrupt:  called every time an event occurs
void OrangeInterrupt(void) {
	blue = digitalRead (BLUE_BUTTON_PIN);
	orange = digitalRead (ORANGE_BUTTON_PIN);
	//orange being let go
	if (orange == 1){
		if (supress_orange != 1){
			if (millis() - OrangePressTime > LONG_PRESS_TIME){
				if (blue == 0){
					if (BluePressTime > OrangePressTime){
						both_long_orange = 1; //Both being let go, ORANGE was first pressed	
					}else{
						both_long_blue = 1;  //Both being let go, Blue was first pressed	
					}
					supress_blue = 1; //ignore the other button coming up, don't generate a long press for it
				}else{
					orange_long = 1; //Orange being let go, Long press	
				}
			}else{
				if (blue == 0){
					//printf("BOTH being let go SHORT\n");
					supress_blue = 1;
				}else{
					orange_short = 1; //Orange being let go, Short press		
				}
			}
		}
		supress_orange = 0;  //reset orange supression
	}else{
		OrangePressTime = millis(); //orange button going down, record time of event
	}
}

void BlueInterrupt(void) {
	blue = digitalRead (BLUE_BUTTON_PIN);
	orange = digitalRead (ORANGE_BUTTON_PIN);
	//blue being let go
	if (blue == 1){
		if (supress_blue != 1){
			if (millis() - BluePressTime > LONG_PRESS_TIME){
				if (orange == 0){
					if (BluePressTime > OrangePressTime){
						both_long_orange = 1; //Both being let go, ORANGE was first pressed	
					}else{
						both_long_blue = 1;  //Both being let go, Blue was first pressed	
					}
					supress_orange = 1;  //ignore the other button coming up, don't generate a long press for it
				}else{
					blue_long = 1; //Blue being let go, Long press	
				}
			}else{
				if (orange == 0){
					supress_orange = 1; //ignore the other button coming up, don't generate a long press for it
				}else{
					blue_short = 1; //Blue being let go, Short press	
				}
			}
		}
		supress_blue = 0; //reset blue supression
	}else{
		BluePressTime = millis(); //Blue button going down, record time of event
	}
}



// -------------------------------------------------------------------------
// main
int main(void) {

	signal(SIGINT, INThandler);
	pipecontrol_setup();
	udpcontrol_setup();
	ledcontrol_setup();	
	
	wiringPiSetup();
	pinMode(BLUE_BUTTON_PIN,INPUT );
	pinMode(ORANGE_BUTTON_PIN,INPUT );
	pinMode(BLUE_LED,OUTPUT );
	pinMode(ORANGE_LED,OUTPUT );
	pinMode(PWM_LED,PWM_OUTPUT );
	
	struct station {
		int source;
		int state;
		int offset;
		int time;
	};
	
	int known_stations = 0; //how many stations we have right now
	struct station station_data[MAX_STATIONS];
	
	// sets up the wiringPi library
	if (wiringPiSetup () < 0) {
		fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno));
		return 1;
	}

	// set Pin 17/0 generate an interrupt on high-to-low transitions
	// and attach myInterrupt() to the interrupt
	if ( wiringPiISR (ORANGE_BUTTON_PIN, INT_EDGE_BOTH , &OrangeInterrupt) < 0 ) {
		fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
		return 1;
	}

	if ( wiringPiISR (BLUE_BUTTON_PIN, INT_EDGE_BOTH , &BlueInterrupt) < 0 ) {
		fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno));
		return 1;
	}

	int omxplayer_video_length = 0;
	int omxplayer_start_time = 0;
	int omxplayer_running = 0;
	
	int remote_state = 0;
	int self_state = 0;
	int local_state = 0;
	int connection_initiator = 0;
	
	int udp_send_time = 0;
	
	int time_this_cycle = 0;
	int fps_counter = 0;
	int fps = 0;
	
	
	// display counter value every second.
	while ( 1 ) {
		
		delay(20);

		time_this_cycle = millis();
		
		fps++;
		if (millis() - fps_counter > 1000){
			printf("FPS: %d\n",fps);
			fps = 0;
			fps_counter = millis();
		}
		
		
		
		//STAND ALONE STATE MACHINE CHANGES HERE
		
		int local_state_previous = local_state;
		int self_state_previous = self_state;
		
		if( orange_long || blue_long){
			orange_long = 0; //set as handled
			blue_long = 0; //set as handled
			local_state = 0; //reset local state
			connection_initiator = 0; //reset initiator
			self_state = 0; //reset self state
			iptables_set(0,0,0,0);
		}
		
		else if (orange_short){
			orange_short=0; //set as handled
			
			if(local_state == 0 && self_state == 0){
				local_state=1;
			}else if(local_state == 1){
				local_state=2;
				connection_initiator=1; 
			}else if(local_state == 2 && connection_initiator == 1){
				local_state=3;
			}else if(local_state == 4){
				local_state=5;
			}else if(local_state == 5){
				local_state=4;
			}else if(local_state == -4){ //swap places
				local_state=4;
			}else if(local_state == 2 && connection_initiator == 0){  //answer an incoming call immediately and open portal on button press
				local_state=4;  
			}else if(self_state > 0 && self_state < 4){
				self_state++;
			}			
		}
		
		else if (blue_short){
			blue_short=0; //set as handled
			
			if (local_state ==0 && self_state == 0){
				local_state = -1;
			}else if (local_state ==-1){
				local_state =-2;
				connection_initiator = 1;
			}else if (local_state ==-2 && connection_initiator == 1){
				local_state = -3;
			}else if (local_state ==-2 && connection_initiator == 0){
				local_state = -4;
			}else if (local_state ==-3 && connection_initiator == 0){
				local_state = -4; //connection established
			}else if(self_state < 0 && self_state > -4){
				self_state--;
			}
		}
		
		else if (both_long_orange){
			both_long_orange=0; //set as handled
			
			if ((local_state == 0 && self_state == 0) || local_state == 1||local_state == 2||local_state == 3  ){
				self_state = local_state +1;
				local_state_previous = local_state = 0;  //avoid transition changes
			}else if(self_state ==1 ||  self_state ==2 || self_state ==3){
				self_state++;
			}else if(self_state == -3 || self_state == -4 || self_state == -5 || self_state == 4  || self_state == 5){
				self_state=3;
			}			
		}
		
		else if (both_long_blue){
			both_long_blue=0; //set as handled
			
			if ((local_state == 0 && self_state == 0)|| local_state == -1 || local_state == -2 ||local_state == -3){
				self_state =local_state-1;  //do this outside of MAX macro
				self_state = MAX(self_state,-3); //blue needs to clamp to -3 for visual consistency since we dont have a blue portal in local state -3 mode
				
				local_state_previous = local_state = 0;  //avoid transition changes
			}else if(self_state ==-1 || self_state ==-2 || self_state ==-3){
				self_state--;
			}else if(self_state == 3 || self_state == 4  || self_state == 5 || self_state == -4  || self_state == -5){
				self_state= -3;
			}
		}
		
		
		
		//BUTTON BLINK CODE HERE
		if (local_state != 0){
		SetLEDs(&time_this_cycle,&local_state);
		}else{
		SetLEDs(&time_this_cycle,&self_state);
		}
		
		//NETWORK CODE HERE
		int temp_source;
		int temp_state;
		int temp_offset;
		while (udp_receive_state(&temp_source,&temp_state,&temp_offset) > 0 && millis() - time_this_cycle < 20){ //dont spend more than 20ms dumping the buffer, flood protection
			int matching_station_index = 0;
			while (matching_station_index < MAX_STATIONS && matching_station_index < known_stations){
				if (temp_source == station_data[matching_station_index].source){
					break;
				}
				matching_station_index++;
			}
			//if we are adding a station increment
			if(matching_station_index < MAX_STATIONS){
				if (matching_station_index == known_stations){
					known_stations++;
					printf ("New Station: %d\n", matching_station_index); 	
				}
				station_data[matching_station_index].source = temp_source;
				station_data[matching_station_index].state = temp_state;
				station_data[matching_station_index].offset = temp_offset;		
				station_data[matching_station_index].time = time_this_cycle;					
			}
		}

		//check for expired stations
		int matching_station_index = 0;
		while (matching_station_index < MAX_STATIONS && matching_station_index < known_stations){
			if (time_this_cycle - station_data[matching_station_index].time > STATION_EXPIRE){ //1 second expire time
				if (known_stations == 1){ //this should never happen, the self station should never expire, but covered just in case
					known_stations--;
				}else{
					station_data[matching_station_index].source = station_data[known_stations-1].source;
					station_data[matching_station_index].state = station_data[known_stations-1].state;
					station_data[matching_station_index].offset = station_data[known_stations-1].offset;		
					station_data[matching_station_index].time = station_data[known_stations-1].time;	
					known_stations--;
					matching_station_index--; //check the station we just moved
				}
			}
			matching_station_index++;
		}
		
		
		int remote_state_minimum = 0;
		int remote_state_maximum = 0;
		int total_time_offset = time_this_cycle; //time offset to pull the effects to
		
		for (int i = 0; i < known_stations; i++){
		
			total_time_offset = total_time_offset + station_data[i].offset; //increment led_index data for a new cycle
			remote_state_minimum = MIN(remote_state_minimum,station_data[i].state);
			remote_state_maximum = MAX(remote_state_maximum,station_data[i].state);
			//target_effect_offset = MAX(target_effect_offset,station_data[i].effect_offset);
		}

		const int effect_resolution = 400;
		const int breathing_rate = 2000;
		total_time_offset=  int((float)((total_time_offset / (known_stations + 1))% (breathing_rate)) * ((float)effect_resolution)/((float)(breathing_rate)));
	
	
		//DECIDE ON A NETWORK STATE FROM ALL STATIONS
		int remote_state_previous = remote_state;
		remote_state = 0;
		
		//if someone is dialling in ring all guns, first one to press amber gets to broadcast.
		if (remote_state_minimum < 0){
			remote_state = remote_state_minimum;
		}
		//if someone is dialling out override and ring all guns
		
		if (remote_state_maximum > 0){
			remote_state = remote_state_maximum;
		}
		
		//NETWORK STATE CAUSED TRANSITIONS HERE
		
		
		if (self_state == 0){
			if ((remote_state_previous >= 2 || remote_state_previous <= -2)  && remote_state == 0){
				local_state = 0;
			}
			
			if (remote_state_previous != -2 && remote_state == -2 && connection_initiator == 0){
				local_state = 2;
			}
			
			if (remote_state_previous != 2 && remote_state == 2 && connection_initiator == 0){
				local_state = -2;
			}
			
			if (remote_state_previous != 3 && remote_state == 3 && connection_initiator == 0){
				local_state = -3;
			}
			
			if (remote_state_previous != -3 && remote_state == -3 && connection_initiator == 0){
				local_state = 2;
			}
			
			//if (remote_state_previous != -3 && remote_state == -3 && connection_initiator == 1){
			//	local_state = 4;
			//	connection_initiator = 0;
			//}
			
			if (remote_state_previous != -4 && remote_state == -4 && connection_initiator == 1){
				local_state = 4;
				connection_initiator = 0;
			}
			
			if (remote_state_previous != 4 && remote_state == 4 && connection_initiator == 1){
				local_state = -4;
				connection_initiator = 0;
			}
			
			if (remote_state_previous == -4 && remote_state >= 4 ){
				local_state = -4;
				connection_initiator = 0;
			}
		}else{
			//code to pull out of self state
			if ((remote_state_previous != remote_state )&& (remote_state <= -2)){
				self_state = 0;
				if (self_state <= -3 || self_state>=3){
					local_state = 4;
				}else {
					local_state = 2;
				}
			}
		}
		
		
		int color1 = -1;
		//set color from state data		
		if (local_state > 0 || self_state > 0){
			color1 = 20;
		}else if(local_state < 0 || self_state < 0){
			color1 = 240;
		}
		
		int width_request = 20;
		//set width	
		if(local_state == 1 || self_state == -1 ||self_state == 1 || self_state == -1 ){
			width_request = 10;
		}else if(local_state == -1 ){
			width_request = 1;	
		}else if(local_state == -2 ){
			width_request = 5;	
		}else if(local_state == -3 ){
			width_request = 10;	
		}
		
		int width_speed = 200;
		//set width speed
		if (local_state <= -4 ||  local_state>= 4 || self_state <= -4 ||  self_state>= 4 ){
			width_speed = 0;
		}
		
		int shutdown_effect = 0;
		//shutdown_effect
		if (local_state == 0 && self_state == 0){
			shutdown_effect = 1;
		}
		
		
		int ahrs_number = 9;
		//ahrs effects
		
		// for networked modes
		if (self_state == 0){
			if (local_state ==3) {
				ahrs_number = 7;
			}else if (local_state >= 4) {
				ahrs_number = 6;
			}		
		}
		// for self modes
		if (local_state == 0){
			if (self_state ==3 || self_state ==4){
				ahrs_number = 7;
			}else if (self_state == -3 || self_state == -4){
				ahrs_number =  1;
			}else if (self_state < -4){
				ahrs_number = 0;
			}else if (self_state > 4){
				ahrs_number = 6;
			}		
		}

		
		
		//LOCAL STATES
		if ((local_state_previous != 0 || self_state_previous != 0) && (local_state == 0 && self_state == 0)){
			cmus_remote_play("/home/pi/portalgun/portal_close1.wav");
			iptables_set(0,0,0,0); //default close down
		}
		
		//on entering state 1
		if ((local_state_previous != local_state) && (local_state == 1)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge1.wav");
		}
		//on entering state -1
		if ((local_state_previous != -1)&& (local_state == -1)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge1.wav");			
		}
		//on entering state 2
		if ((local_state_previous !=2 ) &&  (local_state == 2)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge2.wav");		
			iptables_set(0,0,1,0); //prep to receive
		}
		//on entering state -2 from -1
		if ((local_state_previous !=-2 ) &&  (local_state == -2)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge2.wav");
			iptables_set(1,1,0,0); //prep to send
		}	
		if ((local_state_previous !=-3 ) &&  (local_state == -3)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge3.wav");
			iptables_set(1,1,0,0); //prep to send
		}	
		//on entering state 3 from 2
		if ((local_state_previous == 2 ) && ( local_state ==3)){	
			cmus_remote_play("/home/pi/portalgun/portalgun_shoot_blue1.wav");
			iptables_set(0,0,1,0); //prep to receive
		}
		//on quick swap to rec
		if ((local_state_previous < 4 )&& (local_state == 4)){
			cmus_remote_play("/home/pi/portalgun/portal_open2.wav");
			iptables_set(0,0,1,0);
		}
		//on quick swap to transmit
		if ((local_state_previous >= 4 )&& (local_state <= -4)){
			cmus_remote_play("/home/pi/portalgun/portal_fizzle2.wav");
			iptables_set(1,1,0,0);
		}			
		// on enter state 5 from 4 enable audio
		if ((local_state_previous == 4) && (local_state == 5)){
			iptables_set(0,0,1,1);
		}
		// on enter state 4 from 5 disable audio
		if ((local_state_previous == 5 )&&( local_state == 4)){
			iptables_set(0,0,1,0);
		}
		
		//SELF STATES
		if ((self_state_previous != self_state) && (self_state == 1 || self_state == -1)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge1.wav");
		}
		//on entering state 2 or -2
		if ((self_state_previous != self_state) && (self_state == 2 || self_state == -2)){
			cmus_remote_play("/home/pi/physcannon/physcannon_charge2.wav");				
		}
		
		//on entering state 3 or -3 from 0
		if ((self_state_previous < 3 && self_state_previous > -3  ) && (self_state == 3 || self_state == -3)){
			cmus_remote_play("/home/pi/portalgun/portalgun_shoot_blue1.wav");
		}

		
		//on quick swap 
		if (( self_state_previous >= 3 && self_state == -3) || (self_state_previous <= -3 && self_state == 3)){
			cmus_remote_play("/home/pi/portalgun/portal_open2.wav");		
			if (omxplayer_running >0){
				omxplayer_kill();
				omxplayer_running = 0;
			}
		}
		
		
		//on quick swap to transmit
		if ((self_state_previous != -4 && self_state == -4) || (self_state_previous != 4  && self_state == 4) ){
			//preload omx
			if (omxplayer_running == 0){
				printf("OMXPLAYER: Loading...\n");
				
				//replace with random video choosing code
				
				omxplayer_start(current_video);
			
				omxplayer_video_length = video_length[current_video-1];
				
				current_video++;
				if (current_video > 12){
				current_video = 1;
				}
				
				omxplayer_running = 1;
				omxplayer_start_time = time_this_cycle;
			}
			
		}
		
		//omxplayer fade in
		if (omxplayer_running == 1 && (time_this_cycle - omxplayer_start_time >  2600)){  //Set fade in time here, this is how long it takes OMX to load in ms
			printf("OMXPLAYER: Fade In\n");
			cmus_remote_play("/home/pi/portalgun/portal_open1.wav");
			omxplayer_running++;
			if( self_state == 4){
				self_state=5;
			}else if( self_state == -4){
				self_state=-5;
			}
		}
		
		//omxplayer fade out
		if (omxplayer_running == 2 && ( time_this_cycle - omxplayer_start_time >  omxplayer_video_length + 1500)){
			printf("OMXPLAYER: Fade Out\n");
			if (self_state == 5){
				self_state = 3;
				ahrs_command(7);  // force sending command right now because we are killing omxplayer
			}else if (self_state == -5){
				self_state = -3;
				ahrs_command(1); // force sending command right now because we are killing omxplayer
			}
			omxplayer_kill();
			omxplayer_running = 0;
			cmus_remote_play("/home/pi/portalgun/portal_close1.wav");
		}
		
		//omxplayer kill switch
		if ((self_state != 4 && self_state != -4 && self_state != 5 && self_state != -5 ) && omxplayer_running > 0){
			omxplayer_kill();
			omxplayer_running = 0;
			printf("OMXPLAYER: Emergency Kill switch\n");
		}

		iptables_update(&time_this_cycle);  //update iptables, time is given to prevent flooding
		
		//LED SUBPROCCESS CODE HERE
		
		
		float buttonbrightness = ledcontrol_update(color1,width_request,width_speed,shutdown_effect, total_time_offset);
		
		if (known_stations > 0){
		
		pwmWrite(PWM_LED,int( 1024 * buttonbrightness));
		}else{
		pwmWrite(PWM_LED,int( 64));
		}

		//debug
		if ((local_state_previous != local_state)){
			printf( "Local Change State: %d %d\n",local_state_previous,local_state);
		}
		if ((remote_state_previous != remote_state)){
			printf( "Remote Change State: %d %d\n",remote_state_previous,remote_state);
		}
		if ((self_state_previous != self_state)){
			printf( "Self Change State: %d %d\n",self_state_previous,self_state);
		}

		//UDP SEND CODE
		if (time_this_cycle - udp_send_time > 100){  //only broadcast state once every 100ms
			ahrs_command(ahrs_number);
			udp_send_state(&local_state,&time_this_cycle);
			udp_send_time = time_this_cycle;
		}
				
	}
}

void INThandler(int dummy) {
	printf("Cleaning up...\n");
	pipecontrol_cleanup();
	exit(1);
}