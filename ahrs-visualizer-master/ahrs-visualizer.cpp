#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <wordexp.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <sys/time.h>

#include <bcm_host.h>

#include <GLES/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <boost/program_options.hpp>
namespace opts = boost::program_options;

#include "model_board/model_board.h"
#include "version.h"

#define MAX 100


int frame = 0;
int state = 0;
/* OpenGL rotation matrix that converts board coordinates
* into ground coordinates (x=north, y=east, z=down).  It is
* column-major so do matrix[COL][ROW]. */
float locked_matrix[4][4];
float last_matrix[4][4];
float matrix[4][4];
float final_matrix[4][4];

// Acceleration vector in units of g (9.8 m/s^2).
float acceleration[3];

// Magnetic field vector where each component is approximately between -1 and 1.
float magnetic_field[3];

// 0 = Screen faces south, 90 = West, 180 = North, 270 = East
// TODO: read screen_orientation from environment
float screen_orientation = 0;

uint32_t screen_width, screen_height;
EGLDisplay display;
EGLSurface surface;
EGLContext context;

static inline VC_RECT_T rect_width_height(int width, int height)
{
	VC_RECT_T r;
	r.x = r.y = 0;
	r.width = width;
	r.height = height;
	return r;
}

// Sets the display, OpenGL|ES context and screen stuff
static void opengl_init(void)
{
	EGLBoolean result;
	EGLint num_config;

	static EGL_DISPMANX_WINDOW_T nativewindow;

	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;

	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	EGLConfig config;

	// get an EGL display connection
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (display == EGL_NO_DISPLAY)
	{
		throw std::runtime_error("Failed to get display.  eglGetDisplay failed.");
	}

	// initialize the EGL display connection
	result = eglInitialize(display, NULL, NULL);
	if (result == EGL_FALSE)
	{
		throw std::runtime_error("Failed to initialize display.  eglInitialize failed.");
	}

	// get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(display, attribute_list, &config, 1, &num_config);
	if (result == EGL_FALSE)
	{
		throw std::runtime_error("Failed to choose config.  eglChooseConfig failed.");
	}

	// create an EGL rendering context
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
	if (context == EGL_NO_CONTEXT)
	{
		throw std::runtime_error("Failed to create context.  eglCreateContext failed.");
	}

	// create an EGL window surface
	int32_t success = graphics_get_display_size(0 /* LCD */, &screen_width, &screen_height);
	if (success < 0)
	{
		throw std::runtime_error("Failed to get display size.");
	}

	VC_RECT_T dst_rect = rect_width_height(screen_width, screen_height);
	VC_RECT_T src_rect = rect_width_height(screen_width<<16, screen_height<<16);

	dispman_display = vc_dispmanx_display_open(0 /* LCD */);
	dispman_update = vc_dispmanx_update_start(0);

	dispman_element = vc_dispmanx_element_add (dispman_update, dispman_display,
	128/*layer*/, &dst_rect, 0/*src*/,
	&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, (DISPMANX_TRANSFORM_T)0/*transform*/);

	nativewindow.element = dispman_element;
	nativewindow.width = screen_width;
	nativewindow.height = screen_height;
	vc_dispmanx_update_submit_sync(dispman_update);

	surface = eglCreateWindowSurface(display, config, &nativewindow, NULL);
	if(surface == EGL_NO_SURFACE)
	{
		fprintf(stderr, "eglCreateWindowSurface returned ELG_NO_SURFACE.  "
		"Try closing other OpenGL programs.\n");
		exit(1);
	}

	// connect the context to the surface
	result = eglMakeCurrent(display, surface, surface, context);
	if (result == EGL_FALSE)
	{
		throw std::runtime_error("Failed to connect to the surface.");
	}

	glClearColor(0, 0, 0, 0);   // set background colors
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear buffers
	glShadeModel(GL_FLAT);

	// Enable back face culling.
	glEnable(GL_CULL_FACE);
}

// Description: Sets the OpenGL|ES model to default values
static void projection_init()
{
	float nearp = 1, farp = 500.0f, hht, hwd;

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glViewport(0, 0, (GLsizei)screen_width, (GLsizei)screen_height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	hht = nearp * tan(45.0 / 2.0 / 180.0 * M_PI);
	hwd = hht * screen_width / screen_height;

	glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
}

static void textures_init(void)
{
	// Enable alpha blending so what we see through transparent
	// parts of the model is the same as the background.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
}


static void redraw_scene()
{
	// Start with a clear screen
	glClear(GL_COLOR_BUFFER_BIT);

	// Move the camera back so we can see the board.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -30);

	//glScalef(1,1,1);
	// Convert screen coords (right, up, out) to ground coords (north, east, down).
	//glRotatef(spinning, 0, 0, 1);
	// glRotatef(0, 0, 0, 1);
	//glRotatef(0, 0, 1, 0);
	// glRotatef(0, 0, 0, 1);
	
	// Convert ground coords to board coordinates.

	//glMultMatrixf(final_matrix[0]);
	
	glMultMatrixf(locked_matrix[0]);
	
	model_board_redraw(acceleration, magnetic_field,frame);
	eglSwapBuffers(display, surface);
}

static void opengl_deinit(void)
{
	// clear screen
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(display, surface);

	// Release OpenGL resources
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(display, surface);
	eglDestroyContext(display, context);
	eglTerminate(display);
}

/* The matrix expected on the standrd input is a 
* ROW-major matrix that converts a vector from board coordinates
* to ground coordinates. */
static void read_input(void)
{



	
	 while(1)
	{
	// Read the rotation matrix from the standard input.
	// The input values are ROW-major but we need to make a
	// COLUMN-major matrix for OpenGL.
	int result = scanf("%f %f %f", &acceleration[0], &acceleration[2], &acceleration[1] );

	//if (result < 0 ) {
	//	perror("AHRS_Process: FIFO_PIPE ");
	//}
	//int result=0;
	//char buf[MAX];	
	//while (result <= 0){
	//	result = read(STDIN_FILENO,buf,MAX);
	//	buf[result] = '\0';
		
	//	sscanf(buf,"%f %f %f", &acceleration[0], &acceleration[2], &acceleration[1] );
	//	if (result <= 0){
	//		usleep(19000);
	//	}
	//}
	
	
	
	acceleration[0] = acceleration[0] * -1.0;
	acceleration[1] = acceleration[1] * -1.0;
	// Read to the end of the line so that we don't get stuck forever on one invalid line.
	 while(getc(stdin) != '\n');

	  if (result >= 3)
	  {
	
	    break; // Success
		
	 }
	
	//fprintf(stderr, "unrecognized input: only regonized %d items.\n", result);
	}
}
int millis()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}
static void read_args(int argc, char *argv[])
{
	try
	{
		opts::options_description generic("Generic options");
		generic.add_options()
		("help,h", "produce help message")
		("version,v", "print version number")
		;

		opts::options_description config("Configuration");
		config.add_options()
		("screen-angle,a", opts::value<float>(&screen_orientation)->default_value(0),
		"specifies your screen orientation.  "
		"0 = screen faces South, 90 = West, 180 = North, 270 = East\n")
		;

		opts::options_description cmdline_options;
		cmdline_options.add(generic);
		cmdline_options.add(config);

		// Read options from command line.
		opts::variables_map options;
		opts::store(opts::command_line_parser(argc, argv).options(cmdline_options).run(), options);

		// Read options form config file, ~/.ahrs-visualizer
		{
			wordexp_t expansion_result;
			wordexp("/home/pi/.ahrs-visualizer", &expansion_result, 0);
			std::ifstream file(expansion_result.we_wordv[0]);
			opts::store(opts::parse_config_file(file, config), options);
		}

		opts::notify(options);

		if(options.count("help"))
		{
			std::cout << cmdline_options;
			std::cout << "For more information, run: man ahrs-visualizer" << std::endl;
			exit(0);
		}

		if (options.count("version"))
		{
			std::cout << VERSION << std::endl;
			exit(0);
		}
	}
	catch(const opts::multiple_occurrences & error)
	{
		std::cerr << "Error: " << error.what() << " of " << error.get_option_name() << " option." << std::endl;
		exit(1);
	}
}


int main(int argc, char *argv[])
{	 
	// Set the translation part to be the identity.
	locked_matrix[3][0] = locked_matrix[3][1] = locked_matrix[3][2] = 0;
	locked_matrix[0][3] = locked_matrix[1][3] = locked_matrix[2][3] = 0;
	locked_matrix[3][3] = 1;

	locked_matrix[0][0] = locked_matrix[1][1] = locked_matrix[2][2] = 1;
	locked_matrix[0][1] = locked_matrix[1][0] = locked_matrix[2][0] = locked_matrix[0][2]= locked_matrix[2][1] = locked_matrix[1][2]=0;
	
	memcpy(&final_matrix,locked_matrix[0],sizeof last_matrix);

	
	int fd;
	
	mkfifo ("/tmp/FIFO_PIPE", 0777 );
	//OPEN PIPE WITH READ ONLY
	if ((fd = open ("/tmp/FIFO_PIPE",  ( O_RDONLY | O_NONBLOCK ) ))<0)
	{
		perror("AHRS_Process: Could not open named pipe for reading.");
		exit(-1);
	}
	printf("Pipe has been opened.\n");
	fflush(stdout);
	
	try
	{
		read_args(argc, argv);

		bcm_host_init();
		opengl_init();
		projection_init();
		textures_init();
		model_board_init();
		printf("Textures have been loaded.\n");
		fflush(stdout);
		
		int fps_counter = 0;
		int fps = 0;
		
		while(1)
		{
			int result;
			char buf[MAX];
			result = read(fd,buf,MAX);
			//if (result < 0 ) {
			//	perror("AHRS_Process: FIFO_PIPE ");

			//}
			if (result > 0){
				buf[result] = '\0';
				frame = atoi(buf);
				if (state != frame){
					printf("AHRS_Process: %d\n",frame);
					state = frame;
				}
			}
			
			
			read_input();
			redraw_scene();

			fps++;
			if (millis() - fps_counter > 1000){
				printf("FPS: %d\n",fps);
				fps = 0;
				fps_counter = millis();
			}
			
		}
		
		opengl_deinit();
		bcm_host_deinit();
		return 0;
	}
	catch(const std::exception & error)
	{
		std::cerr << "Error: " << error.what() << std::endl;
		exit(9);
	}
}



