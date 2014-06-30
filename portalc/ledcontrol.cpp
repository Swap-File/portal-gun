
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/types.h>
#include <stdlib.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "ledcontrol.h" 


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

const int effect_resolution = 400;
const uint8_t ending[2] = {0x00};
int spi_handle_unbuffered; 
FILE * spi_handle_buffered;
const int led_strip_length = 20;
uint8_t raw_buffer[led_strip_length*3]= {0x00};
uint8_t effect_buffer[led_strip_length*3]= {0x00}; 
int timearray[led_strip_length]= {0};
int color1 = 0; 
int color2 = -1; 
int previous_color1 = color1;
uint8_t overlay = 0x80;
bool overlay_primer = true;
bool overlay_enabled = false;
int overlay_timer;

int timeoffset=0;


int offset_target_time = 0 ;
//breathing mode
bool breathing_request = true;
bool breathing_active = true;

int effect_index = 0;


//leading edge mode
bool leading_edge_request = true;
bool leading_edge_active = true;

int led_index = 0;
int led_width_actual = 0;
int led_width_requested = 0	;
int color_update_index = 0;

int total_offset_previous = 0;
int width_speed = 200; //.2 seconds
int cooldown_time= 0; 

const int breathingrate = 2; //period of 2 seconds
float effect_array[effect_resolution];
int ticks_since_overlay_enable = 128; //disabled overlay on bootup
//int shift_speed_last_update;
int width_speed_last_update = 0;
char brightnesslookup[128][effect_resolution] = {0};
//red and blue swapped
void Wheel(int WheelPos, uint8_t *b, uint8_t *r, uint8_t *g){
	
	//color span code
	//if (WheelPos < 384){
	//	WheelPos = (WheelPos +384) % 384;
	//}

	if (WheelPos >= 0){
		switch(WheelPos / 128)
		{
		case 0:
			*r = (127 - WheelPos % 128) ;   //Red down
			*g = (WheelPos % 128);      // Green up
			*b = 0;                  //blue off
			break; 
		case 1:
			*g = (127 - WheelPos % 128);  //green down
			*b =( WheelPos % 128) ;      //blue up
			*r = 0;                  //red off
			break; 
		case 2:
			*b = (127 - WheelPos % 128);  //blue down 
			*r = (WheelPos % 128 );      //red up
			*g = 0;                  //green off
			break; 
		case 3:
			*r = 42;
			*g = 42;
			*b = 42;
			break; 
		case 4:
			*r = 127;
			*g = 127;
			*b = 127;
			break; 
		}
	}else{
		*r = 0;
		*g = 0;
		*b = 0;
	}
	//take into consideration disc brightness

	return;
}

int spi_init(int filedes) {
	int ret;
	const uint8_t mode = SPI_MODE_0;
	const uint8_t bits = 8;
	const uint32_t speed = 16000000;

	ret = ioctl(filedes,SPI_IOC_WR_MAX_SPEED_HZ,&speed);
	if(ret==-1) {
		return -1;
	}

	return 0;
}

int millis()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}

void ledcontrol_setup(void) {
	overlay_timer =  millis();
	
	for ( int i = 0; i < effect_resolution; i++ ) { 
		//add pi/2 to put max value (1) at start of range
		effect_array[i] =  ((exp(sin( M_PI/2  +(float(i)/(effect_resolution/breathingrate)*M_PI))) )/ (M_E));
		//printf( "Y: %f\n", effect_array[i] );
	}
	
	
	
	for ( int x = 0; x < 128; x++ ) { 
		//printf( "X: %d\n", x);
		for ( int y = 0; y < effect_resolution; y++ ) { 
			brightnesslookup[x][y] = int(float(x * effect_array[y])) | 0x80;
			//printf( "Y: %d\n", brightnesslookup[x][y] );
		}
	}
	
	/* Open SPI device */
	spi_handle_unbuffered = open("/dev/spidev0.0",O_WRONLY  );
	if(spi_handle_unbuffered<0) {
		/* Open failed */
		fprintf(stderr, "Error: SPI device open failed.\n");
		exit(1);
	}
	
	spi_handle_buffered = fopen("/dev/spidev0.0","wb"  );
	
	/* Initialize SPI bus for lpd8806 pixels */
	if(spi_init(spi_handle_unbuffered)<0) {
		/* Initialization failed */
		fprintf(stderr, "Unable to initialize SPI bus.\n");
		exit(1);
	}


	width_speed_last_update = millis();

	
}

float ledcontrol_update(int color_temp,int width_temp,int width_speed_temp,int overlay_temp, int  total_offset ) {

	if (total_offset_previous > 200 && total_offset < 200 && led_width_actual == 20){
		//printf("RESET!\n");
		led_index = 0;
	}
	
	total_offset_previous = (total_offset_previous + 4) % 400;  //expected approximate change per cycle
	
	if (abs(total_offset - total_offset_previous) > 8){ //this gives it a bit more leeway  50 FPS, 2 seconds per offset, offset cycle of 400. 400/100 = 4
		//find shortest route 
		if((( total_offset - total_offset_previous + 400) % 400) < 200){  // add to total_offset_previous to reach total_offset
			total_offset_previous = ((total_offset_previous + 8)  % 400 ) ;
			//printf("this: %d previous %d adding to catchup!\n", total_offset, total_offset_previous);
		}else {// subtract from total_offset_previous to reach total_offset
			total_offset_previous = (total_offset_previous - 8 + 400) % 400;
			//printf("this: %d previous %d subbing to catchup!\n", total_offset, total_offset_previous);
		}		   
		total_offset = total_offset_previous; // use old value
	}
		//at 50 FPS, and 2 second effect time, and 20 LEDs, every 2 seconds we do 3 rotations
	//if we are at full width, reset the index to line it up
	

	
	
	total_offset_previous = total_offset;
	

	int time_this_cycle = millis();
	
	if (overlay_temp == 0){
		overlay_primer = true;
		overlay_enabled=false;
		overlay = 0x80;
	}else{
		if( overlay_primer == true && overlay_enabled== false){
			overlay = 0xFF | 0x80;
			overlay_enabled= true;
			overlay_primer = false;
			overlay_timer = time_this_cycle;
		}
	}
	
	if (overlay_primer == true){
		
		if ((color_temp >= -1) && (color_temp < 1024)){
			color1 = color_temp;
		}
	
		if ((width_temp <= 20) && (width_temp >= 0)){
			led_width_requested = width_temp;
		}
		
		if (width_speed_temp >=0){
			width_speed = width_speed_temp;
		}
	}

	
	//on a color change, or coming back from zero width, go full bright on fill complete
	if( previous_color1 != color1 || (led_width_requested !=0 && led_width_actual == 0 ) && overlay_enabled == false){
		//adjust time offset, aiming to hit max brightness as color fill completes
		//offset to half of resolution for 50 FPS
		timeoffset =   (300 -  total_offset ) ;
		cooldown_time = time_this_cycle;
		color_update_index = 0;
		previous_color1 = color1;
	}
	
	//tweak breathing rate to attempt to keep in sync across network
	//only tweaks by 1 each cycle to be unnoticeable
	//only tweak when overlay not enabled

	
	if (time_this_cycle - cooldown_time > 1000){ 
		for ( int i = 0; i < led_strip_length; i++ ){
			if(timearray[i] < 0){  // add to total_offset_previous to reach total_offset
				timearray[i] = timearray[i] + 1;
			}else if(timearray[i] > 0) {// subtract from total_offset_previous to reach total_offset
				timearray[i] = timearray[i] - 1;
			}	
		}
		
		if(timeoffset < 0){  // add to total_offset_previous to reach total_offset
			timeoffset = timeoffset + 1;
			//printf("Subbing to timearray! %d \n",timeoffset);
		}else if(timeoffset > 0){// subtract from total_offset_previous to reach total_offset
			timeoffset = timeoffset - 1;
			//printf("Adding to timearray! %d \n",timeoffset);
		}	

	}

	if (overlay_enabled == true){
		
		ticks_since_overlay_enable = time_this_cycle - overlay_timer;
		
		//printf("curtime %d\n", ticks_since_overlay_enable);
		
		if (ticks_since_overlay_enable > 127){
			//overlay is now done, disable overlay
			//blank the buffer, and set all variables to 0
			overlay_enabled = false;
			led_width_requested = 0;
			led_width_actual = 0 ;
			color_update_index = 20;
			color1 = -1;
			previous_color1 = color1;
			for ( int i = 0; i < led_strip_length; i++ ){
				raw_buffer[3*i+0] = 0x00;
				raw_buffer[3*i+1] = 0x00;
				raw_buffer[3*i+2] = 0x00;
				timearray[i] = timeoffset;
			}
			overlay = 0x80;
			
		}else{	
			//during the overlay ramp up linearly, gives white output
			overlay = ticks_since_overlay_enable | 0x80;
		}
	}
	
	//Update the color1 and time of lit LEDs
	
	if (color_update_index < led_width_actual){
		//timearray[color_update_index] = timeoffset;
		
		timearray[color_update_index] = timeoffset;
		Wheel(color1,&raw_buffer[3*color_update_index+0],&raw_buffer[3*color_update_index+1],&raw_buffer[3*color_update_index+2]);
		color_update_index = color_update_index + 1;

	}
	
	//width stuff
	if (time_this_cycle - width_speed_last_update > width_speed){
		//Narrow the lit section by blanking the index LED and incrementing the index.
		//Don't change time data for color2 LEDs
		if (led_width_requested < led_width_actual && led_width_requested >= 0){
			//printf("smaller\n" );
			
			int location = 3*(led_width_actual -1);
			Wheel(color2, &raw_buffer[location+0], &raw_buffer[location+1], &raw_buffer[location+2] );

			//timearray[led_width_actual] = timeoffset;
			

			led_width_actual = led_width_actual -1;
		}
		//Widen the lit section by copying the index LED color1 and time data and decrementing the index.
		else if( led_width_requested > led_width_actual  && led_width_requested <= 20){
			//printf("bigger\n" );
			
			int location = 3*(led_width_actual);

			if (led_width_actual == 0  ){
				//starting an empty array
				Wheel(color1, &raw_buffer[0],&raw_buffer[1] , &raw_buffer[2] );
				timearray[0] = timeoffset;
			}else{
				raw_buffer[location+0] = raw_buffer[location-3];
				raw_buffer[location+1] = raw_buffer[location-2];
				raw_buffer[location+2] = raw_buffer[location-1];
				timearray[led_width_actual] = timearray[led_width_actual - 1];
			}
			led_width_actual = led_width_actual + 1;
			
			//supress color update code
			color_update_index = color_update_index +1;

		}
		width_speed_last_update = time_this_cycle;
	}
	

	//printf("curtime %d\n", ((total_offset + timearray[0]) % effect_resolution));
	for ( int i = 0; i < led_strip_length; i++ ) {
		
		int curtime = 0;
		//dont apply brightness correction to first LED  Breathing is always active
		if (i != (led_width_actual-1) and breathing_active == true){
			curtime = (total_offset + timearray[i] + effect_resolution) % effect_resolution;
		}
		int current_location = (i + led_index) % led_strip_length;

		effect_buffer[current_location*3 + 0] = brightnesslookup[raw_buffer[i*3 + 0]][curtime] | overlay;
		effect_buffer[current_location*3 + 1] = brightnesslookup[raw_buffer[i*3 + 1]][curtime] | overlay;
		effect_buffer[current_location*3 + 2] = brightnesslookup[raw_buffer[i*3 + 2]][curtime] | overlay;

	}
	fwrite(effect_buffer, sizeof(char),sizeof(effect_buffer),spi_handle_buffered);
	fwrite(effect_buffer, sizeof(char),sizeof(effect_buffer),spi_handle_buffered);
	fwrite(ending, sizeof(char),sizeof(ending),spi_handle_buffered);
	fflush(spi_handle_buffered);
	//Shift array one LED forward and update index
	led_index = (led_index + 1) % led_strip_length;

	
	return effect_array[(total_offset + timeoffset) % effect_resolution];
}

