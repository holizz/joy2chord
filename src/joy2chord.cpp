 /***************************************************************************
 *   Copyright (C) 2007 by Nathanael Anderson                              *
 *   wirelessdreamer AT gmail DOT com                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

// Uinput code based off example in January 2007 Dashboard Issue by Mehul Patel
// http://www.einfochips.com/download/dash_jan_tip.pdf

#include <string>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <linux/joystick.h>

/* Globals */

#define TOOL_NAME "JoystickPlugin"

using namespace std;

static int uinp_fd = -1;
struct uinput_user_dev uinp;       // uInput device structure
struct input_event event; // Input device structure
int joystick_no = 0;
int axes;
int joy_fd;
int buttons;
int verbose = 1;
int maxbad = 40000;

int open_joystick()
{

        char device[256];
        char name[128] = "Unknown";

        sprintf(device, "/dev/input/js%i", joystick_no);

        if ((joy_fd = open(device, O_RDONLY)) < 0) {
                fprintf(stderr, "%s: ", TOOL_NAME); perror(device);
                sprintf(device, "/dev/js%i", joystick_no);

                if ((joy_fd = open(device, O_RDONLY)) < 0) {
                        fprintf(stderr, "%s: ", TOOL_NAME); 
perror(device);
                        exit(-3);
                }
        }

        ioctl(joy_fd, JSIOCGAXES, &axes);
        ioctl(joy_fd, JSIOCGBUTTONS, &buttons);
        ioctl(joy_fd, JSIOCGNAME(128), name);

        cout << "Using Joystick (" << name << ") through device " << device << " with " << axes << " axes and " << buttons <<" buttons." << endl;

        return 0;
}

/* Setup the uinput device */
int setup_uinput_device()
{

        // Temporary variable
        int i=0;
        // Open the input device
        uinp_fd = open("/dev/input/uinput", O_WRONLY | O_NDELAY);
        if (!uinp_fd)
        {
        	cout << "Unable to open /dev/input/uinput" <<endl;
                return -1;
       	}
	memset(&uinp,0,sizeof(uinp)); // Intialize the uInput device to NULL
	strncpy(uinp.name, "Joystick to Keyboard/Mouse Converter", UINPUT_MAX_NAME_SIZE);
       	uinp.id.version = 4;
       	uinp.id.bustype = BUS_USB;
       	
	// Setup the uinput device
       	ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
       	ioctl(uinp_fd, UI_SET_EVBIT, EV_REL);
       	ioctl(uinp_fd, UI_SET_RELBIT, REL_X);
       	ioctl(uinp_fd, UI_SET_RELBIT, REL_Y);
       	for (i=0; i < 256; i++) {
        	ioctl(uinp_fd, UI_SET_KEYBIT, i);
       	}
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TOUCH);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_LEFT);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MIDDLE);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_RIGHT);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_FORWARD);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_BACK);
       
       	/* Create input device into input sub-system */
       	write(uinp_fd, &uinp, sizeof(uinp));
       	if (ioctl(uinp_fd, UI_DEV_CREATE))
       	{
        	cout << "Unable to create UINPUT device.";
                return -1;
       	}
       	return 1;
}

void send_click_events( ) //not implemented yet
{
       	// Move pointer to (0,0) location
       	memset(&event, 0, sizeof(event));
       	gettimeofday(&event.time, NULL);
     	event.type = EV_REL;
     	event.code = REL_X;
     	event.value = 100;
     	write(uinp_fd, &event, sizeof(event));
     	event.type = EV_REL;
     	event.code = REL_Y;
     	event.value = 100;
     	write(uinp_fd, &event, sizeof(event));
     	event.type = EV_SYN;
     	event.code = SYN_REPORT;
     	event.value = 0;
     	write(uinp_fd, &event, sizeof(event));
       	
	// Report BUTTON CLICK - PRESS event
       	memset(&event, 0, sizeof(event));
       	gettimeofday(&event.time, NULL);
       	event.type = EV_KEY;
       	event.code = BTN_LEFT;
       	event.value = 1;
       	write(uinp_fd, &event, sizeof(event));
       	event.type = EV_SYN;
       	event.code = SYN_REPORT;
       	event.value = 0;
       	write(uinp_fd, &event, sizeof(event));
       
       	// Report BUTTON CLICK - RELEASE event
       	memset(&event, 0, sizeof(event));
       	gettimeofday(&event.time, NULL);
       	event.type = EV_KEY;
       	event.code = BTN_LEFT;
       	event.value = 0;
       	write(uinp_fd, &event, sizeof(event));
       	event.type = EV_SYN;
       	event.code = SYN_REPORT;
       	event.value = 0;
       	write(uinp_fd, &event, sizeof(event));
}

void send_key_down(__u16 code){
	// Report BUTTON CLICK - PRESS event
  	memset(&event, 0, sizeof(event));
       	gettimeofday(&event.time, NULL);
     	event.type = EV_KEY;
     	event.code = code;
      	event.value = 1;
      	write(uinp_fd, &event, sizeof(event));
      	event.type = EV_SYN;
      	event.code = SYN_REPORT;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
}

void send_key_up(__u16 code){
	// Report BUTTON CLICK - RELEASE event
        memset(&event, 0, sizeof(event));
        gettimeofday(&event.time, NULL);
      	event.type = EV_KEY;
      	event.code = code;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
      	event.type = EV_SYN;
      	event.code = SYN_REPORT;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
}

void main_loop()
{
        struct js_event js;

	int joy_b1 = 4;
	int joy_b2 = 6;
	int joy_b3 = 9;
	int joy_b4 = 3;
	int joy_b5 = 5;
	int joy_b6 = 7;

	int button_state[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int send_code[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int button_code = 0;
	int mode = 1;
	int mode3count = 0;
	int doubleclick = 0;

	int mode1_code = 31; // this key combination (thumb 1 + 4 finger buttons changes to mode 1
	int mode2_code = 47; // 47 this key combination ( thumb 2 + 4 finger buttons changes to mode 2
	int mode3_code = 63; // 63 this key combination ( thumb 1 & 2 + 4 finger buttons changes to mode 3

	int total_modes = 3; 
	__u16 modes[total_modes][64];
	__u16 lastkey = KEY_RESERVED; // initilize the last key to nothing
	__u16 thiskey = KEY_RESERVED; // for tracking the current key

	int total_mods = 4; // if this is changed the if statement where codes are sent needs altered also
	int norelease = 0;
	int justpressed = 0;
	int modifier_state[4] = {0,0,0,0};

	__u16 modifier[total_mods];
	modifier[0] = KEY_LEFTMETA;
	modifier[1] = KEY_LEFTCTRL;
	modifier[2] = KEY_LEFTALT;
	modifier[3] = KEY_LEFTSHIFT;
	int meta = 0;
	int ctrl = 0;
	int alt = 0;
	int shift = 0;

	//Mappings from /usr/include/linux/input.h	
	// position 0 never gets touched
	
	modes[1][1] = KEY_A;
        modes[1][2] = KEY_B;
        modes[1][3] = KEY_C;
        modes[1][4] = KEY_D;
        modes[1][5] = KEY_E;
        modes[1][6] = KEY_F;
        modes[1][7] = KEY_G;
        modes[1][8] = KEY_H;
        modes[1][9] = KEY_I;
        modes[1][10] = KEY_J;
        modes[1][11] = KEY_K;
        modes[1][12] = KEY_L;
        modes[1][13] = KEY_M;
        modes[1][14] = KEY_N;
        modes[1][15] = KEY_O;
        modes[1][16] = KEY_P;
        modes[1][17] = KEY_Q;
        modes[1][18] = KEY_R;
        modes[1][19] = KEY_S;
        modes[1][20] = KEY_T;
        modes[1][21] = KEY_U;
        modes[1][22] = KEY_V;
        modes[1][23] = KEY_W;
        modes[1][24] = KEY_X;
        modes[1][25] = KEY_Y;
        modes[1][26] = KEY_Z;
        modes[1][27] = KEY_LEFTSHIFT;
	modes[1][28] = KEY_LEFTMETA;
        modes[1][29] = KEY_LEFTCTRL;
        modes[1][30] = KEY_LEFTALT;
	modes[1][31] = KEY_KBDILLUMTOGGLE; // using this for mode one
	modes[1][32] = KEY_ENTER;
	modes[1][33] = KEY_SPACE;
	modes[1][34] = KEY_TAB;
	modes[1][35] = KEY_BACKSPACE;
	modes[1][36] = KEY_RESERVED;
	modes[1][37] = KEY_RESERVED;
	modes[1][38] = KEY_BRIGHTNESSDOWN; // binding for ctrl+alt+right
	modes[1][39] = KEY_BRIGHTNESSUP; // binding for ctrl+alt+left
	modes[1][40] = KEY_ESC;
	modes[1][41] = KEY_PAGEUP;
	modes[1][42] = KEY_PAGEDOWN;
	modes[1][43] = KEY_HOME;
	modes[1][44] = KEY_END;
	modes[1][45] = KEY_INSERT;
	modes[1][46] = KEY_DELETE;
	modes[1][47] = KEY_KBDILLUMDOWN; //using this for mode two
	modes[1][48] = KEY_RESERVED;
	modes[1][49] = KEY_RESERVED;
	modes[1][50] = KEY_RESERVED;
	modes[1][51] = KEY_RESERVED;
	modes[1][52] = KEY_RESERVED;
	modes[1][53] = KEY_RESERVED;
	modes[1][54] = KEY_RESERVED;
	modes[1][55] = KEY_RESERVED;
	modes[1][56] = KEY_RESERVED;
	modes[1][57] = KEY_RESERVED;
	modes[1][58] = KEY_RESERVED;
	modes[1][59] = KEY_RESERVED;
	modes[1][60] = KEY_RESERVED;
	modes[1][61] = KEY_RESERVED;
	modes[1][62] = KEY_RESERVED;
	modes[1][63] = KEY_KBDILLUMUP; //using this for mode two
	
	modes[2][1] = KEY_1;
        modes[2][2] = KEY_2;
        modes[2][3] = KEY_3;
        modes[2][4] = KEY_4;
        modes[2][5] = KEY_5;
        modes[2][6] = KEY_6;
        modes[2][7] = KEY_7;
        modes[2][8] = KEY_8;
        modes[2][9] = KEY_9;
        modes[2][10] = KEY_0;
        modes[2][11] = KEY_RESERVED;
        modes[2][12] = KEY_RESERVED;
        modes[2][13] = KEY_RESERVED;
        modes[2][14] = KEY_RESERVED;
        modes[2][15] = KEY_RESERVED;
        modes[2][16] = KEY_RESERVED;
        modes[2][17] = KEY_RESERVED;
        modes[2][18] = KEY_RESERVED;
        modes[2][19] = KEY_RESERVED;
        modes[2][20] = KEY_RESERVED;
        modes[2][21] = KEY_LEFTBRACE;
        modes[2][22] = KEY_RIGHTBRACE;
        modes[2][23] = KEY_RESERVED;
        modes[2][24] = KEY_RESERVED;
        modes[2][25] = KEY_RESERVED;
        modes[2][26] = KEY_RESERVED;
        modes[2][27] = KEY_LEFTSHIFT;	
	modes[2][28] = KEY_LEFTMETA;
        modes[2][29] = KEY_LEFTCTRL;
        modes[2][30] = KEY_LEFTALT;
	modes[2][31] = KEY_KBDILLUMTOGGLE; // using this for mode one
	modes[2][32] = KEY_ENTER;
	modes[2][33] = KEY_SPACE;
	modes[2][34] = KEY_TAB;
	modes[2][35] = KEY_BACKSPACE;
	modes[2][36] = KEY_RESERVED;
	modes[2][37] = KEY_RESERVED;
	modes[2][38] = KEY_RESERVED;
	modes[2][39] = KEY_RESERVED;
	modes[2][40] = KEY_ESC;
	modes[2][41] = KEY_RESERVED;
	modes[2][42] = KEY_RESERVED;
	modes[2][43] = KEY_HOME;
	modes[2][44] = KEY_END;
	modes[2][45] = KEY_INSERT;
	modes[2][46] = KEY_DELETE;
	modes[2][47] = KEY_KBDILLUMDOWN; //using this for mode two
	modes[2][48] = KEY_RESERVED;
	modes[2][49] = KEY_RESERVED;
	modes[2][50] = KEY_RESERVED;
	modes[2][51] = KEY_RESERVED;
	modes[2][52] = KEY_RESERVED;
	modes[2][53] = KEY_RESERVED;
	modes[2][54] = KEY_RESERVED;
	modes[2][55] = KEY_RESERVED;
	modes[2][56] = KEY_RESERVED;
	modes[2][57] = KEY_RESERVED;
	modes[2][58] = KEY_RESERVED;
	modes[2][59] = KEY_RESERVED;
	modes[2][60] = KEY_RESERVED;
	modes[2][61] = KEY_RESERVED;
	modes[2][62] = KEY_RESERVED;
	modes[2][63] = KEY_KBDILLUMUP; //using this for mode two

	modes[3][1] = KEY_UP;
        modes[3][2] = KEY_LEFT;
        modes[3][3] = KEY_PAGEUP;
        modes[3][4] = KEY_RIGHT;
        modes[3][5] = KEY_RESERVED;
        modes[3][6] = KEY_RESERVED;
        modes[3][7] = KEY_RESERVED;
        modes[3][8] = KEY_DOWN;
        modes[3][9] = KEY_RESERVED;
        modes[3][10] = KEY_RESERVED;
        modes[3][11] = KEY_RESERVED;
        modes[3][12] = KEY_PAGEDOWN;
        modes[3][13] = KEY_RESERVED;
        modes[3][14] = KEY_RESERVED;
        modes[3][15] = KEY_RESERVED;
        modes[3][16] = KEY_RESERVED;
        modes[3][17] = KEY_RESERVED;
        modes[3][18] = KEY_RESERVED;
        modes[3][19] = KEY_RESERVED;
        modes[3][20] = KEY_RESERVED;
        modes[3][21] = KEY_RESERVED;
        modes[3][22] = KEY_RESERVED;
        modes[3][23] = KEY_RESERVED;
        modes[3][24] = KEY_RESERVED;
        modes[3][25] = KEY_RESERVED;
        modes[3][26] = KEY_RESERVED;
        modes[3][27] = KEY_LEFTSHIFT;
	modes[3][28] = KEY_LEFTMETA;
        modes[3][29] = KEY_LEFTCTRL;
        modes[3][30] = KEY_LEFTALT;
	modes[3][31] = KEY_KBDILLUMTOGGLE; // using this for mode one
	modes[3][32] = KEY_ENTER;
	modes[3][33] = KEY_F1;
	modes[3][34] = KEY_F2;
	modes[3][35] = KEY_F3;
	modes[3][36] = KEY_F4;
	modes[3][37] = KEY_F5;
	modes[3][38] = KEY_F6;
	modes[3][39] = KEY_F7;
	modes[3][40] = KEY_F8;
	modes[3][41] = KEY_F9;
	modes[3][42] = KEY_F10;
	modes[3][43] = KEY_F11;
	modes[3][44] = KEY_F12;
	modes[3][45] = KEY_RESERVED;
	modes[3][46] = KEY_RESERVED;
	modes[3][47] = KEY_KBDILLUMDOWN; //using this for mode two
	modes[3][48] = KEY_RESERVED;
	modes[3][49] = KEY_RESERVED;
	modes[3][50] = KEY_RESERVED;
	modes[3][51] = KEY_RESERVED;
	modes[3][52] = KEY_RESERVED;
	modes[3][53] = KEY_RESERVED;
	modes[3][54] = KEY_RESERVED;
	modes[3][55] = KEY_RESERVED;
	modes[3][56] = KEY_RESERVED;
	modes[3][57] = KEY_SYSRQ;
	modes[3][58] = KEY_BREAK;
	modes[3][59] = KEY_PAUSE;
	modes[3][60] = KEY_CAPSLOCK;
	modes[3][61] = KEY_NUMLOCK;
	modes[3][62] = KEY_SCROLLLOCK;
	modes[3][63] = KEY_KBDILLUMUP; //using this for mode two
	
	while (1){
		if (read(joy_fd, &js, sizeof(struct js_event)) != sizeof(struct js_event)) {
			perror(TOOL_NAME ": error reading from joystick device");
			exit (-5);
		}

		switch(js.type & ~JS_EVENT_INIT) {
                	case JS_EVENT_BUTTON:
				if (js.value) { // if a button is pressed down remember its state until all buttons are released
	                                if (js.number == joy_b1){
	                                	button_state[0] = 1;
	                                        send_code[0] = 1;
	                                }
	                                if (js.number == joy_b2){
	                                        button_state[1] = 1;
	                                        send_code[1] = 1;
	                                }
	                                if (js.number == joy_b3){
	                                        button_state[2] = 1;
	                                        send_code[2] = 1;
	                                }
	                                if (js.number == joy_b4){
	                                        button_state[3] = 1;
	                                        send_code[3] = 1;
	                                }
	                                if (js.number == joy_b5){
	                                        button_state[4] = 1;
	                                        send_code[4] = 1;
	                                }
	                  		if (js.number == joy_b6){
	                                        button_state[5] = 1;
	                                        send_code[5] = 1;
	                                }      
				}else{ // track when buttons are released
	 				if (js.number == joy_b1){
	                                        button_state[0] = 0;
	                                }
	                                if (js.number == joy_b2){
	                                        button_state[1] = 0;
	                                }
					if (js.number == joy_b3){
	                                        button_state[2] = 0;
	                                }
	                                if (js.number == joy_b4){
	                                        button_state[3] = 0;
	                                }
	                                if (js.number == joy_b5){
	                                        button_state[4] = 0;
	                                }
	                       		if (js.number == joy_b6){
	                                        button_state[5] = 0;
	                                } 
				}
				thiskey = modes[mode][button_code];	
	                                // this is the "key code" that is going to be sent
				// sanity checker, to make sure no bad data get through
				if (send_code[0] > maxbad){send_code[0] = 0;}
				if (send_code[1] > maxbad){send_code[1] = 0;}
				if (send_code[2] > maxbad){send_code[2] = 0;}
				if (send_code[3] > maxbad){send_code[3] = 0;}
				if (send_code[4] > maxbad){send_code[4] = 0;}
				if (send_code[5] > maxbad){send_code[5] = 0;}
				
				if (verbose){
					cout << "Before: 1:" << send_code[0] << " 2:"<< send_code[1] << " 3:" <<  send_code[2] << " 4:" << send_code[3] << " 5:" <<  send_code[4] << " 6:" << send_code[5] << " ctrl: " << ctrl << " alt: " << alt << " shift: " << shift << " meta: " << meta << endl;
				}
	                        button_code = (send_code[0] + (send_code[1] * 2) + (send_code[2] * 4) + (send_code[3] * 8) + (send_code[4] * 16) + (send_code[5] * 32));
	                        if ((button_state[0] == 0) && (button_state[1] == 0) && (button_state[2] == 0) && (button_state[3] == 0) && (button_state[4] == 0) && (button_state[5] == 0) && (button_code != 0)){ // if all buttons are released then send the code and clear everything
					if (button_code == mode1_code){
						cout << "mode 1" <<endl;
						mode = 1;
					}
					if (button_code == mode2_code){
						cout << "mode 2" <<endl;
						mode = 2;
					}
					if (button_code == mode3_code){
						cout << "mode 3" <<endl;
						mode = 3;
					}
					justpressed = 0;
		 			if ((button_code != mode1_code) && (button_code != mode2_code) && (button_code != mode3_code) && (thiskey != KEY_RESERVED)){
						if (verbose){
							cout << "Sending Down Code: " <<  button_code << endl;
						}
						send_key_down(thiskey);	
						lastkey = thiskey;
					}

					if (thiskey == KEY_LEFTALT){ alt = 1; justpressed = 1;}
					if (thiskey == KEY_LEFTSHIFT){ shift = 1; justpressed = 1; }
					if (thiskey == KEY_LEFTCTRL){ ctrl = 1; justpressed = 1; }
					if (thiskey == KEY_LEFTMETA){ meta = 1; justpressed = 1; }
				
					if ((button_code != mode1_code) && (button_code != mode2_code) && (button_code != mode3_code) && (thiskey != KEY_RESERVED) && (justpressed == 0)){
						if (verbose){
							cout << "Sending Up Code: " <<  button_code << endl;
						}
						send_key_up(thiskey);	
						if ((alt == 1) && (thiskey != KEY_TAB) && (thiskey != KEY_BACKSPACE)){ 
							alt = 0;
							send_key_up(KEY_LEFTALT);
						}		
						if (ctrl == 1){ 
							ctrl = 0;
							send_key_up(KEY_LEFTCTRL);
						}
						if (shift == 1){ 
							shift = 0;
							send_key_up(KEY_LEFTSHIFT);
						}
						if (meta == 1){ 
							meta = 0;
							send_key_up(KEY_LEFTMETA);
						}
					}	
	
				int clearl;
				for (clearl=0; clearl <16; clearl++){
					send_code[clearl] = 0;
				}
	                        button_code = 0;
				doubleclick = 0;
				}
	                        break;
		}
	
	
		}//while
}

int main()
{
         // Return an error if device not found.
         if (setup_uinput_device() < 0)
         {
                 cout << "Unable to find uinput device" << endl;
                 return -1;
         }
	open_joystick();
	main_loop();
	//send_click_events();           // Send mouse event
        /* Destroy the input device */
        ioctl(uinp_fd, UI_DEV_DESTROY);
        /* Close the UINPUT device */
        close(uinp_fd);
}

