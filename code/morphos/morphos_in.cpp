/*
 * Copyright (C) 2005 Mark Olsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

//#include "../qcommon/q_shared.h"
//#include "../qcommon/qcommon.h"
//#include "../client/client.h"
//#include "../rd-vanilla/tr_local.h"

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <intuition/extensions.h>
#include <intuition/intuitionbase.h>
#include <devices/input.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include <clib/alib_protos.h>

#include "../client/client.h"

#include "morphos_in.h"

#define MAXIMSGS 32

static struct InputEvent imsgs[MAXIMSGS];
extern struct IntuitionBase *IntuitionBase;
extern struct Window *win;
extern struct Screen *screen;

static int imsglow = 0;
static int imsghigh = 0;

extern int mousevisible;

static struct Interrupt InputHandler;
static struct MsgPort *inputport = 0;
static struct IOStdReq *inputreq = 0;
static BYTE inputret = -1;

static int mouse_x, mouse_y;

cvar_t *in_joystick = NULL;

#define DEBUGRING(x)

unsigned char keyconv[] =
{
	0, /* 0 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 10 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 20 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 30 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 40 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 50 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 60 */
	0,
	0,
	0,
	0,
	A_BACKSPACE,
	A_TAB,
	A_KP_ENTER,
	A_ENTER,
	A_ESCAPE,
	A_DELETE, /* 70 */
	A_INSERT,
	A_PAGE_UP,
	A_PAGE_DOWN,
	0,
	A_F11,
	A_CURSOR_UP,
	A_CURSOR_DOWN,
	A_CURSOR_RIGHT,
	A_CURSOR_LEFT,
	A_F1, /* 80 */
	A_F2,
	A_F3,
	A_F4,
	A_F5,
	A_F6,
	A_F7,
	A_F8,
	A_F9,
	A_F10,
	0, /* 90 */
	0,
	0,
	0,
	0,
	0,
	A_SHIFT,
	A_SHIFT,
	0,
	A_CTRL,
	A_ALT, /* 100 */
	A_ALT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	A_PAUSE, /* 110 */
	A_F12,
	A_HOME,
	A_END,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 120 */
	0,
	A_MWHEELUP,
	A_MWHEELDOWN,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 150 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 180 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 220 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 230 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 250 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

struct InputEvent *myinputhandler_real(void);

struct EmulLibEntry myinputhandler =
{
	TRAP_LIB, 0, (void(*)(void))myinputhandler_real
};

void IN_Shutdown()
{
	if (inputret == 0)
	{
		inputreq->io_Data = (void *)&InputHandler;
		inputreq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)inputreq);

		CloseDevice((struct IORequest *)inputreq);

		inputret = -1;
	}

	if (inputreq)
	{
		DeleteStdIO(inputreq);

		inputreq = 0;
	}

	if (inputport)
	{
		DeletePort(inputport);

		inputport = 0;
	}

}

void IN_Restart(void)
{
	IN_Shutdown();
	IN_Init();
}

void IN_Init(void)
{
	Com_DPrintf ("\n------- Input Initialization -------\n");

	inputport = CreatePort(0, 0);

	if (inputport == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create message port");
	}

	inputreq = CreateStdIO(inputport);

	if (inputreq == 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to create IO request");
	}

	inputret = OpenDevice("input.device", 0, (struct IORequest *)inputreq, 0);

	if (inputret != 0)
	{
		IN_Shutdown();
		Sys_Error("Unable to open input.device");
	}

	InputHandler.is_Node.ln_Type = NT_INTERRUPT;
	InputHandler.is_Node.ln_Pri = 100;
	InputHandler.is_Node.ln_Name = "Quake 3 input handler";
	InputHandler.is_Data = 0;
	InputHandler.is_Code = (void(*)())&myinputhandler;
	inputreq->io_Data = (void *)&InputHandler;
	inputreq->io_Command = IND_ADDHANDLER;
	DoIO((struct IORequest *)inputreq);

	Com_DPrintf ("------------------------------------\n");
}


void IN_Frame (void) 
{
}


static sysEvent_t evstore;

int IN_GetEvent(sysEvent_t *ev)
{
	int i;
	unsigned char key;
	int down;

	struct InputEvent ie;

	if (evstore.evType)
	{
		ev->evType = evstore.evType;
		ev->evValue = evstore.evValue;
		ev->evValue2 = evstore.evValue2;

		bzero(&evstore, sizeof(evstore));

		return 1;
	}

	if (mouse_x != 0 || mouse_y != 0)
	{
		ev->evType = SE_MOUSE;
		ev->evValue = mouse_x;
		ev->evValue2 = mouse_y;

		mouse_x = 0;
		mouse_y = 0;

		return 1;
	}

	while (imsglow != imsghigh)
	{
		i = imsglow;

		imsglow++;
		imsglow%= MAXIMSGS;

		if ((win->Flags & WFLG_WINDOWACTIVE))
		{
			if (imsgs[i].ie_Class == IECLASS_RAWKEY)
			{
				key = 0;

				down = !(imsgs[i].ie_Code&IECODE_UP_PREFIX);
				imsgs[i].ie_Code&=~IECODE_UP_PREFIX;

				if (imsgs[i].ie_Code <= 255)
					key = keyconv[imsgs[i].ie_Code];

				if (key == 0)
				{
					bzero(&ie, sizeof(ie));

					ie.ie_Class = IECLASS_RAWKEY;
					ie.ie_SubClass = 0;
					ie.ie_Code = imsgs[i].ie_Code;
					ie.ie_Qualifier = imsgs[i].ie_Qualifier&~(IEQUALIFIER_CONTROL);

					MapRawKey(&ie, &key, 1, 0);
				}

				if (key)
				{
					ev->evType = SE_KEY;
					ev->evValue = key;
					ev->evValue2 = down;

					if (down && (keyconv[imsgs[i].ie_Code] == 0 || key == A_BACKSPACE))
					{
						if (key == A_BACKSPACE)
							key = 8;

						evstore.evType = SE_CHAR;
						evstore.evValue = key;
					}

				}

				else
				{
#if 0
					Com_Printf("Unknown key %d\n", imsgs[i].ie_Code);
#endif
				}
			}

			else if (imsgs[i].ie_Class == IECLASS_RAWMOUSE)
			{
				if ((imsgs[i].ie_Code&(~IECODE_UP_PREFIX)) == IECODE_LBUTTON)
					ev->evValue = A_MOUSE1;

				else if ((imsgs[i].ie_Code&(~IECODE_UP_PREFIX)) == IECODE_RBUTTON)
					ev->evValue = A_MOUSE2;

				else if ((imsgs[i].ie_Code&(~IECODE_UP_PREFIX)) == IECODE_MBUTTON)
					ev->evValue = A_MOUSE3;

				if (ev->evValue)
				{
					ev->evType = SE_KEY;

					if ((imsgs[i].ie_Code&IECODE_UP_PREFIX))
						ev->evValue2 = qfalse;

					else
						ev->evValue2 = qtrue;
				}

				mouse_x+= imsgs[i].ie_position.ie_xy.ie_x;
				mouse_y+= imsgs[i].ie_position.ie_xy.ie_y;
			}

			else if (imsgs[i].ie_Class == IECLASS_NEWMOUSE)
			{
				ev->evValue2 = qtrue;

				if (imsgs[i].ie_Code == NM_WHEEL_UP)
					ev->evValue = A_MWHEELUP;

				else if (imsgs[i].ie_Code == NM_WHEEL_DOWN)
					ev->evValue = A_MWHEELDOWN;

				else if (imsgs[i].ie_Code == NM_BUTTON_FOURTH)
					ev->evValue = A_MOUSE4;

				else if (imsgs[i].ie_Code == (NM_BUTTON_FOURTH|IECODE_UP_PREFIX))
				{
					ev->evValue = A_MOUSE4;
					ev->evValue2 = qfalse;
				}

				if (ev->evValue)
				{
					ev->evType = SE_KEY;
				}
			}
		}

		if (mouse_x != 0 || mouse_y != 0)
		{
			if (ev->evType)
				evstore = *ev;

			ev->evType = SE_MOUSE;
			ev->evValue = mouse_x;
			ev->evValue2 = mouse_y;

			mouse_x = 0;
			mouse_y = 0;

			return 1;
		}

		if (ev->evType)
			return 1;
	}

	return 0;
}


struct InputEvent *myinputhandler_real()
{
	struct InputEvent *moo = (struct InputEvent *)REG_A0;

	struct InputEvent *coin;

	int screeninfront;

	coin = moo;

	if (screen)
	{
		screeninfront = screen==IntuitionBase->FirstScreen;
	}

	else
		screeninfront = 1;

	do
	{
		if (coin->ie_Class == IECLASS_RAWMOUSE || coin->ie_Class == IECLASS_RAWKEY || coin->ie_Class == IECLASS_NEWMOUSE)
		{
			if ((imsghigh > imsglow && !(imsghigh == MAXIMSGS-1 && imsglow == 0)) || (imsghigh < imsglow && imsghigh != imsglow-1) || imsglow == imsghigh)
			{
				memcpy(&imsgs[imsghigh], coin, sizeof(imsgs[0]));
				imsghigh++;
				imsghigh%= MAXIMSGS;
			}

			if (!mousevisible && (win->Flags & WFLG_WINDOWACTIVE) && coin->ie_Class == IECLASS_RAWMOUSE && screeninfront && win->MouseX > 0 && win->MouseY > 0)
			{
					coin->ie_position.ie_xy.ie_x = 0;
					coin->ie_position.ie_xy.ie_y = 0;
			}
		}

		coin = coin->ie_NextEvent;

	} while(coin);

	return moo;
}
