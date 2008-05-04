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
#include <math.h>
#include <sstream>
#include <map>
#include <errno.h>

#include "ConfigFile.h" // from http://www-personal.umich.edu/~wagnerr/ConfigFile.html

/* Globals */

#define TOOL_NAME "joy2chord"

using namespace std;

const int MAX_CODES = 512; // input.h can define up to 512 input buttons, so using 512 here too
const int MAX_BUTTONS = 64;// I ran into problems dynamically allocating how many array positions there would be for holding buttons, so using this for now
const int MAX_MODES = 16; // same problem with dynamically allocating
const int MAX_AXES = 64; // same problem with dynamically allocating
const int MAX_BAD = 40000; // A workaround for handling device initilization input, This should only be a Temp Solution
const int MAX_MACROS = 16; // How many macros can be understood
const int MAX_MODIFIERS = 8; // How many modifier codes can be used (ctrl, alt, meta, etc) these keys are held down untill a non modifier is held down

static int uinp_fd = -1;
struct uinput_user_dev uinp;       // uInput device structure
struct input_event event; // Input device structure

class joy2chord
{
public:
	string device_name;
	string config_file;

	// mapping variables
	__u16 modes[MAX_MODES][MAX_CODES];
	__u16 simple_modes[MAX_MODES][MAX_CODES];
	__u16 modifier[MAX_MODIFIERS];
	int total_modifiers;
	int modifier_state[MAX_MODIFIERS];
	int total_modes;
	int mode_code[MAX_MODES];
	int total_macros;
	int macro_values[MAX_MACROS];
	int mode;
	int button_state[MAX_BUTTONS];
	int send_code[MAX_BUTTONS];
	int modecount[MAX_MODES];
	int button_code;
	int verbose;
	int debug;
	int calibration;
	
	// joystick variables
	int device_number; 
	int total_chorded_buttons; // how many buttons are treated as a chorded keyboard
	int total_simple_buttons; // how many buttons are treated as a normal key
	int controller_buttons; // how many buttons the hardware controller provides
	int axes; // how many axes we defined functions for
	int total_axes; // how many analog axes the hardware controller provides
	int chord_values[MAX_CODES];
	int simple_values[MAX_CODES];
	int joy_fd;
	__u16 lastkey;
	__u16 thiskey;
	int justpressed;

	int open_joystick();
	int setup_uinput_device();
	int read_config(map<string, __u16> & chordmap);
	void send_click_events();
	void send_key_down(__u16 key_code);
	void send_key_up(__u16 key_code);
	void process_events(js_event js);
	int valid_key(string newkey);
	void main_loop(map<string, __u16> chordmap);
	void ioctl_wrapper(int uinp_fd, int UI_SETBIT, int i);
	void macro_parser(string macro);
};

int joy2chord::open_joystick()
{
        char device[256];
	char name[128];

        sprintf(device, "/dev/input/js%i", device_number);

        if (0 > (joy_fd = open(device, O_RDONLY))) 
	{
		cerr << " error opening device " << device << endl;
                sprintf(device, "/dev/js%i", device_number);

                if ((joy_fd = open(device, O_RDONLY)) < 0) 
		{
                       	cerr << "Error opening Device " << device << endl; 
                        exit(-3);
                }
        }

        if (0 > ioctl(joy_fd, JSIOCGAXES, &axes))
	{
		cerr << "Invalid Value from JSIOCGAXES" << endl;
	}
	if ( 0 > ioctl(joy_fd, JSIOCGBUTTONS, &controller_buttons))
	{
		cerr << "Invalid Value from JSIOCGBUTTONS" << endl;
	}
	if (controller_buttons > MAX_BUTTONS)
	{
		cerr << "controller_buttons is greater then " << MAX_BUTTONS << " This is likely an error, but if you need support for more buttons, recompile joy2chord with a higher value for MAX_BUTTONS" << endl;
	}
        if ((total_chorded_buttons + total_simple_buttons) > controller_buttons)
	{
		cerr << "More buttons (" << total_chorded_buttons << ") defined then controller supports (" << controller_buttons << ")" << endl;
	}
	if ( 0 > ioctl(joy_fd, JSIOCGNAME(128), name))
	{
		cerr << "Invalid Value from JSIOCGNAME" << endl;
	}

	device_name.assign(name);

	if (verbose)
	{
        	cout << "Using Joystick " << device_number << " ( " << device_name << ") through device " << device << " with " << total_axes << " axes and " << controller_buttons <<" chorded buttons." << endl;
	}

        return 0;
}

void joy2chord::ioctl_wrapper(int uinp_fd, int UI_SETBIT, int i)
{
	if ( -1 == (ioctl(uinp_fd, UI_SET_EVBIT, i)))
	{ 
		cerr << "ioctl Error: " << strerror(errno) << endl; 
		exit (-2);
	}

}

/* Setup the uinput device */
int joy2chord::setup_uinput_device()
{
	int i = 0;
        // Open the input device
        if ( -1 == (uinp_fd = open("/dev/misc/uinput", O_WRONLY | O_NDELAY)))
        {
        	if ( -1 == (uinp_fd = open("/dev/input/uinput", O_WRONLY | O_NDELAY)))
		{
        		if ( -1 ==  (uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY)))
			{
        			cerr << "Unable to open /dev/misc/uinput" << endl;
				cerr << "Unable to open /dev/input/uinput" << endl;
				cerr << "Unable to open /dev/uinput" << endl;
				cerr << "Unable to open any uinput device: " << strerror(errno) << endl;
				cerr << "Please make sure the uinput module is loaded in your kernel" << endl;
				exit(-2);
			}
		}
       	}
	
	memset(&uinp,0,sizeof(uinp)); // Intialize the uInput device to NULL
	strncpy(uinp.name, "Joystick to Keyboard/Mouse Converter", UINPUT_MAX_NAME_SIZE);
       	uinp.id.version = 4;
       	uinp.id.bustype = BUS_USB;
       	
	// Setup the uinput device
       	ioctl_wrapper(uinp_fd, UI_SET_EVBIT, EV_KEY);
	ioctl_wrapper(uinp_fd, UI_SET_EVBIT, EV_REL);
	ioctl_wrapper(uinp_fd, UI_SET_RELBIT, REL_X);
       	ioctl_wrapper(uinp_fd, UI_SET_RELBIT, REL_Y);

	// these parts need better error checker, or fix ioctrl_wrapper so it works with them
       	for (i=0; i < 256; i++) {
        	ioctl(uinp_fd, UI_SET_KEYBIT, i);
       	}
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TOUCH);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MOUSE);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_LEFT);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MIDDLE);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_RIGHT);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_FORWARD);
       	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_BACK);
       
       	/* Create input device into input sub-system */
       	write(uinp_fd, &uinp, sizeof(uinp));
       	int result = ioctl(uinp_fd, UI_DEV_CREATE);
       	if (result)
	{
        	cerr << "Unable to create UINPUT device. Error: " << strerror(errno) << endl;
		close(uinp_fd);
                exit (-2);
       	}
       	return 1;
}

int joy2chord::read_config(map<string,__u16>  & chordmap)
{
/*	// string manipulation example code
		ostringstream lbuffer;
		lbuffer << mode_loop;
		string tmpname = lbuffer.str();
		string filename = "joy2chord-mode" + tmpname;
		ConfigFile config(filename);
*/
	// these values come from /usr/include/linux/input.h
	chordmap["KEY_RESERVED"] = 0;
	chordmap["KEY_ESC"] = 1;
	chordmap["KEY_1"] = 2;
	chordmap["KEY_2"] = 3;
	chordmap["KEY_3"] = 4;
	chordmap["KEY_4"] = 5;
	chordmap["KEY_5"] = 6;
	chordmap["KEY_6"] = 7;
	chordmap["KEY_7"] = 8;
	chordmap["KEY_8"] = 9;
	chordmap["KEY_9"] = 10;
	chordmap["KEY_0"] = 11;
	chordmap["KEY_MINUS"] = 12;
	chordmap["KEY_EQUAL"] = 13;
	chordmap["KEY_BACKSPACE"] = 14;
	chordmap["KEY_TAB"] = 15;
	chordmap["KEY_Q"] = 16;
	chordmap["KEY_W"] = 17;
	chordmap["KEY_E"] = 18;
	chordmap["KEY_R"] = 19;
	chordmap["KEY_T"] = 20;
	chordmap["KEY_Y"] = 21;
	chordmap["KEY_U"] = 22;
	chordmap["KEY_I"] = 23;
	chordmap["KEY_O"] = 24;
	chordmap["KEY_P"] = 25;
	chordmap["KEY_LEFTBRACE"] = 26;
	chordmap["KEY_RIGHTBRACE"] = 27;
	chordmap["KEY_ENTER"] = 28;
	chordmap["KEY_LEFTCTRL"] = 29;
	chordmap["KEY_A"] = 30;
	chordmap["KEY_S"] = 31;
	chordmap["KEY_D"] = 32;
	chordmap["KEY_F"] = 33;
	chordmap["KEY_G"] = 34;
	chordmap["KEY_H"] = 35;
	chordmap["KEY_J"] = 36;
	chordmap["KEY_K"] = 37;
	chordmap["KEY_L"] = 38;
	chordmap["KEY_SEMICOLON"] = 39;
	chordmap["KEY_APOSTROPHE"] = 40;
	chordmap["KEY_GRAVE"] = 41;
	chordmap["KEY_LEFTSHIFT"] = 42;
	chordmap["KEY_BACKSLASH"] = 43;
	chordmap["KEY_Z"] = 44;
	chordmap["KEY_X"] = 45;
	chordmap["KEY_C"] = 46;
	chordmap["KEY_V"] = 47;
	chordmap["KEY_B"] = 48;
	chordmap["KEY_N"] = 49;
	chordmap["KEY_M"] = 50;
	chordmap["KEY_COMMA"] = 51;
	chordmap["KEY_DOT"] = 52;
	chordmap["KEY_SLASH"] = 53;
	chordmap["KEY_RIGHTSHIFT"] = 54;
	chordmap["KEY_KPASTERISK"] = 55;
	chordmap["KEY_LEFTALT"] = 56;
	chordmap["KEY_SPACE"] = 57;
	chordmap["KEY_CAPSLOCK"] = 58;
	chordmap["KEY_F1"] = 59;
	chordmap["KEY_F2"] = 60;
	chordmap["KEY_F3"] = 61;
	chordmap["KEY_F4"] = 62;
	chordmap["KEY_F5"] = 63;
	chordmap["KEY_F6"] = 64;
	chordmap["KEY_F7"] = 65;
	chordmap["KEY_F8"] = 66;
	chordmap["KEY_F9"] = 67;
	chordmap["KEY_F10"] = 68;
	chordmap["KEY_NUMLOCK"] = 69;
	chordmap["KEY_SCROLLLOCK"] = 70;
	chordmap["KEY_KP7"] = 71;
	chordmap["KEY_KP8"] = 72;
	chordmap["KEY_KP9"] = 73;
	chordmap["KEY_KPMINUS"] = 74;
	chordmap["KEY_KP4"] = 75;
	chordmap["KEY_KP5"] = 76;
	chordmap["KEY_KP6"] = 77;
	chordmap["KEY_KPPLUS"] = 78;
	chordmap["KEY_KP1"] = 79;
	chordmap["KEY_KP2"] = 80;
	chordmap["KEY_KP3"] = 81;
	chordmap["KEY_KP0"] = 82;
	chordmap["KEY_KPDOT"] = 83;
	chordmap["KEY_ZENKAKUHANKAKU"] = 85;
	chordmap["KEY_102ND"] = 86;
	chordmap["KEY_F11"] = 87;
	chordmap["KEY_F12"] = 88;
	chordmap["KEY_RO"] = 89;
	chordmap["KEY_KATAKANA"] = 90;
	chordmap["KEY_HIRAGANA"] = 91;
	chordmap["KEY_HENKAN"] = 92;
	chordmap["KEY_KATAKANAHIRAGANA"] = 93;
	chordmap["KEY_MUHENKAN"] = 94;
	chordmap["KEY_KPJPCOMMA"] = 95;
	chordmap["KEY_KPENTER"] = 96;
	chordmap["KEY_RIGHTCTRL"] = 97;
	chordmap["KEY_KPSLASH"] = 98;
	chordmap["KEY_SYSRQ"] = 99;
	chordmap["KEY_RIGHTALT"] = 100;
	chordmap["KEY_LINEFEED"] = 101;
	chordmap["KEY_HOME"] = 102;
	chordmap["KEY_UP"] = 103;
	chordmap["KEY_PAGEUP"] = 104;
	chordmap["KEY_LEFT"] = 105;
	chordmap["KEY_RIGHT"] = 106;
	chordmap["KEY_END"] = 107;
	chordmap["KEY_DOWN"] = 108;
	chordmap["KEY_PAGEDOWN"] = 109;
	chordmap["KEY_INSERT"] = 110;
	chordmap["KEY_DELETE"] = 111;
	chordmap["KEY_MACRO"] = 112;
	chordmap["KEY_MUTE"] = 113;
	chordmap["KEY_VOLUMEDOWN"] = 114;
	chordmap["KEY_VOLUMEUP"] = 115;
	chordmap["KEY_POWER"] = 116;
	chordmap["KEY_KPEQUAL"] = 117;
	chordmap["KEY_KPPLUSMINUS"] = 118;
	chordmap["KEY_PAUSE"] = 119;
	chordmap["KEY_KPCOMMA"] = 121;
	chordmap["KEY_HANGEUL"] = 122;
	chordmap["KEY_HANGUEL"] = KEY_HANGEUL;
	chordmap["KEY_HANJA"] = 123;
	chordmap["KEY_YEN"] = 124;
	chordmap["KEY_LEFTMETA"] = 125;
	chordmap["KEY_RIGHTMETA"] = 126;
	chordmap["KEY_COMPOSE"] = 127;
	chordmap["KEY_STOP"] = 128;
	chordmap["KEY_AGAIN"] = 129;
	chordmap["KEY_PROPS"] = 130;
	chordmap["KEY_UNDO"] = 131;
	chordmap["KEY_FRONT"] = 132;
	chordmap["KEY_COPY"] = 133;
	chordmap["KEY_OPEN"] = 134;
	chordmap["KEY_PASTE"] = 135;
	chordmap["KEY_FIND"] = 136;
	chordmap["KEY_CUT"] = 137;
	chordmap["KEY_HELP"] = 138;
	chordmap["KEY_MENU"] = 139;
	chordmap["KEY_CALC"] = 140;
	chordmap["KEY_SETUP"] = 141;
	chordmap["KEY_SLEEP"] = 142;
	chordmap["KEY_WAKEUP"] = 143;
	chordmap["KEY_FILE"] = 144;
	chordmap["KEY_SENDFILE"] = 145;
	chordmap["KEY_DELETEFILE"] = 146;
	chordmap["KEY_XFER"] = 147;
	chordmap["KEY_PROG1"] = 148;
	chordmap["KEY_PROG2"] = 149;
	chordmap["KEY_WWW"] = 150;
	chordmap["KEY_MSDOS"] = 151;
	chordmap["KEY_COFFEE"] = 152;
	chordmap["KEY_SCREENLOCK"] = KEY_COFFEE;
	chordmap["KEY_DIRECTION"] = 153;
	chordmap["KEY_CYCLEWINDOWS"] = 154;
	chordmap["KEY_MAIL"] = 155;
	chordmap["KEY_BOOKMARKS"] = 156;
	chordmap["KEY_COMPUTER"] = 157;
	chordmap["KEY_BACK"] = 158;
	chordmap["KEY_FORWARD"] = 159;
	chordmap["KEY_CLOSECD"] = 160;
	chordmap["KEY_EJECTCD"] = 161;
	chordmap["KEY_EJECTCLOSECD"] = 162;
	chordmap["KEY_NEXTSONG"] = 163;
	chordmap["KEY_PLAYPAUSE"] = 164;
	chordmap["KEY_PREVIOUSSONG"] = 165;
	chordmap["KEY_STOPCD"] = 166;
	chordmap["KEY_RECORD"] = 167;
	chordmap["KEY_REWIND"] = 168;
	chordmap["KEY_PHONE"] = 169;
	chordmap["KEY_ISO"] = 170;
	chordmap["KEY_CONFIG"] = 171;
	chordmap["KEY_HOMEPAGE"] = 172;
	chordmap["KEY_REFRESH"] = 173;
	chordmap["KEY_EXIT"] = 174;
	chordmap["KEY_MOVE"] = 175;
	chordmap["KEY_EDIT"] = 176;
	chordmap["KEY_SCROLLUP"] = 177;
	chordmap["KEY_SCROLLDOWN"] = 178;
	chordmap["KEY_KPLEFTPAREN"] = 179;
	chordmap["KEY_KPRIGHTPAREN"] = 180;
	chordmap["KEY_NEW"] = 181;
	chordmap["KEY_REDO"] = 182;
	chordmap["KEY_F13"] = 183;
	chordmap["KEY_F14"] = 184;
	chordmap["KEY_F15"] = 185;
	chordmap["KEY_F16"] = 186;
	chordmap["KEY_F17"] = 187;
	chordmap["KEY_F18"] = 188;
	chordmap["KEY_F19"] = 189;
	chordmap["KEY_F20"] = 190;
	chordmap["KEY_F21"] = 191;
	chordmap["KEY_F22"] = 192;
	chordmap["KEY_F23"] = 193;
	chordmap["KEY_F24"] = 194;
	chordmap["KEY_PLAYCD"] = 200;
	chordmap["KEY_PAUSECD"] = 201;
	chordmap["KEY_PROG3"] = 202;
	chordmap["KEY_PROG4"] = 203;
	chordmap["KEY_SUSPEND"] = 205;
	chordmap["KEY_CLOSE"] = 206;
	chordmap["KEY_PLAY"] = 207;
	chordmap["KEY_FASTFORWARD"] = 208;
	chordmap["KEY_BASSBOOST"] = 209;
	chordmap["KEY_PRINT"] = 210;
	chordmap["KEY_HP"] = 211;
	chordmap["KEY_CAMERA"] = 212;
	chordmap["KEY_SOUND"] = 213;
	chordmap["KEY_QUESTION"] = 214;
	chordmap["KEY_EMAIL"] = 215;
	chordmap["KEY_CHAT"] = 216;
	chordmap["KEY_SEARCH"] = 217;
	chordmap["KEY_CONNECT"] = 218;
	chordmap["KEY_FINANCE"] = 219;
	chordmap["KEY_SPORT"] = 220;
	chordmap["KEY_SHOP"] = 221;
	chordmap["KEY_ALTERASE"] = 222;
	chordmap["KEY_CANCEL"] = 223;
	chordmap["KEY_BRIGHTNESSDOWN"] = 224;
	chordmap["KEY_BRIGHTNESSUP"] = 225;
	chordmap["KEY_MEDIA"] = 226;
	chordmap["KEY_SWITCHVIDEOMODE"] = 227;
	chordmap["KEY_KBDILLUMTOGGLE"] = 228;
	chordmap["KEY_KBDILLUMDOWN"] = 229;
	chordmap["KEY_KBDILLUMUP"] = 230;
	chordmap["KEY_SEND"] = 231;
	chordmap["KEY_REPLY"] = 232;
	chordmap["KEY_FORWARDMAIL"] = 233;
	chordmap["KEY_SAVE"] = 234;
	chordmap["KEY_DOCUMENTS"] = 235;
	chordmap["KEY_BATTERY"] = 236;
	chordmap["KEY_BLUETOOTH"] = 237;
	chordmap["KEY_WLAN"] = 238;
	chordmap["KEY_UNKNOWN"] = 240;
	chordmap["KEY_VIDEO_NEXT"] = 241;
	chordmap["KEY_VIDEO_PREV"] = 242;
	chordmap["KEY_BRIGHTNESS_CYCLE"] = 243;
	chordmap["KEY_BRIGHTNESS_ZERO"] = 244;
	chordmap["KEY_DISPLAY_OFF"] = 245;
	chordmap["KEY_OK"] = 0x160;
	chordmap["KEY_SELECT"] = 0x161;
	chordmap["KEY_GOTO"] = 0x162;
	chordmap["KEY_CLEAR"] = 0x163;
	chordmap["KEY_POWER2"] = 0x164;
	chordmap["KEY_OPTION"] = 0x165;
	chordmap["KEY_INFO"] = 0x166;
	chordmap["KEY_TIME"] = 0x167;
	chordmap["KEY_VENDOR"] = 0x168;
	chordmap["KEY_ARCHIVE"] = 0x169;
	chordmap["KEY_PROGRAM"] = 0x16a;
	chordmap["KEY_CHANNEL"] = 0x16b;
	chordmap["KEY_FAVORITES"] = 0x16c;
	chordmap["KEY_EPG"] = 0x16d;
	chordmap["KEY_PVR"] = 0x16e;
	chordmap["KEY_MHP"] = 0x16f;
	chordmap["KEY_LANGUAGE"] = 0x170;
	chordmap["KEY_TITLE"] = 0x171;
	chordmap["KEY_SUBTITLE"] = 0x172;
	chordmap["KEY_ANGLE"] = 0x173;
	chordmap["KEY_ZOOM"] = 0x174;
	chordmap["KEY_MODE"] = 0x175;
	chordmap["KEY_KEYBOARD"] = 0x176;
	chordmap["KEY_SCREEN"] = 0x177;
	chordmap["KEY_PC"] = 0x178;
	chordmap["KEY_TV"] = 0x179;
	chordmap["KEY_TV2"] = 0x17a;
	chordmap["KEY_VCR"] = 0x17b;
	chordmap["KEY_VCR2"] = 0x17c;
	chordmap["KEY_SAT"] = 0x17d;
	chordmap["KEY_SAT2"] = 0x17e;
	chordmap["KEY_CD"] = 0x17f;
	chordmap["KEY_TAPE"] = 0x180;
	chordmap["KEY_RADIO"] = 0x181;
	chordmap["KEY_TUNER"] = 0x182;
	chordmap["KEY_PLAYER"] = 0x183;
	chordmap["KEY_TEXT"] = 0x184;
	chordmap["KEY_DVD"] = 0x185;
	chordmap["KEY_AUX"] = 0x186;
	chordmap["KEY_MP3"] = 0x187;
	chordmap["KEY_AUDIO"] = 0x188;
	chordmap["KEY_VIDEO"] = 0x189;
	chordmap["KEY_DIRECTORY"] = 0x18a;
	chordmap["KEY_LIST"] = 0x18b;
	chordmap["KEY_MEMO"] = 0x18c;
	chordmap["KEY_CALENDAR"] = 0x18d;
	chordmap["KEY_RED"] = 0x18e;
	chordmap["KEY_GREEN"] = 0x18f;
	chordmap["KEY_YELLOW"] = 0x190;
	chordmap["KEY_BLUE"] = 0x191;
	chordmap["KEY_CHANNELUP"] = 0x192;
	chordmap["KEY_CHANNELDOWN"] = 0x193;
	chordmap["KEY_FIRST"] = 0x194;
	chordmap["KEY_LAST"] = 0x195;
	chordmap["KEY_AB"] = 0x196;
	chordmap["KEY_NEXT"] = 0x197;
	chordmap["KEY_RESTART"] = 0x198;
	chordmap["KEY_SLOW"] = 0x199;
	chordmap["KEY_SHUFFLE"] = 0x19a;
	chordmap["KEY_BREAK"] = 0x19b;
	chordmap["KEY_PREVIOUS"] = 0x19c;
	chordmap["KEY_DIGITS"] = 0x19d;
	chordmap["KEY_TEEN"] = 0x19e;
	chordmap["KEY_TWEN"] = 0x19f;
	chordmap["KEY_VIDEOPHONE"] = 0x1a0;
	chordmap["KEY_GAMES"] = 0x1a1;
	chordmap["KEY_ZOOMIN"] = 0x1a2;
	chordmap["KEY_ZOOMOUT"] = 0x1a3;
	chordmap["KEY_ZOOMRESET"] = 0x1a4;
	chordmap["KEY_WORDPROCESSOR"] = 0x1a5;
	chordmap["KEY_EDITOR"] = 0x1a6;
	chordmap["KEY_SPREADSHEET"] = 0x1a7;
	chordmap["KEY_GRAPHICSEDITOR"] = 0x1a8;
	chordmap["KEY_PRESENTATION"] = 0x1a9;
	chordmap["KEY_DATABASE"] = 0x1aa;
	chordmap["KEY_NEWS"] = 0x1ab;
	chordmap["KEY_VOICEMAIL"] = 0x1ac;
	chordmap["KEY_ADDRESSBOOK"] = 0x1ad;
	chordmap["KEY_MESSENGER"] = 0x1ae;
	chordmap["KEY_DISPLAYTOGGLE"] = 0x1af;
	chordmap["KEY_DEL_EOL"] = 0x1c0;
	chordmap["KEY_DEL_EOS"] = 0x1c1;
	chordmap["KEY_INS_LINE"] = 0x1c2;
	chordmap["KEY_DEL_LINE"] = 0x1c3;
	chordmap["KEY_FN"] = 0x1d0;
	chordmap["KEY_FN_ESC"] = 0x1d1;
	chordmap["KEY_FN_F1"] = 0x1d2;
	chordmap["KEY_FN_F2"] = 0x1d3;
	chordmap["KEY_FN_F3"] = 0x1d4;
	chordmap["KEY_FN_F4"] = 0x1d5;
	chordmap["KEY_FN_F5"] = 0x1d6;
	chordmap["KEY_FN_F6"] = 0x1d7;
	chordmap["KEY_FN_F7"] = 0x1d8;
	chordmap["KEY_FN_F8"] = 0x1d9;
	chordmap["KEY_FN_F9"] = 0x1da;
	chordmap["KEY_FN_F10"] = 0x1db;
	chordmap["KEY_FN_F11"] = 0x1dc;
	chordmap["KEY_FN_F12"] = 0x1dd;
	chordmap["KEY_FN_1"] = 0x1de;
	chordmap["KEY_FN_2"] = 0x1df;
	chordmap["KEY_FN_D"] = 0x1e0;
	chordmap["KEY_FN_E"] = 0x1e1;
	chordmap["KEY_FN_F"] = 0x1e2;
	chordmap["KEY_FN_S"] = 0x1e3;
	chordmap["KEY_FN_B"] = 0x1e4;
	chordmap["KEY_BRL_DOT1"] = 0x1f1;
	chordmap["KEY_BRL_DOT2"] = 0x1f2;
	chordmap["KEY_BRL_DOT3"] = 0x1f3;
	chordmap["KEY_BRL_DOT4"] = 0x1f4;
	chordmap["KEY_BRL_DOT5"] = 0x1f5;
	chordmap["KEY_BRL_DOT6"] = 0x1f6;
	chordmap["KEY_BRL_DOT7"] = 0x1f7;
	chordmap["KEY_BRL_DOT8"] = 0x1f8;
	chordmap["KEY_BRL_DOT9"] = 0x1f9;
	chordmap["KEY_BRL_DOT10"] = 0x1fa;
	chordmap["KEY_MIN_INTERESTING"] = KEY_MUTE;
	chordmap["KEY_MAX"] = 0x1ff;
	chordmap["BTN_MISC"] = 0x100;
	chordmap["BTN_0"] = 0x100;
	chordmap["BTN_1"] = 0x101;
	chordmap["BTN_2"] = 0x102;
	chordmap["BTN_3"] = 0x103;
	chordmap["BTN_4"] = 0x104;
	chordmap["BTN_5"] = 0x105;
	chordmap["BTN_6"] = 0x106;
	chordmap["BTN_7"] = 0x107;
	chordmap["BTN_8"] = 0x108;
	chordmap["BTN_9"] = 0x109;
	chordmap["BTN_JOYSTICK"] = 0x120;
	chordmap["BTN_TRIGGER"] = 0x120;
	chordmap["BTN_THUMB"] = 0x121;
	chordmap["BTN_THUMB2"] = 0x122;
	chordmap["BTN_TOP"] = 0x123;
	chordmap["BTN_TOP2"] = 0x124;
	chordmap["BTN_PINKIE"] = 0x125;
	chordmap["BTN_BASE"] = 0x126;
	chordmap["BTN_BASE2"] = 0x127;
	chordmap["BTN_BASE3"] = 0x128;
	chordmap["BTN_BASE4"] = 0x129;
	chordmap["BTN_BASE5"] = 0x12a;
	chordmap["BTN_BASE6"] = 0x12b;
	chordmap["BTN_DEAD"] = 0x12f;
	chordmap["BTN_GAMEPAD"] = 0x130;
	chordmap["BTN_A"] = 0x130;
	chordmap["BTN_B"] = 0x131;
	chordmap["BTN_C"] = 0x132;
	chordmap["BTN_X"] = 0x133;
	chordmap["BTN_Y"] = 0x134;
	chordmap["BTN_Z"] = 0x135;
	chordmap["BTN_TL"] = 0x136;
	chordmap["BTN_TR"] = 0x137;
	chordmap["BTN_TL2"] = 0x138;
	chordmap["BTN_TR2"] = 0x139;
	chordmap["BTN_SELECT"] = 0x13a;
	chordmap["BTN_START"] = 0x13b;
	chordmap["BTN_MODE"] = 0x13c;
	chordmap["BTN_THUMBL"] = 0x13d;
	chordmap["BTN_THUMBR"] = 0x13e;
	chordmap["BTN_DIGI"] = 0x140;
	chordmap["BTN_TOOL_PEN"] = 0x140;
	chordmap["BTN_TOOL_RUBBER"] = 0x141;
	chordmap["BTN_TOOL_BRUSH"] = 0x142;
	chordmap["BTN_TOOL_PENCIL"] = 0x143;
	chordmap["BTN_TOOL_AIRBRUSH"] = 0x144;
	chordmap["BTN_TOOL_FINGER"] = 0x145;
	chordmap["BTN_TOOL_MOUSE"] = 0x146;
	chordmap["BTN_TOOL_LENS"] = 0x147;
	chordmap["BTN_TOUCH"] = 0x14a;
	chordmap["BTN_STYLUS"] = 0x14b;
	chordmap["BTN_STYLUS2"] = 0x14c;
	chordmap["BTN_TOOL_DOUBLETAP"] = 0x14d;
	chordmap["BTN_TOOL_TRIPLETAP"] = 0x14e;
	chordmap["BTN_WHEEL"] = 0x150;
	chordmap["BTN_GEAR_DOWN"] = 0x150;
	chordmap["BTN_GEAR_UP"] = 0x151;

	ConfigFile config (config_file);
	
	if (!(config.readInto(device_number, "jsdev")))
	{
		cerr << "Invalid entry for jsdev" << endl;
	}
	
	if (!(config.readInto(total_modes, "total_modes")))
	{
		cerr << "Invalid code for total_modes" << endl;
	}
	
	if (verbose)
	{
		cout << "Using " << config_file << " for configuration information" << endl;
	}
	
		if (!(config.readInto(total_simple_buttons, "total_simple_buttons"))) // we don't care about how many buttons the controller provides, only about how many we have values defined for
	{
		cerr << "Invalid entry for total_simple_buttons" << endl;
	}else{
		if (verbose)
		{
			cout << total_simple_buttons << " simple buttons defined by the config file" << endl; 
		}
	}

	for (int mode_loop = 1; mode_loop <= total_simple_buttons; mode_loop++)
	{	
		ostringstream lbuffer;
		lbuffer << mode_loop;
		string button_name = "simple_b" + lbuffer.str();
		if (!(config.readInto(simple_values[mode_loop], button_name)))
		{
			cerr << "Invalid code entered for " << button_name << " of: " << simple_values[mode_loop] << endl;
		}else{
			if (debug)
			{
				if (1 == mode_loop)
				{
					cout << "Simple Button Values: B" << lbuffer.str() << ": " << simple_values[mode_loop];
				}else{
					cout << " B" << lbuffer.str() << ": " << simple_values[mode_loop];
				}
				if (mode_loop == total_simple_buttons)
				{
					cout << endl;
				}
			}
		}
	}

	for (int mode_loop = 1; mode_loop <= total_modes; mode_loop++)
	{	
		//lbuffer.str() = "";
		ostringstream lbuffer;
		lbuffer << mode_loop;
		if (debug)
		{
			cout << "Mode " << lbuffer.str() << endl;
		}
		for (int key_loop = 1; key_loop <= total_simple_buttons; key_loop++)
		{// position 0 isn't used on key loop
			ostringstream tbuffer;
			tbuffer << key_loop;
			string itemname = lbuffer.str() + "simple" + tbuffer.str();
			string readvalue = "";
			if (!(config.readInto(readvalue,itemname)))
			{
				cerr << "Invalid entry for value: " << itemname << endl;
			}
			__u16 ukeyvalue = chordmap.find(readvalue)->second;
			simple_modes[mode_loop][key_loop] = ukeyvalue;
			//SOMETHING [mode_loop][key_loop] = readvalue;
			if ((debug) && (readvalue != ""))
			{ // only read valid entries
				cout << "Adding " << readvalue << "[" << ukeyvalue << "]" << " to simple[" << mode_loop << "][" << key_loop << "] " << endl;
			}
		}
	}

	if (debug)
	{
		cout << "Done Loading simple config values" << endl;	
	}
	if (!(config.readInto(total_macros, "total_macros")))
	{
		cerr << "Invalid code for total_macros" << endl;
	}
	if (!(config.readInto(total_modifiers, "total_modifiers")))
	{
		cerr << "Invalid code for total_modifiers" << endl;
	}else{
		for(int loadmacro = 1; loadmacro <= total_modifiers; loadmacro++)
		{
			ostringstream mbuffer;
			mbuffer << loadmacro;
			string current_modifier_code = mbuffer.str() + "modifier";	
			string tmpstring = "";
			if (!(config.readInto(tmpstring, current_modifier_code)))
			{
				cerr << " Invalid code for " << current_modifier_code << endl;
			}else{
				__u16 ukeyvalue = chordmap.find(tmpstring)->second;
				modifier[loadmacro] = ukeyvalue;
			}
		}
	}
	
	if (!(config.readInto(total_chorded_buttons, "total_chorded_buttons"))) // we don't care about how many buttons the controller provides, only about how many we have values defined for
	{
		cerr << "Invalid entry for total_chorded_buttons" << endl;
	}else{
		if (verbose)
		{
			cout << total_chorded_buttons << " chorded buttons defined by the config file" << endl; 
		}
	}

	string button_name;
	ostringstream buttonbuffer;
	for (int load_codes = 1; load_codes <= total_chorded_buttons; load_codes++)
	{
		button_name.empty();
		buttonbuffer.str("");
		buttonbuffer << load_codes;
		button_name = "chord_b" + buttonbuffer.str();
		if (!(config.readInto(chord_values[load_codes - 1], button_name)))
		{
			cerr << "Invalid code entered for " << button_name << " of: " << chord_values[load_codes] << endl;
		}
		if (debug)
			{
				if (1 == load_codes)
				{
					cout << "Chord Button Values: B" << buttonbuffer.str() << ": " << chord_values[load_codes];
				}else{
					cout << " B" << buttonbuffer.str() << ": " << chord_values[load_codes];
				}
				if (load_codes == total_chorded_buttons)
				{
					cout << endl;
				}
			}
		}
	
	if (debug)
	{
		cout << "Starting to load chorded config values from 0 to " << pow(2,total_chorded_buttons) << "(2^" << total_chorded_buttons << ")" << endl;
	}
	for (int mode_loop = 1; mode_loop <= total_modes; mode_loop++)
	{	
		ostringstream lbuffer;
		lbuffer << mode_loop;
		string current_mode_code = lbuffer.str() + "modecode";
		if (!(config.readInto(mode_code[mode_loop], current_mode_code )))
		{ // code used for changing modes
			cerr << "Invalid code for " << current_mode_code << endl;
		}
		if (debug)
		{
			cout << "Adding Mode " << mode_code[mode_loop] << " into " << current_mode_code << endl;
		}
		for (int key_loop = 1; key_loop < (pow(2,total_chorded_buttons)); key_loop++)
		{// position 0 isn't used on key loop
			ostringstream tbuffer;
			tbuffer << key_loop;
			string itemname = lbuffer.str() + "chord" + tbuffer.str();
			string readvalue = "";
			if (!(config.readInto(readvalue,itemname)))
			{
				cerr << "Invalid entry for value: " << itemname << endl;
			}
			__u16 ukeyvalue = chordmap.find(readvalue)->second;
			modes[mode_loop][key_loop] = ukeyvalue;
			if ((debug) && (readvalue != ""))
			{ // only read valid entries
				cout << "Adding " << readvalue << "[" << ukeyvalue << "]" << " to chorded[" << mode_loop << "][" << key_loop << "] " << endl;
			}
		}
	}
	if (debug)
	{
		cout << "Done Loading chorded config values" << endl;	
	}
}

void joy2chord::macro_parser( string macro)
{

}

void joy2chord::send_click_events() //not implemented yet
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

void joy2chord::send_key_down(__u16 key_code)
{
	// Report BUTTON CLICK - PRESS event
  	memset(&event, 0, sizeof(event));
       	gettimeofday(&event.time, NULL);
     	event.type = EV_KEY;
     	event.code = key_code;
      	event.value = 1;
      	write(uinp_fd, &event, sizeof(event));
      	event.type = EV_SYN;
      	event.code = SYN_REPORT;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
}

void joy2chord::send_key_up(__u16 key_code)
{
	// Report BUTTON CLICK - RELEASE event
        memset(&event, 0, sizeof(event));
        gettimeofday(&event.time, NULL);
      	event.type = EV_KEY;
      	event.code = key_code;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
      	event.type = EV_SYN;
      	event.code = SYN_REPORT;
      	event.value = 0;
      	write(uinp_fd, &event, sizeof(event));
}

int joy2chord::valid_key( string newkey){
	if (newkey.length() > 3)
	{
		string tempkey,t1,t2,t3;
		t1 = newkey[0];
		t2 = newkey[1];
		t3 = newkey[2];
		tempkey = t1 + t2 + t3;
		if (tempkey == "key")
		{
			newkey.erase(0,3);
			istringstream buffer(newkey);
			int keyvalue;
			buffer >> keyvalue;
			return keyvalue;
		}
	}
	return -1;
}

void joy2chord::main_loop(map<string,__u16> chordmap)
{
	struct js_event js;
	
	// make an initilization function for these?

	for (int cleararray = 0; cleararray <= MAX_BUTTONS; cleararray++){
		button_state[cleararray] = 0;
	}
	for (int cleararray = 0; cleararray <= MAX_BUTTONS; cleararray++){
		send_code[cleararray] = 0;
	}	

	for (int cleararray = 0; cleararray <= MAX_MODES; cleararray++){
		modecount[cleararray] = 0;
	}
	
	lastkey = KEY_RESERVED; // initilize the last key to nothing
	thiskey = KEY_RESERVED; // for tracking the current key

	justpressed = 0;

	while (1){
		if (read(joy_fd, &js, sizeof(struct js_event)) != sizeof(struct js_event)) 
		{
			perror(TOOL_NAME ": error reading from joystick device");
			exit (-5);
		}
		process_events(js);
	}//while
}

void joy2chord::process_events(js_event js)
{
	switch(js.type & ~JS_EVENT_INIT) 
	{
               	case JS_EVENT_BUTTON:
			if (js.value) 
			{ // if a button is pressed down remember its state until all buttons are released
				for ( int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
				{
					if (calibration)
						{
							printf("Pressed: %i\n",js.number);
						}
					if( js.number == chord_values[allbuttons])
					{
						
						button_state[allbuttons] = 1;
						send_code[allbuttons] = 1;
					}
				}		
				for (int allsimple = 1; allsimple <= total_simple_buttons; allsimple++)
				{
					if (simple_values[allsimple] == js.number)
					{
						if (verbose)
						{
							//cout << "Sending Down: " << // simple_values[allsimple];
						}
						send_key_down(simple_modes[mode][allsimple]);	
					}
				}
			}else{ // track when buttons are released
				for ( int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
				{
		
					{
						button_state[allbuttons] = 0;
					}
				}
				for (int allsimple = 1; allsimple <= total_simple_buttons; allsimple++)
				{
					if (simple_values[allsimple] == js.number)
					{
						if (verbose)
						{
							//cout << "Sending Up: " << //simple_values[allsimple];
						}
						send_key_up(simple_modes[mode][allsimple]);	
					}
				}
			}
			// sanity checker, to make sure no bad data get through
			for (int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
			{
				if (send_code[allbuttons] > MAX_BAD)
				{
					// this is a workaround for when bad values are input
					send_code[allbuttons] = 0;
				}
			}
			
			if (debug)
			{
				cout << "Button State: Chord: ";
				for (int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++){
					cout << allbuttons << ":" << send_code[allbuttons] << " ";
				}
				cout << endl;
			}
                        // this is the "key code" that is going to be sent
			button_code = 0;
			for (int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
			{
				if (allbuttons == 0 )
				{
					button_code += send_code[allbuttons];
				}
				else
				{
					button_code += (send_code[allbuttons] * pow(2,allbuttons));
				}
			}
			thiskey = modes[mode][button_code];	
			int clear = 0;
			if (0 == button_code)
			{
				clear++;
			}
			for ( int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
			{
				if (button_state[allbuttons] != 0)
				{
					clear++;
				}
			}
			if (clear == 0)
			{ // if all buttons are released then send the code and clear everything
				for (int allbuttons = 0; allbuttons < total_chorded_buttons; allbuttons++)
				{
					send_code[allbuttons] = 0;
				}
				for (int macro_loop = 0; macro_loop < total_macros; macro_loop++)
				{
					if (button_code == macro_values[macro_loop])
					{ // use a seprate loop for macros and modes
						if (verbose)
						{
							cout << "Macro " << macro_loop  << endl;
						}
						// mode = 2;
						// mode2count++;
						for (int cleararray = 0; cleararray <= MAX_MODES; cleararray++)
						{
							modecount[cleararray] = 0;
						}
						/* mode3count = 0;
						if (mode2count == 2)
						{
							send_key_down(KEY_LEFTCTRL);
							send_key_down(KEY_LEFTALT);
							send_key_down(KEY_S);
							send_key_up(KEY_S);
							mode = 3;
						}
						if (mode2count == 3)
						{
							send_key_up(KEY_LEFTALT);
							send_key_up(KEY_LEFTCTRL);
							mode2count = 0;
							mode = 1;
						}	*/
					}
				}				
				for (int mode_loop = 0; mode_loop < MAX_MODES; mode_loop++)
				{
					if( mode_code[mode_loop] == button_code)
					{
						if (verbose)
						{
							cout << "Mode changed to " << mode_loop << endl;
						}
						mode = mode_loop;
					}
				}
				justpressed = 0;
				int senddown = 0;
				for (int allbuttons = 0; allbuttons < total_modes; allbuttons++)
				{
					// if no mode button is pressed down, and key is not defined as KEY_RESERVED
					if(( button_code == mode_code[allbuttons]) || (button_code == KEY_RESERVED ))
					{
						senddown++;
					}
				}
				if (0 == senddown)
				{
					if (debug)
					{
						cout << "Sending Mode[" << mode << "] Down Code: " <<  button_code << endl;
					}
					send_key_down(thiskey);	
					lastkey = thiskey;
				}
					for (int mbuttons = 1; mbuttons <= total_modifiers; mbuttons++)
				{
					if (thiskey == modifier[mbuttons])
					{
						modifier_state[mbuttons] = 1;
						justpressed = 1;
						if (debug)
						{
							cout << "Set Modifier State " << mbuttons << endl;
						}
					}
				}
				int sendup = 0;
				for (int allbuttons = 0; allbuttons < total_modes; allbuttons++)
				{
					// if no mode button is pressed down, and key is not defined as KEY_RESERVED, and button was just pressed
					if((( button_code == mode_code[allbuttons]) || (button_code == KEY_RESERVED )) && (0 == justpressed))
					{
						sendup++;
					}
					if (1 == justpressed) 
					{
						sendup++;
					}
				}
				if (0 == sendup)
				{
					if (debug)
					{
						cout << "Sending Mode[" << mode << "] Up Code: " <<  button_code << endl;
					}
					send_key_up(thiskey);	
					
					for (int mbuttons = 1; mbuttons <= total_modifiers; mbuttons++)
					{
						if (1 == modifier_state[mbuttons])
						{
							if (debug)
							{
								cout << "Cleared Modifier State " << mbuttons << endl;
							}
							modifier_state[mbuttons] = 0;
							send_key_up(modifier[mbuttons]);
						}
					}
				}	

				int clearl;
				for (clearl=0; clearl < total_chorded_buttons; clearl++){
					send_code[clearl] = 0;
				}
			}
                        break;
	}
}

int main( int argc, char *argv[])
{
	int c;
	extern char *optarg;
	
	int init_device_number = 0;
	string init_config_file = "joy2chord-config";
	int setverbose = 0;
	int setdebug = 0;
	int setcalibration = 0;
	
	while ((c = getopt(argc, argv, "hvdbc:j:")) != -1){
		switch (c){
			case 'h':
				cout << "Useage: " << TOOL_NAME << " -d -v -b -c [keymap_file] -j [joystick_number]" << endl;
				cout << "	-d Enable Debug output" << endl;
				cout << "	-v Enable Verbose output" << endl;
				cout << "	-b Enable Calibration output" << endl;
				cout << "	-c Specify a keymap file to use" << endl;
				cout << "	-j Specify the joystick number to use" << endl;
				exit(-2);
				break;
			case 'd':
				cout << "debug messages enabled" << endl;
				setdebug = 1; 
				break;
			case 'v':
				cout << "verbose messages enabled" << endl;
				setverbose = 1; 
				break;
			case 'c':
				init_config_file = optarg; 
				break;			
			case 'b':
				setcalibration = 1;
				break;
			case 'j':
				init_device_number = atoi (optarg);
				cout << "joystick number set to " << init_device_number << endl;
				break;
		}
	}

	map<string,__u16> chordmap;
	
	joy2chord myjoy;

	// these should happen in the constructor
	myjoy.total_modes = 0;
	myjoy.config_file = init_config_file;
	myjoy.axes = 0;
	myjoy.total_simple_buttons = 0;
	myjoy.total_chorded_buttons = 0;
	myjoy.mode = 1;
	myjoy.calibration = setcalibration;
	myjoy.debug = setdebug;
	myjoy.verbose = setverbose;
	for (int allmodifier = 0; allmodifier < MAX_MODIFIERS; allmodifier++)
	{
		myjoy.modifier[allmodifier] = 0;
	}
        
	myjoy.setup_uinput_device();
	myjoy.read_config(chordmap);
	myjoy.device_number = init_device_number; // once the device number is pulled from the config file, store it with the other information in the class
	myjoy.open_joystick();

	myjoy.main_loop(chordmap);
	//send_click_events();           // Send mouse event
        /* Destroy the input device */
	int destroy = ioctl(uinp_fd, UI_DEV_DESTROY);
       	if (destroy)
	{
        	cerr << "Unable to destroy UINPUT device. Error: " << strerror(errno) << endl;
                exit (-2);
       	}
        /* Close the UINPUT device */
        close(uinp_fd);
}
