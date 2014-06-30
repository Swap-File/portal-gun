#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <stdexcept>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "data.h"
#include "../png_texture.h"
#define PI 3.14159265358979323846
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
GLfloat texCoords[] = {
	0,  0,
	1,  0,
	0,  1,
	1,  1,

	0,  0,
	1,  0,
	0,  1,
	1,  1,

	0,  0,
	1,  0,
	0,  1,
	1,  1,

	// These objects don't use textures:
	0,0, 0,0, // x axis
	0,0, 0,0, // y axis
	0,0, 0,0, // z axis
	0,0, 0,0, // acceleration line
	0,0, 0,0, // magnetic field lines
	// These objects don't use textures:
	0,0, 0,0, // x axis
	0,0, 0,0, // y axis
	0,0, 0,0, // z axis
	0,0, 0,0, // acceleration line
	0,0, 0,0, // magnetic field lines
};

GLfloat vertices[700] = {
	/* TOP */
	-EX, -EY,  0,
	EX, -EY,  0,
	-EX,  EY,  0,
	EX,  EY,  0,

	/* BOTTOM */
	-EX, -EY,  .5,
	EX, -EY,  .5,
	-EX,  EY,  .5,
	EX,  EY,  .5,

	/* BOTTOM */
	-EX, -EY,  1,
	EX, -EY,  1,
	-EX,  EY,  1,
	EX,  EY,  1,


};
static GLuint orange_1,orange_0,blue_0,blue_1,texture_orange,texture_blue, texture_blank,texture_black;
float portal_spin = 0;
float portal_background_spin = 0;
float portal_background_fader = 0;

float last_acceleration[2] = {0,0};
float running_acceleration[2] = {0,0};

float closed_fader = 0;
int close_processed = 999; //impossible value to force process on boot

float blank_fader = 0;
int  blank_processed = 999; //impossible value to force process on boot

int lastcolor = -1;


float line_array_angles[100] = {0};
float line_array_mags[100] = {0};

void model_board_init(void)
{
	orange_0 = png_texture_load(ASSET_DIR "/orange_0.png", NULL, NULL);
	orange_1 = png_texture_load(ASSET_DIR "/orange_1.png", NULL, NULL);

	blue_0 = png_texture_load(ASSET_DIR "/blue_0.png", NULL, NULL);
	blue_1 = png_texture_load(ASSET_DIR "/blue_1.png", NULL, NULL);
	
	texture_orange = png_texture_load(ASSET_DIR "/orange_portal.png", NULL, NULL);
	texture_blue = png_texture_load(ASSET_DIR "/blue_portal.png", NULL, NULL);
	
	texture_blank = png_texture_load(ASSET_DIR "/blank.png", NULL, NULL);
	texture_black = png_texture_load(ASSET_DIR "/black.png", NULL, NULL);
	
	if (texture_blank ==0||texture_black ==0||texture_orange == 0 || texture_blue == 0|| orange_0 == 0|| orange_1 == 0|| blue_0 == 0|| blue_1 == 0)
	{
		throw std::runtime_error("Loading textures failed.");
	}

	
	
	
	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);


	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

	
	glClientActiveTexture(GL_TEXTURE0);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);


	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
	
}

void model_board_redraw(float * acceleration, float * magnetic_field, int frame)
{

	portal_spin = portal_spin + .50;
	if (portal_spin > 360){
		portal_spin = portal_spin - 360;
	}
	
	portal_background_spin = portal_background_spin - .01;
	if (portal_background_spin < 360){
		portal_background_spin = portal_background_spin + 360;
	}

	portal_background_fader = portal_background_fader + .01;
	if (portal_background_fader > .75){
		portal_background_fader = -.75;
	}

	
	
	if CHECK_BIT(frame,1){
		if (lastcolor != 1){
			
			blank_fader = -100;
			blank_processed = 1;
			closed_fader = .99;
			close_processed=2;
		} 
	}else{
		if (lastcolor != 2){
			blank_fader = -100;
			blank_processed = 1;
			closed_fader = .99;
			close_processed=2;
		} 
	}


	//this controls making the portal zoom in (shutter)
	if CHECK_BIT(frame,3){
		if (blank_processed !=1){
			blank_fader = -100;
			blank_processed = 1;
		}
		
	}else{
		
		if (blank_processed != 2){
			blank_fader = -99.99;
			blank_processed=2;
		}
	}

	//this controls making the portal fade into the backgroud video
	if CHECK_BIT(frame,0){
		
		if (close_processed !=1){
			closed_fader = 1;
			close_processed = 1;
		}
	}else{
		//wait for zoom to finish before fading to background
		if (close_processed != 2 and blank_fader >= -.5){
			closed_fader = .99;
			close_processed=2;
		}
	}
	
	
	glEnable(GL_TEXTURE0);
	glEnable(GL_TEXTURE1);

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	

	if CHECK_BIT(frame,1){
		glBindTexture(GL_TEXTURE_2D, orange_0);
	}else{
		glBindTexture(GL_TEXTURE_2D, blue_0);
	}
	
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	
	
	GLfloat rgba[4] = {1.0,1.0,1.0,0.0};
	
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, rgba);
	
	
	//use the texture's color, no blending
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
	//glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_CONSTANT);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	//use the texture's alpha channel
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA); 
	

	
	
	glActiveTexture(GL_TEXTURE1);
	glEnable(GL_TEXTURE_2D);
	if CHECK_BIT(frame,1){
		glBindTexture(GL_TEXTURE_2D, orange_1);
	}else{
		glBindTexture(GL_TEXTURE_2D, blue_1);
	}
	
	GLfloat rgba2[4] = {1.0,1.0,1.0,(GLfloat)fabs(portal_background_fader)};
	
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, rgba2);

	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	//use the texture's color, no blending
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PREVIOUS);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_RGB, GL_TEXTURE);
	//glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC2_RGB, GL_CONSTANT);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
	//use the texture's alpha channel
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC1_ALPHA, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA); 
	
	glColor4f(1.0 ,1.0 ,1.0 ,closed_fader);

	//this controls fadeout speed of the portal
	if (closed_fader > 0 and closed_fader < 1 ){
		closed_fader = closed_fader - 0.007;
		if (closed_fader<0){
			closed_fader = 0;
		}
	}
	glPushMatrix();
	
	glRotatef(portal_background_spin, 0, 0, 1);	
	glDrawArrays(GL_TRIANGLE_STRIP,0, 4);
	
	
	glPopMatrix();
	glBindTexture(GL_TEXTURE_2D, 0);  // Turn off textures
	glDisable(GL_TEXTURE1);		
	
	
	

	glBindTexture(GL_TEXTURE_2D, 0);  // Turn off textures

	// Acceleration
	const float accel_scale = 10;
	
	
	
	if CHECK_BIT(frame,2){
		glColor4f(247.0/255.0 ,145.0/255.0 ,38.0/255.0 ,1.0);
	}else{
		glColor4f(23.0/255.0 ,192.0/255.0 ,233.0/255.0 ,1.0);
	}
	
	
	

	
	
	float alpha = .5;
	float relative_acceleration[2];
	
	relative_acceleration[0] = (last_acceleration[0] - acceleration[0]);
	relative_acceleration[1] = (last_acceleration[1] - acceleration[1]);
	
	//ignore small disturbances
	if (fabs(relative_acceleration[0] ) < .02  && fabs(running_acceleration[0]) < .02 ){
		relative_acceleration[0] = 0;
	}
	if (fabs(relative_acceleration[1] ) < .02 && fabs(running_acceleration[1]) < .02  ){
		relative_acceleration[1] = 0;
	}
	
	//filter acceleration
	running_acceleration[0] = running_acceleration[0] * alpha + (1-alpha)*relative_acceleration[0];
	running_acceleration[1] = running_acceleration[1] * alpha + (1-alpha)*relative_acceleration[1];
	
	//scale magnitude to 0 to 1
	float running_magnitude = sqrt( running_acceleration[0]  * running_acceleration[0]  + running_acceleration[1] * running_acceleration[1]);
	running_magnitude = running_magnitude / .5;
	running_magnitude = MIN(running_magnitude,1);
	
	float angle_target = atan2(running_acceleration[1],running_acceleration[0] );
	
	//printf( "%f %f \n", running_acceleration[0],running_acceleration[1]);
	
	int starting_verticie = 12*3;
	int circlepoint_resolution = 100;

	float scale_factor = 11.0 * (1.0-(blank_fader/-100.0));
	
	//printf( "%f %f %f\n",offset, x, y);
	int arrayindex = 0;
	for ( int circlepoint = 0; circlepoint < circlepoint_resolution;circlepoint++ ) {
		
		arrayindex = starting_verticie + circlepoint*6;
		
		float offset = 2*PI*(((float)circlepoint)/circlepoint_resolution) + ((float)portal_spin)* PI/180;
				
		float x =  sin(offset)* scale_factor;
		float y = 1.34*cos(offset)* scale_factor;
		
		float angle_current = atan2(x,y);
		
		//printf( "%f %f %f\n",offset, x, y);
		
		vertices[arrayindex + 0] = y ;
		vertices[arrayindex + 1] = x ;
		vertices[arrayindex +2] = 0.0;
		 
		float length = 1;
		
		//float twist = running_magnitude * .5 * PI;
		
		// offset+ .5 *PI  is trailing perfectly tangent
		// offset  is fully erect
		 
		//generates  0-1 number, 1 at the direction of acceleration
		float angle_diff =(1 - ( PI - fabs(fmod(fabs(angle_current - angle_target), 2*PI) - PI))/PI);
				
		//make values slowly fall off
		line_array_angles[circlepoint] = line_array_angles[circlepoint] * .99;
		line_array_mags[circlepoint] = line_array_mags[circlepoint] * .99;
		
		
		//line_array_angles[circlepoint] = MAX( line_array_angles[circlepoint] ,angle_diff); 
		//line_array_angles[circlepoint] = line_array_angles[circlepoint] * .5 + angle_diff * .5;
		if (angle_diff >  line_array_angles[circlepoint]){
			line_array_angles[circlepoint] = line_array_angles[circlepoint] * .5 + angle_diff * .5;
		}
		
		float newmag =4*angle_diff * running_magnitude;
		
		line_array_mags[circlepoint] = MAX( line_array_mags[circlepoint] ,newmag); 
		//line_array_mags[circlepoint] = line_array_mags[circlepoint] * .5 + newmag * .5;
		if (newmag >  line_array_mags[circlepoint]){
			line_array_mags[circlepoint] = line_array_mags[circlepoint] * .5 + newmag * .5;
		}
		float twist =  offset + .5 *PI -line_array_angles[circlepoint]  * .5 * PI;
		
		float x2 = x - sin(twist)* scale_factor *line_array_mags[circlepoint] ;
		float y2 = y - 1.34*cos(twist)* scale_factor  *line_array_mags[circlepoint]  ;
				
		vertices[arrayindex + 3] = y2;
		vertices[arrayindex + 4] =  x2 ;
		vertices[arrayindex + 5] =  0.0;
		
	}
	
	//arrayindex = starting_verticie + circlepoint_resolution*6;
	//vertices[arrayindex+0] = acceleration[0] * accel_scale;
	//vertices[arrayindex+1] = acceleration[1] * accel_scale;
	//vertices[arrayindex+2] = 0.0;
	//vertices[arrayindex + 3] = 0.0 ;
	//vertices[arrayindex+ 4] = 0.0 ;
	//vertices[arrayindex + 5] = 0.0;
	
	//arrayindex = arrayindex + 6;
	

	//vertices[arrayindex+0] = running_acceleration[0] * 10*accel_scale;
	//vertices[arrayindex+1] = running_acceleration[1]*10*accel_scale;
	//vertices[arrayindex+2] = 0.0;
	//vertices[arrayindex + 3] = 0.0 ;
	//vertices[arrayindex+ 4] = 0.0 ;
	//vertices[arrayindex + 5] = 0.0;
	
	last_acceleration[0] = acceleration[0];
	last_acceleration[1] = acceleration[1];
	
	glDrawArrays(GL_LINES, 12, (circlepoint_resolution )* 2 );
	
	
	
	
	
	
	glActiveTexture(GL_TEXTURE0);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	
	glPushMatrix();
	glColor4f(0,0,0,1);	  
	// Portal
	if CHECK_BIT(frame,2){
		glBindTexture(GL_TEXTURE_2D, texture_orange);
	}else{
		glBindTexture(GL_TEXTURE_2D, texture_blue);
	}
	
	//iris speed
	if( blank_fader < 0 ){
		blank_fader = blank_fader - .2*blank_fader ;
		if( blank_fader > 0 ){
			blank_fader = 0;
		}
	}
	

	// printf( "%f %f \n",closed_fader, blank_fader);
	
	texCoords[8] =  texCoords[9] =  texCoords[11] =  texCoords[12] = (blank_fader);
	texCoords[10] =  texCoords[13] =  texCoords[14] =  texCoords[15] = (1.0 - blank_fader);
	
	glScalef(1.55,1.15,1);
	glRotatef(portal_spin, 0, 0, 1);
	glDrawArrays(GL_TRIANGLE_STRIP, 4, 4);

	glPopMatrix();
	
	
	
	
	
	
	
	
	
	
	glColor4f(0,0,0,1);	  
	
	GLfloat rgba4[4] = {1.0,1.0,1.0,1.0};
	
	glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, rgba4);
	
	// shutter
	
	if  CHECK_BIT(frame,3){
		glBindTexture(GL_TEXTURE_2D, texture_black);
		lastcolor = -1;
		blank_fader = -100;
		blank_processed = 1;
		closed_fader = 1;
		close_processed = 1;
		
	}else{
		glBindTexture(GL_TEXTURE_2D, texture_blank);
	}
	
	glDrawArrays(GL_TRIANGLE_STRIP, 8, 4);
	glBindTexture(GL_TEXTURE_2D, 0);  // Turn off textures
	
	if CHECK_BIT(frame,1){
		lastcolor = 1; //blue
	}else{
		lastcolor = 2; //orange
	}
	
}
