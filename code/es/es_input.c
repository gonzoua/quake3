/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "../sys/sys_local.h"

// q3rev.cpp : Defines the entry point for the application.
//

#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/mouse.h>
#include <fcntl.h>
#include <unistd.h>

#include <termios.h>
#include <sys/kbio.h>

#include "es_scancodes.h"

static int mouse_fd = -1;
static uint8_t mouse_buttons = MOUSE_SYS_STDBUTTONS;

static int kbd_fd = -1;
static struct termios kbd_orig_tty;
static int orig_kbd_mode;
static int kbd_orig_repeat[2];

void init_kbd()
{
	struct termios kbdtty;

	kbd_fd = fileno(stdin);
	if (kbd_fd < 0)
		return;

	if (ioctl(kbd_fd, KDGKBMODE, &orig_kbd_mode)) {
		perror("ioctl(KDGKBMODE)");
		goto out;
	}

	if (ioctl(kbd_fd, KDGETREPEAT, kbd_orig_repeat)) {
                perror("ioctl(KDGETREPEAT)");
		goto out;
	}


	if (ioctl(kbd_fd, KDSKBMODE, K_CODE)) {
		perror("ioctl(KDSKBMODE)");
		goto out;
	}

      	if (tcgetattr(kbd_fd, &kbdtty)) {
		perror("tcgetattr");
		goto out;
	}

	kbd_orig_tty = kbdtty;

	kbdtty.c_iflag = IGNPAR | IGNBRK;
	/* kbdtty.c_oflag = 0; */
	kbdtty.c_cflag = CREAD | CS8;
	kbdtty.c_lflag = 0;
	kbdtty.c_cc[VTIME] = 0;
	kbdtty.c_cc[VMIN] = 1;
	cfsetispeed(&kbdtty, 9600);
	cfsetospeed(&kbdtty, 9600);
	if (tcsetattr(kbd_fd, TCSANOW, &kbdtty) < 0) {
		perror("tcsetattr");
		goto out;
	}

        int arg[2];
        arg[0] = INT_MAX;
        arg[1] = INT_MAX;
        if (ioctl(kbd_fd, KDSETREPEAT, arg))
                perror("ioctl(KDSETREPEAT)");
	return;

out:
	kbd_fd = -1;
}

void close_kbd()
{
	if (kbd_fd < 0)
		return;

	if (tcsetattr(kbd_fd, TCSANOW, &kbd_orig_tty) < 0) {
		perror("tcsetattr");
	}

	if (ioctl(kbd_fd, KDSKBMODE, orig_kbd_mode))
		perror("ioctl(KDSKBMODE)");

        if (ioctl(kbd_fd, KDSETREPEAT, kbd_orig_repeat))
                perror("ioctl(KDSETREPEAT)");

	kbd_fd = -1;
}


int has_kbd_event( void )
{
	struct timeval tv;
	fd_set fds;

	if (kbd_fd < 0)
		return 0;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(kbd_fd, &fds);
	select(kbd_fd+1, &fds, NULL, NULL, &tv);

	return FD_ISSET(kbd_fd, &fds);
}

#define MAX_CONSOLE_KEYS 16

/*
===============
IN_IsConsoleKey
===============
*/
static qboolean IN_IsConsoleKey( keyNum_t key, const unsigned char character )
{
	typedef struct consoleKey_s
	{
		enum
		{
			KEY,
			CHARACTER
		} type;

		union
		{
			keyNum_t key;
			unsigned char character;
		} u;
	} consoleKey_t;

	static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
	static int numConsoleKeys = 0;
	int i;

	// Only parse the variable when it changes
	if( cl_consoleKeys->modified )
	{
		char *text_p, *token;

		cl_consoleKeys->modified = qfalse;
		text_p = cl_consoleKeys->string;
		numConsoleKeys = 0;

		while( numConsoleKeys < MAX_CONSOLE_KEYS )
		{
			consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
			int charCode = 0;

			token = COM_Parse( &text_p );
			if( !token[ 0 ] )
				break;

			if( strlen( token ) == 4 )
				charCode = Com_HexStrToInt( token );

			if( charCode > 0 )
			{
				c->type = CHARACTER;
				c->u.character = (unsigned char)charCode;
			}
			else
			{
				c->type = KEY;
				c->u.key = Key_StringToKeynum( token );

				// 0 isn't a key
				if( c->u.key <= 0 )
					continue;
			}

			numConsoleKeys++;
		}
	}

	// If the character is the same as the key, prefer the character
	if( key == character )
		key = 0;

	for( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch( c->type )
		{
			case KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}



void send_kbd_event(uint8_t code, qboolean pressed)
{
	keyNum_t key = 0;
	char ch = 0;

	switch (code) {
		case SCANCODE_ESCAPE:
			key = K_ESCAPE;
			break;
		case SCANCODE_1:
		case SCANCODE_2:
		case SCANCODE_3:
		case SCANCODE_4:
		case SCANCODE_5:
		case SCANCODE_6:
		case SCANCODE_7:
		case SCANCODE_8:
		case SCANCODE_9:
			key = '1' + code - SCANCODE_1;
			ch = '1' + code - SCANCODE_1;
			break;
		case SCANCODE_0:
			key = '0';
			ch = '0';
			break;
		case SCANCODE_MINUS:
			key = '-';
			ch = '-';
			break;
		case SCANCODE_EQUAL:
			key = '=';
			ch = '=';
			break;
		case SCANCODE_BACKSPACE:
			key = K_BACKSPACE;
			break;
		case SCANCODE_TAB:
			key = K_TAB;
			ch = '\t';
			break;
		case SCANCODE_Q:
		case SCANCODE_W:
		case SCANCODE_E:
		case SCANCODE_R:
		case SCANCODE_T:
		case SCANCODE_Y:
		case SCANCODE_U:
		case SCANCODE_I:
		case SCANCODE_O:
		case SCANCODE_P:
		case SCANCODE_BRACKET_LEFT:
		case SCANCODE_BRACKET_RIGHT: {
			const char *qwerty = "qwertyuiop[]";
			key = qwerty[code - SCANCODE_Q];
			ch = qwerty[code - SCANCODE_Q];
			break;
		}
		case SCANCODE_ENTER:
			key = K_ENTER;
			//ch = ;
			break;
		case SCANCODE_RIGHTCONTROL:
		case SCANCODE_LEFTCONTROL:
			key = K_CTRL;
			break;
		case SCANCODE_A:
		case SCANCODE_S:
		case SCANCODE_D:
		case SCANCODE_F:
		case SCANCODE_G:
		case SCANCODE_H:
		case SCANCODE_J:
		case SCANCODE_K:
		case SCANCODE_L:
		case SCANCODE_SEMICOLON:
		case SCANCODE_APOSTROPHE:
		case SCANCODE_GRAVE: {
			const char *asdf = "asdfghjkl;'`";
			key = asdf[code - SCANCODE_A];
			ch = asdf[code - SCANCODE_A];
			break;
		}
		case SCANCODE_LEFTSHIFT:
		case SCANCODE_RIGHTSHIFT:
			key = K_SHIFT;
			break;
		case SCANCODE_BACKSLASH:
		case SCANCODE_Z:
		case SCANCODE_X:
		case SCANCODE_C:
		case SCANCODE_V:
		case SCANCODE_B:
		case SCANCODE_N:
		case SCANCODE_M:
		case SCANCODE_COMMA:
		case SCANCODE_PERIOD:
		case SCANCODE_SLASH: {
			const char *zxcv = "\\zxcvbnm,./";
			key = zxcv[code - SCANCODE_BACKSLASH];
			ch = zxcv[code - SCANCODE_BACKSLASH];
			break;
		}

		case SCANCODE_KEYPADMULTIPLY: key = K_KP_STAR; break;

		case SCANCODE_LEFTALT:
		case SCANCODE_RIGHTALT:
			key = K_ALT;
			break;
		case SCANCODE_SPACE:
			key = ' ';
			ch = ' ';
			break;
		case SCANCODE_CAPSLOCK:	key = K_CAPSLOCK; break;
		case SCANCODE_F1:	key = K_F1; break;
		case SCANCODE_F2:	key = K_F2; break;
		case SCANCODE_F3:	key = K_F3; break;
		case SCANCODE_F4:	key = K_F4; break;
		case SCANCODE_F5:	key = K_F5; break;
		case SCANCODE_F6:	key = K_F6; break;
		case SCANCODE_F7:	key = K_F7; break;
		case SCANCODE_F8:	key = K_F8; break;
		case SCANCODE_F9:	key = K_F9; break;
		case SCANCODE_F10:	key = K_F10; break;

		case SCANCODE_NUMLOCK:		key = K_KP_NUMLOCK; break;
		case SCANCODE_SCROLLLOCK:	key = K_SCROLLOCK; break;
		case SCANCODE_KEYPAD7:		key = K_KP_PGDN; break;
		case SCANCODE_KEYPAD8:		key = K_KP_UPARROW; break;
		case SCANCODE_KEYPAD9: 		key = K_KP_PGUP; break;
		case SCANCODE_KEYPADMINUS:	key = K_KP_MINUS; break;
		case SCANCODE_KEYPAD4:		key = K_KP_LEFTARROW; break;
		case SCANCODE_KEYPAD5:		key = K_KP_5; break;
		case SCANCODE_KEYPAD6:		key = K_KP_RIGHTARROW; break;
		case SCANCODE_KEYPADPLUS:	key = K_KP_PLUS; break;
		case SCANCODE_KEYPAD1:		key = K_KP_END; break;
		case SCANCODE_KEYPAD2:		key = K_KP_DOWNARROW; break;
		case SCANCODE_KEYPAD3:		key = K_KP_PGDN; break;
		case SCANCODE_KEYPAD0:		key = K_KP_INS; break;
		case SCANCODE_KEYPADPERIOD:	key = K_KP_DEL; break;
		case SCANCODE_F11:			key = K_F11; break;
		case SCANCODE_F12:			key = K_F12; break;
		case SCANCODE_KEYPADENTER:		key = K_KP_ENTER; break;
		case SCANCODE_KEYPADDIVIDE: 		key = K_KP_SLASH; break;
		case SCANCODE_PRINTSCREEN:		key = K_PRINT; break;
		case SCANCODE_BREAK:			key = K_BREAK; break;
		case SCANCODE_HOME:			key = K_HOME; break;
		case SCANCODE_PAGEUP:			key = K_PGUP; break;
		case SCANCODE_CURSORBLOCKLEFT:		key = K_LEFTARROW; break;
		case SCANCODE_CURSORBLOCKRIGHT:		key = K_RIGHTARROW; break;
		case SCANCODE_CURSORBLOCKUP:		key = K_UPARROW; break;
		case SCANCODE_CURSORBLOCKDOWN:		key = K_DOWNARROW; break;
		case SCANCODE_END:			key = K_END; break;
		case SCANCODE_PAGEDOWN:			key = K_PGUP; break;
		case SCANCODE_INSERT:			key = K_INS; break;

		case SCANCODE_RIGHTWIN:
		case SCANCODE_LEFTWIN:
			key = K_COMMAND;
			break;
	}

	if( IN_IsConsoleKey( key, ch ) )
	{
		// Console keys can't be bound or generate characters
		key = K_CONSOLE;
		ch = 0;
	}

	if (key)
		Com_QueueEvent( 0, SE_KEY, key, pressed, 0, NULL );

	if (ch && pressed)
		Com_QueueEvent( 0, SE_CHAR, ch, 0, 0, NULL );
}

void process_kbd_events( void )
{
	uint8_t code[4];
	int bytes;

	if (kbd_fd < 0)
		return;

	bytes = read(kbd_fd, code, sizeof(code));
	for (int i = 0; i < bytes; i++) {
		send_kbd_event(code[i] & 0x7f, ((code[i] & 0x80) ? qfalse : qtrue));
	}
}

#define DEFAULT_MOUSE "/dev/sysmouse"
void init_mouse()
{
	char *devname = getenv("Q_MOUSE_DEV");

	mouse_fd = open(devname != NULL ? devname : DEFAULT_MOUSE, O_RDONLY);
	if (mouse_fd < 0)
		return;

	int level = 1;
	if (ioctl(mouse_fd, MOUSE_SETLEVEL, &level)) {
		close(mouse_fd);
		return;
	}
}

int has_mouse_event( void )
{
	struct timeval tv;
	fd_set fds;

	if (mouse_fd < 0)
		return 0;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(mouse_fd, &fds);
	select(mouse_fd+1, &fds, NULL, NULL, &tv);

	return FD_ISSET(mouse_fd, &fds);
}

void send_mouse_event(int button, int pressed)
{
	uint8_t b;
	switch( button)
	{
		case 0:   b = K_MOUSE3;     break;
		case 1:   b = K_MOUSE2;     break;
		case 2:   b = K_MOUSE1;     break;
		default:  b = 0xff;
	}

	if (b != 0xff)
		Com_QueueEvent( 0, SE_KEY, b, ( pressed ? qtrue : qfalse ), 0, NULL );

}

void process_mouse_events( void )
{
	int8_t packet[MOUSE_SYS_PACKETSIZE];
	uint8_t status, changed;
	int16_t relx, rely;

	if (mouse_fd < 0)
		return;

	if (read(mouse_fd, packet, sizeof(packet)) < sizeof(packet))
		return;

	status = packet[0] & MOUSE_SYS_STDBUTTONS;
	changed = status ^ mouse_buttons;
	if (changed) {
		for (int i = 0; i < 3; i++)
			if (changed & (1<<i))
				send_mouse_event(i, ((status & (1<<i)) == 0));
	}

	mouse_buttons = status;
	relx = packet[1] + packet[3];
	rely = -(packet[2] + packet[4]);
	Com_QueueEvent( 0, SE_MOUSE, relx, rely, 0, NULL );
}

void close_mouse()
{
	close(mouse_fd);
	mouse_fd = -1;
}

/*
===============
IN_ProcessEvents
===============
*/
static void IN_ProcessEvents( void )
{
	while (has_kbd_event()) {
		process_kbd_events();
	}

	while (has_mouse_event()) {
		process_mouse_events();
	}
}

/*
===============
IN_Init
===============
*/
void IN_Init( void )
{
	init_mouse();
	init_kbd();
}

/*
===============
IN_Frame
===============
*/
void IN_Frame( void )
{
	IN_ProcessEvents( );
}

/*
===============
IN_Shutdown
===============
*/
void IN_Shutdown( void )
{
	close_kbd();
	close_mouse();
}

/*
===============
IN_Restart
===============
*/
void IN_Restart( void )
{
}
