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
#include <sstream>
#include <map>

#include "ConfigFile.h" // from http://www-personal.umich.edu/~wagnerr/ConfigFile.html

/* Globals */

#define TOOL_NAME "joy2chord"

using namespace std;

static int uinp_fd = -1;
struct uinput_user_dev uinp;       // uInput device structure
struct input_event event; // Input device structure
int axes;
int joy_fd;
int buttons;
int verbose = 0;
int debug = 0;
int maxbad = 40000;
int total_modes = 4; // 4 modes are used because 0 is ignored, and 1-3 are used
__u16 modes[4][64]; //complains when not constant
int joy_values[16] = {4,6,9,3,5,7,0,0,0,0,0,0,0,0,0,0}; // if no config file values are used these will be used, use these to hard code values in

int open_joystick(int default_joystick)
{

        char device[256];
        char name[128] = "Unknown";

        sprintf(device, "/dev/input/js%i", default_joystick);

        if ((joy_fd = open(device, O_RDONLY)) < 0) {
                fprintf(stderr, "%s: ", TOOL_NAME); perror(device);
                sprintf(device, "/dev/js%i", default_joystick);

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
        int i = 0;
        // Open the input device
        uinp_fd = open("/dev/misc/uinput", O_WRONLY | O_NDELAY);
        if (!uinp_fd)
        {
        	cout << "Unable to open /dev/misc/uinput" << endl;
        	uinp_fd = open("/dev/input/uinput", O_WRONLY | O_NDELAY);
		if (!uinp_fd){
			cout << "Unable to open /dev/input/uinput" << endl;
        			uinp_fd = open("/dev/uinput", O_WRONLY | O_NDELAY);
				if (!uinp_fd){
					cout << "Unable to open /dev/uinput" << endl;
					cout << "Unable to open any uinput device." << endl;
					cout << "Please make sure the uinput module is loaded in your kernel" << endl;
					exit(-2);
				}
		}
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
        	cout << "Unable to create UINPUT device." << endl;
                exit (-2);
       	}
       	return 1;
}

int read_config(string configfile, map<string,__u16>  & mymap, int default_joystick){
/*	
		ostringstream lbuffer;
		lbuffer << mode_loop;
		string tmpname = lbuffer.str();
		string filename = "joy2chord-mode" + tmpname;
		ConfigFile config(filename);
*/
	mymap["KEY_RESERVED"] = 0;
	mymap["KEY_ESC"] = 1;
	mymap["KEY_1"] = 2;
	mymap["KEY_2"] = 3;
	mymap["KEY_3"] = 4;
	mymap["KEY_4"] = 5;
	mymap["KEY_5"] = 6;
	mymap["KEY_6"] = 7;
	mymap["KEY_7"] = 8;
	mymap["KEY_8"] = 9;
	mymap["KEY_9"] = 10;
	mymap["KEY_0"] = 11;
	mymap["KEY_MINUS"] = 12;
	mymap["KEY_EQUAL"] = 13;
	mymap["KEY_BACKSPACE"] = 14;
	mymap["KEY_TAB"] = 15;
	mymap["KEY_Q"] = 16;
	mymap["KEY_W"] = 17;
	mymap["KEY_E"] = 18;
	mymap["KEY_R"] = 19;
	mymap["KEY_T"] = 20;
	mymap["KEY_Y"] = 21;
	mymap["KEY_U"] = 22;
	mymap["KEY_I"] = 23;
	mymap["KEY_O"] = 24;
	mymap["KEY_P"] = 25;
	mymap["KEY_LEFTBRACE"] = 26;
	mymap["KEY_RIGHTBRACE"] = 27;
	mymap["KEY_ENTER"] = 28;
	mymap["KEY_LEFTCTRL"] = 29;
	mymap["KEY_A"] = 30;
	mymap["KEY_S"] = 31;
	mymap["KEY_D"] = 32;
	mymap["KEY_F"] = 33;
	mymap["KEY_G"] = 34;
	mymap["KEY_H"] = 35;
	mymap["KEY_J"] = 36;
	mymap["KEY_K"] = 37;
	mymap["KEY_L"] = 38;
	mymap["KEY_SEMICOLON"] = 39;
	mymap["KEY_APOSTROPHE"] = 40;
	mymap["KEY_GRAVE"] = 41;
	mymap["KEY_LEFTSHIFT"] = 42;
	mymap["KEY_BACKSLASH"] = 43;
	mymap["KEY_Z"] = 44;
	mymap["KEY_X"] = 45;
	mymap["KEY_C"] = 46;
	mymap["KEY_V"] = 47;
	mymap["KEY_B"] = 48;
	mymap["KEY_N"] = 49;
	mymap["KEY_M"] = 50;
	mymap["KEY_COMMA"] = 51;
	mymap["KEY_DOT"] = 52;
	mymap["KEY_SLASH"] = 53;
	mymap["KEY_RIGHTSHIFT"] = 54;
	mymap["KEY_KPASTERISK"] = 55;
	mymap["KEY_LEFTALT"] = 56;
	mymap["KEY_SPACE"] = 57;
	mymap["KEY_CAPSLOCK"] = 58;
	mymap["KEY_F1"] = 59;
	mymap["KEY_F2"] = 60;
	mymap["KEY_F3"] = 61;
	mymap["KEY_F4"] = 62;
	mymap["KEY_F5"] = 63;
	mymap["KEY_F6"] = 64;
	mymap["KEY_F7"] = 65;
	mymap["KEY_F8"] = 66;
	mymap["KEY_F9"] = 67;
	mymap["KEY_F10"] = 68;
	mymap["KEY_NUMLOCK"] = 69;
	mymap["KEY_SCROLLLOCK"] = 70;
	mymap["KEY_KP7"] = 71;
	mymap["KEY_KP8"] = 72;
	mymap["KEY_KP9"] = 73;
	mymap["KEY_KPMINUS"] = 74;
	mymap["KEY_KP4"] = 75;
	mymap["KEY_KP5"] = 76;
	mymap["KEY_KP6"] = 77;
	mymap["KEY_KPPLUS"] = 78;
	mymap["KEY_KP1"] = 79;
	mymap["KEY_KP2"] = 80;
	mymap["KEY_KP3"] = 81;
	mymap["KEY_KP0"] = 82;
	mymap["KEY_KPDOT"] = 83;
	mymap["KEY_ZENKAKUHANKAKU"] = 85;
	mymap["KEY_102ND"] = 86;
	mymap["KEY_F11"] = 87;
	mymap["KEY_F12"] = 88;
	mymap["KEY_RO"] = 89;
	mymap["KEY_KATAKANA"] = 90;
	mymap["KEY_HIRAGANA"] = 91;
	mymap["KEY_HENKAN"] = 92;
	mymap["KEY_KATAKANAHIRAGANA"] = 93;
	mymap["KEY_MUHENKAN"] = 94;
	mymap["KEY_KPJPCOMMA"] = 95;
	mymap["KEY_KPENTER"] = 96;
	mymap["KEY_RIGHTCTRL"] = 97;
	mymap["KEY_KPSLASH"] = 98;
	mymap["KEY_SYSRQ"] = 99;
	mymap["KEY_RIGHTALT"] = 100;
	mymap["KEY_LINEFEED"] = 101;
	mymap["KEY_HOME"] = 102;
	mymap["KEY_UP"] = 103;
	mymap["KEY_PAGEUP"] = 104;
	mymap["KEY_LEFT"] = 105;
	mymap["KEY_RIGHT"] = 106;
	mymap["KEY_END"] = 107;
	mymap["KEY_DOWN"] = 108;
	mymap["KEY_PAGEDOWN"] = 109;
	mymap["KEY_INSERT"] = 110;
	mymap["KEY_DELETE"] = 111;
	mymap["KEY_MACRO"] = 112;
	mymap["KEY_MUTE"] = 113;
	mymap["KEY_VOLUMEDOWN"] = 114;
	mymap["KEY_VOLUMEUP"] = 115;
	mymap["KEY_POWER"] = 116;
	mymap["KEY_KPEQUAL"] = 117;
	mymap["KEY_KPPLUSMINUS"] = 118;
	mymap["KEY_PAUSE"] = 119;
	mymap["KEY_KPCOMMA"] = 121;
	mymap["KEY_HANGEUL"] = 122;
	mymap["KEY_HANGUEL"] = KEY_HANGEUL;
	mymap["KEY_HANJA"] = 123;
	mymap["KEY_YEN"] = 124;
	mymap["KEY_LEFTMETA"] = 125;
	mymap["KEY_RIGHTMETA"] = 126;
	mymap["KEY_COMPOSE"] = 127;
	mymap["KEY_STOP"] = 128;
	mymap["KEY_AGAIN"] = 129;
	mymap["KEY_PROPS"] = 130;
	mymap["KEY_UNDO"] = 131;
	mymap["KEY_FRONT"] = 132;
	mymap["KEY_COPY"] = 133;
	mymap["KEY_OPEN"] = 134;
	mymap["KEY_PASTE"] = 135;
	mymap["KEY_FIND"] = 136;
	mymap["KEY_CUT"] = 137;
	mymap["KEY_HELP"] = 138;
	mymap["KEY_MENU"] = 139;
	mymap["KEY_CALC"] = 140;
	mymap["KEY_SETUP"] = 141;
	mymap["KEY_SLEEP"] = 142;
	mymap["KEY_WAKEUP"] = 143;
	mymap["KEY_FILE"] = 144;
	mymap["KEY_SENDFILE"] = 145;
	mymap["KEY_DELETEFILE"] = 146;
	mymap["KEY_XFER"] = 147;
	mymap["KEY_PROG1"] = 148;
	mymap["KEY_PROG2"] = 149;
	mymap["KEY_WWW"] = 150;
	mymap["KEY_MSDOS"] = 151;
	mymap["KEY_COFFEE"] = 152;
	mymap["KEY_SCREENLOCK"] = KEY_COFFEE;
	mymap["KEY_DIRECTION"] = 153;
	mymap["KEY_CYCLEWINDOWS"] = 154;
	mymap["KEY_MAIL"] = 155;
	mymap["KEY_BOOKMARKS"] = 156;
	mymap["KEY_COMPUTER"] = 157;
	mymap["KEY_BACK"] = 158;
	mymap["KEY_FORWARD"] = 159;
	mymap["KEY_CLOSECD"] = 160;
	mymap["KEY_EJECTCD"] = 161;
	mymap["KEY_EJECTCLOSECD"] = 162;
	mymap["KEY_NEXTSONG"] = 163;
	mymap["KEY_PLAYPAUSE"] = 164;
	mymap["KEY_PREVIOUSSONG"] = 165;
	mymap["KEY_STOPCD"] = 166;
	mymap["KEY_RECORD"] = 167;
	mymap["KEY_REWIND"] = 168;
	mymap["KEY_PHONE"] = 169;
	mymap["KEY_ISO"] = 170;
	mymap["KEY_CONFIG"] = 171;
	mymap["KEY_HOMEPAGE"] = 172;
	mymap["KEY_REFRESH"] = 173;
	mymap["KEY_EXIT"] = 174;
	mymap["KEY_MOVE"] = 175;
	mymap["KEY_EDIT"] = 176;
	mymap["KEY_SCROLLUP"] = 177;
	mymap["KEY_SCROLLDOWN"] = 178;
	mymap["KEY_KPLEFTPAREN"] = 179;
	mymap["KEY_KPRIGHTPAREN"] = 180;
	mymap["KEY_NEW"] = 181;
	mymap["KEY_REDO"] = 182;
	mymap["KEY_F13"] = 183;
	mymap["KEY_F14"] = 184;
	mymap["KEY_F15"] = 185;
	mymap["KEY_F16"] = 186;
	mymap["KEY_F17"] = 187;
	mymap["KEY_F18"] = 188;
	mymap["KEY_F19"] = 189;
	mymap["KEY_F20"] = 190;
	mymap["KEY_F21"] = 191;
	mymap["KEY_F22"] = 192;
	mymap["KEY_F23"] = 193;
	mymap["KEY_F24"] = 194;
	mymap["KEY_PLAYCD"] = 200;
	mymap["KEY_PAUSECD"] = 201;
	mymap["KEY_PROG3"] = 202;
	mymap["KEY_PROG4"] = 203;
	mymap["KEY_SUSPEND"] = 205;
	mymap["KEY_CLOSE"] = 206;
	mymap["KEY_PLAY"] = 207;
	mymap["KEY_FASTFORWARD"] = 208;
	mymap["KEY_BASSBOOST"] = 209;
	mymap["KEY_PRINT"] = 210;
	mymap["KEY_HP"] = 211;
	mymap["KEY_CAMERA"] = 212;
	mymap["KEY_SOUND"] = 213;
	mymap["KEY_QUESTION"] = 214;
	mymap["KEY_EMAIL"] = 215;
	mymap["KEY_CHAT"] = 216;
	mymap["KEY_SEARCH"] = 217;
	mymap["KEY_CONNECT"] = 218;
	mymap["KEY_FINANCE"] = 219;
	mymap["KEY_SPORT"] = 220;
	mymap["KEY_SHOP"] = 221;
	mymap["KEY_ALTERASE"] = 222;
	mymap["KEY_CANCEL"] = 223;
	mymap["KEY_BRIGHTNESSDOWN"] = 224;
	mymap["KEY_BRIGHTNESSUP"] = 225;
	mymap["KEY_MEDIA"] = 226;
	mymap["KEY_SWITCHVIDEOMODE"] = 227;
	mymap["KEY_KBDILLUMTOGGLE"] = 228;
	mymap["KEY_KBDILLUMDOWN"] = 229;
	mymap["KEY_KBDILLUMUP"] = 230;
	mymap["KEY_SEND"] = 231;
	mymap["KEY_REPLY"] = 232;
	mymap["KEY_FORWARDMAIL"] = 233;
	mymap["KEY_SAVE"] = 234;
	mymap["KEY_DOCUMENTS"] = 235;
	mymap["KEY_BATTERY"] = 236;
	mymap["KEY_BLUETOOTH"] = 237;
	mymap["KEY_WLAN"] = 238;
	mymap["KEY_UNKNOWN"] = 240;
	mymap["KEY_VIDEO_NEXT"] = 241;
	mymap["KEY_VIDEO_PREV"] = 242;
	mymap["KEY_BRIGHTNESS_CYCLE"] = 243;
	mymap["KEY_BRIGHTNESS_ZERO"] = 244;
	mymap["KEY_DISPLAY_OFF"] = 245;
	mymap["KEY_OK"] = 0x160;
	mymap["KEY_SELECT"] = 0x161;
	mymap["KEY_GOTO"] = 0x162;
	mymap["KEY_CLEAR"] = 0x163;
	mymap["KEY_POWER2"] = 0x164;
	mymap["KEY_OPTION"] = 0x165;
	mymap["KEY_INFO"] = 0x166;
	mymap["KEY_TIME"] = 0x167;
	mymap["KEY_VENDOR"] = 0x168;
	mymap["KEY_ARCHIVE"] = 0x169;
	mymap["KEY_PROGRAM"] = 0x16a;
	mymap["KEY_CHANNEL"] = 0x16b;
	mymap["KEY_FAVORITES"] = 0x16c;
	mymap["KEY_EPG"] = 0x16d;
	mymap["KEY_PVR"] = 0x16e;
	mymap["KEY_MHP"] = 0x16f;
	mymap["KEY_LANGUAGE"] = 0x170;
	mymap["KEY_TITLE"] = 0x171;
	mymap["KEY_SUBTITLE"] = 0x172;
	mymap["KEY_ANGLE"] = 0x173;
	mymap["KEY_ZOOM"] = 0x174;
	mymap["KEY_MODE"] = 0x175;
	mymap["KEY_KEYBOARD"] = 0x176;
	mymap["KEY_SCREEN"] = 0x177;
	mymap["KEY_PC"] = 0x178;
	mymap["KEY_TV"] = 0x179;
	mymap["KEY_TV2"] = 0x17a;
	mymap["KEY_VCR"] = 0x17b;
	mymap["KEY_VCR2"] = 0x17c;
	mymap["KEY_SAT"] = 0x17d;
	mymap["KEY_SAT2"] = 0x17e;
	mymap["KEY_CD"] = 0x17f;
	mymap["KEY_TAPE"] = 0x180;
	mymap["KEY_RADIO"] = 0x181;
	mymap["KEY_TUNER"] = 0x182;
	mymap["KEY_PLAYER"] = 0x183;
	mymap["KEY_TEXT"] = 0x184;
	mymap["KEY_DVD"] = 0x185;
	mymap["KEY_AUX"] = 0x186;
	mymap["KEY_MP3"] = 0x187;
	mymap["KEY_AUDIO"] = 0x188;
	mymap["KEY_VIDEO"] = 0x189;
	mymap["KEY_DIRECTORY"] = 0x18a;
	mymap["KEY_LIST"] = 0x18b;
	mymap["KEY_MEMO"] = 0x18c;
	mymap["KEY_CALENDAR"] = 0x18d;
	mymap["KEY_RED"] = 0x18e;
	mymap["KEY_GREEN"] = 0x18f;
	mymap["KEY_YELLOW"] = 0x190;
	mymap["KEY_BLUE"] = 0x191;
	mymap["KEY_CHANNELUP"] = 0x192;
	mymap["KEY_CHANNELDOWN"] = 0x193;
	mymap["KEY_FIRST"] = 0x194;
	mymap["KEY_LAST"] = 0x195;
	mymap["KEY_AB"] = 0x196;
	mymap["KEY_NEXT"] = 0x197;
	mymap["KEY_RESTART"] = 0x198;
	mymap["KEY_SLOW"] = 0x199;
	mymap["KEY_SHUFFLE"] = 0x19a;
	mymap["KEY_BREAK"] = 0x19b;
	mymap["KEY_PREVIOUS"] = 0x19c;
	mymap["KEY_DIGITS"] = 0x19d;
	mymap["KEY_TEEN"] = 0x19e;
	mymap["KEY_TWEN"] = 0x19f;
	mymap["KEY_VIDEOPHONE"] = 0x1a0;
	mymap["KEY_GAMES"] = 0x1a1;
	mymap["KEY_ZOOMIN"] = 0x1a2;
	mymap["KEY_ZOOMOUT"] = 0x1a3;
	mymap["KEY_ZOOMRESET"] = 0x1a4;
	mymap["KEY_WORDPROCESSOR"] = 0x1a5;
	mymap["KEY_EDITOR"] = 0x1a6;
	mymap["KEY_SPREADSHEET"] = 0x1a7;
	mymap["KEY_GRAPHICSEDITOR"] = 0x1a8;
	mymap["KEY_PRESENTATION"] = 0x1a9;
	mymap["KEY_DATABASE"] = 0x1aa;
	mymap["KEY_NEWS"] = 0x1ab;
	mymap["KEY_VOICEMAIL"] = 0x1ac;
	mymap["KEY_ADDRESSBOOK"] = 0x1ad;
	mymap["KEY_MESSENGER"] = 0x1ae;
	mymap["KEY_DISPLAYTOGGLE"] = 0x1af;
	mymap["KEY_DEL_EOL"] = 0x1c0;
	mymap["KEY_DEL_EOS"] = 0x1c1;
	mymap["KEY_INS_LINE"] = 0x1c2;
	mymap["KEY_DEL_LINE"] = 0x1c3;
	mymap["KEY_FN"] = 0x1d0;
	mymap["KEY_FN_ESC"] = 0x1d1;
	mymap["KEY_FN_F1"] = 0x1d2;
	mymap["KEY_FN_F2"] = 0x1d3;
	mymap["KEY_FN_F3"] = 0x1d4;
	mymap["KEY_FN_F4"] = 0x1d5;
	mymap["KEY_FN_F5"] = 0x1d6;
	mymap["KEY_FN_F6"] = 0x1d7;
	mymap["KEY_FN_F7"] = 0x1d8;
	mymap["KEY_FN_F8"] = 0x1d9;
	mymap["KEY_FN_F9"] = 0x1da;
	mymap["KEY_FN_F10"] = 0x1db;
	mymap["KEY_FN_F11"] = 0x1dc;
	mymap["KEY_FN_F12"] = 0x1dd;
	mymap["KEY_FN_1"] = 0x1de;
	mymap["KEY_FN_2"] = 0x1df;
	mymap["KEY_FN_D"] = 0x1e0;
	mymap["KEY_FN_E"] = 0x1e1;
	mymap["KEY_FN_F"] = 0x1e2;
	mymap["KEY_FN_S"] = 0x1e3;
	mymap["KEY_FN_B"] = 0x1e4;
	mymap["KEY_BRL_DOT1"] = 0x1f1;
	mymap["KEY_BRL_DOT2"] = 0x1f2;
	mymap["KEY_BRL_DOT3"] = 0x1f3;
	mymap["KEY_BRL_DOT4"] = 0x1f4;
	mymap["KEY_BRL_DOT5"] = 0x1f5;
	mymap["KEY_BRL_DOT6"] = 0x1f6;
	mymap["KEY_BRL_DOT7"] = 0x1f7;
	mymap["KEY_BRL_DOT8"] = 0x1f8;
	mymap["KEY_BRL_DOT9"] = 0x1f9;
	mymap["KEY_BRL_DOT10"] = 0x1fa;
	mymap["KEY_MIN_INTERESTING"] = KEY_MUTE;
	mymap["KEY_MAX"] = 0x1ff;
	
	ConfigFile config (configfile);
	config.readInto(default_joystick, "jsdev");
	config.readInto(joy_values[0], "joy_b0");
	config.readInto(joy_values[1], "joy_b1");
	config.readInto(joy_values[2], "joy_b2");
	config.readInto(joy_values[3], "joy_b3");
	config.readInto(joy_values[4], "joy_b4");
	config.readInto(joy_values[5], "joy_b5");
	// cout << "Using Joystick number " << default_joystick << endl;
	// y
	for (int mode_loop = 1; mode_loop < total_modes; mode_loop++){	
		ostringstream lbuffer;
		lbuffer << mode_loop;
		for (int key_loop = 1; key_loop < (64 * (total_modes - 1)); key_loop++){ 
			// (64 * (total_modes - 1) means 64 combinations per mode, and modes 0 isn't counted so subtract one
			ostringstream tbuffer;
			tbuffer << key_loop;
			string itemname = lbuffer.str() + "key" + tbuffer.str();
			string readvalue = "";
			config.readInto(readvalue,itemname);
			__u16 ukeyvalue = mymap.find(readvalue)->second;
			if ((debug) && (readvalue != "" )){ // only read valid entries
				cout << "adding " << readvalue << " as " << ukeyvalue;
			}
			modes[mode_loop][key_loop] = ukeyvalue;
			if ((debug) && (readvalue != "")){ // only read valid entries
				cout << " input: "<< itemname << " of mode[" << mode_loop << "][" << key_loop << "] " << modes[mode_loop][key_loop] << endl;
			}
		}
	}

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

int valid_key( string newkey){
	if (newkey.length() > 3){
		string tempkey,t1,t2,t3;
		t1 = newkey[0];
		t2 = newkey[1];
		t3 = newkey[2];
		tempkey = t1 + t2 + t3;
		if (tempkey == "key"){
			newkey.erase(0,3);
			istringstream buffer(newkey);
			int keyvalue;
			buffer >> keyvalue;
			return keyvalue;
		}
	}
	return -1;
}

void main_loop(map<string,__u16> mymap)
{
	struct js_event js;


	int button_state[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int send_code[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int button_code = 0;
	int mode = 1;
	int mode1count = 0;
	int mode2count = 0;
	int mode3count = 0;
	int doubleclick = 0;
	int oldmode = 1;

	int mode1_code = 31; // this key combination (thumb 1 + 4 finger buttons changes to mode 1
	int mode2_code = 47; // 47 this key combination ( thumb 2 + 4 finger buttons changes to mode 2
	int mode3_code = 63; // 63 this key combination ( thumb 1 & 2 + 4 finger buttons changes to mode 3
	__u16 lastkey = KEY_RESERVED; // initilize the last key to nothing
	__u16 thiskey = KEY_RESERVED; // for tracking the current key

	int norelease = 0;
	int justpressed = 0;
	int modifier_state[4] = {0,0,0,0};

	__u16 modifier[4];
	modifier[0] = KEY_LEFTMETA;
	modifier[1] = KEY_LEFTCTRL;
	modifier[2] = KEY_LEFTALT;
	modifier[3] = KEY_LEFTSHIFT;
	int meta = 0;
	int ctrl = 0;
	int alt = 0;
	int shift = 0;

	// position 0 never gets touched

		while (1){
		if (read(joy_fd, &js, sizeof(struct js_event)) != sizeof(struct js_event)) {
			perror(TOOL_NAME ": error reading from joystick device");
			exit (-5);
		}

		switch(js.type & ~JS_EVENT_INIT) {
                	case JS_EVENT_BUTTON:
				if (js.value) { // if a button is pressed down remember its state until all buttons are released
	                                if (js.number == joy_values[0]){
	                                	button_state[0] = 1;
	                                        send_code[0] = 1;
	                                }
	                                if (js.number == joy_values[1]){
	                                        button_state[1] = 1;
	                                        send_code[1] = 1;
	                                }
	                                if (js.number == joy_values[2]){
	                                        button_state[2] = 1;
	                                        send_code[2] = 1;
	                                }
	                                if (js.number == joy_values[3]){
	                                        button_state[3] = 1;
	                                        send_code[3] = 1;
	                                }
	                                if (js.number == joy_values[4]){
	                                        button_state[4] = 1;
	                                        send_code[4] = 1;
	                                }
	                  		if (js.number == joy_values[5]){
	                                        button_state[5] = 1;
	                                        send_code[5] = 1;
	                                }      
				}else{ // track when buttons are released
	 				if (js.number == joy_values[0]){
	                                        button_state[0] = 0;
	                                }
	                                if (js.number == joy_values[1]){
	                                        button_state[1] = 0;
	                                }
					if (js.number == joy_values[2]){
	                                        button_state[2] = 0;
	                                }
	                                if (js.number == joy_values[3]){
	                                        button_state[3] = 0;
	                                }
	                                if (js.number == joy_values[4]){
	                                        button_state[4] = 0;
	                                }
	                       		if (js.number == joy_values[5]){
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
						mode2count = 0;
						mode3count = 0;
					}
					if (button_code == mode2_code){
						cout << "mode 2" <<endl;
						mode = 2;
						mode3count = 0;
						mode2count++;
						if (mode2count == 2){
							send_key_down(KEY_LEFTCTRL);
							send_key_down(KEY_LEFTALT);
							send_key_down(KEY_S);
							send_key_up(KEY_S);
							mode = 3;
						}
						if (mode2count == 3){
							send_key_up(KEY_LEFTALT);
							send_key_up(KEY_LEFTCTRL);
							mode2count = 0;
							mode = 1;
						}	
					}
					if (button_code == mode3_code){
						cout << "mode 3" <<endl;
						mode = 3;
						mode2count = 0;
						mode3count++;
						if (mode3count == 2){
							send_key_down(KEY_LEFTCTRL);
							send_key_down(KEY_LEFTALT);
							send_key_down(KEY_E);
							send_key_up(KEY_LEFTCTRL);
							send_key_up(KEY_LEFTALT);
							send_key_up(KEY_E);
						}
						if (mode3count == 3){
							send_key_down(KEY_LEFTCTRL);
							send_key_down(KEY_LEFTALT);
							send_key_down(KEY_E);
							send_key_up(KEY_LEFTCTRL);
							send_key_up(KEY_LEFTALT);
							send_key_up(KEY_E);
							mode3count = 0;
							mode = 1;
						}
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

int main( int argc, char *argv[])
{
	int default_joystick = 0;
	string config_file = "joy2chord-config"; 
	int c;
	extern char *optarg;
	while ((c = getopt(argc, argv, "hvdc:j:")) != -1){
		switch (c){
			case 'h':
				cout << "Useage: " << TOOL_NAME << " -d -v -c [keymap_file] -j [joystick_number]" << endl;
				cout << "	-d Enable Debug output" << endl;
				cout << "	-v Enable Verbose output" << endl;
				cout << "	-c Specify a keymap file to use" << endl;
				cout << "	-j Specify the joystick number to use" << endl;
				exit(-2);
				break;
			case 'd':
				cout << "debug messages enabled" << endl;
				debug = 1; 
				break;
			case 'v':
				cout << "verbose messages enabled" << endl;
				verbose = 1; 
				break;
			case 'c':
				config_file = optarg; 
				break;			
			case 'j':
				default_joystick = atoi (optarg);
				cout << "joystick number set to " << default_joystick << endl;
				break;
		}
	}

	map<string,__u16> mymap;

        setup_uinput_device();
	read_config(config_file, mymap, default_joystick);
	open_joystick(default_joystick);
	main_loop(mymap);
	//send_click_events();           // Send mouse event
        /* Destroy the input device */
        ioctl(uinp_fd, UI_DEV_DESTROY);
        /* Close the UINPUT device */
        close(uinp_fd);
}
