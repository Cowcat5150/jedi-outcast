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
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
//#include "../rd-vanilla/tr_local.h" // ONLY ENABLE THIS IF WARPOS EVENT HACK IS NOT USED

#include "amiga_local.h"
#include <mgl/gl.h> // needed now for vidpointer - Cowcat

#pragma pack(push,2)

#include <devices/timer.h>
#include <exec/ports.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif 
#endif

#pragma pack(pop)

static cvar_t *in_mouse	 = NULL;
cvar_t *in_nograb        = NULL; 
cvar_t *in_joystick      = NULL;
cvar_t *in_joystickDebug = NULL;
cvar_t *joy_threshold    = NULL;

qboolean mouse_avail;
qboolean mouse_active = qtrue;
//int mx = 0, my = 0;

struct Library *KeymapBase = 0;

extern struct Window *win;
extern qboolean windowmode;

static void IN_ProcessEvents( qboolean keycatch );

void IN_ActivateMouse( qboolean isFullscreen ) 
{
	if (!mouse_avail)
		return;

	if (!mouse_active)
	{
		install_grabs();
		mouse_active = qtrue;
	}
}

void IN_DeactivateMouse( qboolean isFullscreen ) 
{
	if (!mouse_avail)
		return;
		
	if (mouse_active)
	{
		uninstall_grabs();
		mouse_active = qfalse;
	}
}

void IN_Frame (void) 
{
	// IN_JoyMove(); // FIXME: disable if on desktop?

	#if 0

	qboolean loading;

	// If not DISCONNECTED (main menu) or ACTIVE (in game), we're loading
	loading = ( clc.state != CA_DISCONNECTED && clc.state != CA_ACTIVE );

	// update isFullscreen since it might of changed since the last vid_restart
	cls.glconfig.isFullscreen = Cvar_VariableIntegerValue( "r_fullscreen" ) != 0;

	if( !cls.glconfig.isFullscreen && ( Key_GetCatcher( ) & KEYCATCH_CONSOLE ) )
	{
		// Console is down in windowed mode
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}

	else if( !cls.glconfig.isFullscreen && loading )
	{
		// Loading in windowed mode
		IN_DeactivateMouse( cls.glconfig.isFullscreen );
	}

	else
		IN_ActivateMouse( cls.glconfig.isFullscreen );

	#else 

	// Cowcat windowmode mousehandler juggling

	static qboolean mousein;
	qboolean keycatch = qfalse;

	if ( windowmode && mouse_avail )
	{
		int keycatcher = Key_GetCatcher( );

		if( keycatcher & KEYCATCH_CONSOLE )
		{
			if(mousein)
			{
				//Com_Printf("mouseoff\n");

				win->Flags &= ~WFLG_REPORTMOUSE;

				mglEnablePointer(); // new Cowcat
				MouseHandlerOff();
				mousein = qfalse;
			}
		}

		else if( !mousein )
		{
			//Com_Printf("mousein\n");

			win->Flags |= WFLG_REPORTMOUSE;

			mglClearPointer(); // new Cowcat
			MouseHandler();
			mousein = qtrue;
		}

		if ( cls.cgameStarted == qfalse || keycatcher & (KEYCATCH_UI) )
		{
			//Com_Printf("keycath ui\n");
			keycatch = qtrue;
		}
	}

	#endif
	
	IN_ProcessEvents(keycatch);
}


void IN_Init(void) 
{
	if (!KeymapBase)
		KeymapBase = OpenLibrary("keymap.library", 0);

	Com_Printf ("\n------- Input Initialization -------\n");

	// mouse variables
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);

	// developer feature, allows to break without loosing mouse pointer
	in_nograb = Cvar_Get ("in_nograb", "0", 0);

	in_joystick = Cvar_Get ("in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH);
	in_joystickDebug = Cvar_Get ("in_debugjoystick", "0", CVAR_TEMP);
	joy_threshold = Cvar_Get ("joy_threshold", "0.15", CVAR_ARCHIVE); // FIXME: in_joythreshold

	mouse_avail = (in_mouse->value != 0);

	if(mouse_avail)
	{
		if(windowmode)
			MouseHandler();

		mglClearPointer();
	}

	else
	{
		mglEnablePointer();

		win->IDCMPFlags &= ~IDCMP_MOUSEBUTTONS|IDCMP_MOUSEMOVE|IDCMP_DELTAMOVE;
		//win->Flags &= ~WFLG_REPORTMOUSE;
	}

	//IN_StartupJoystick( ); // bk001130 - from cvs1.17 (mkv)

	Com_Printf ("------------------------------------\n");
}

void IN_Shutdown(void)
{
	MouseHandlerOff();

	mglEnablePointer(); // new Cowcat

	mouse_avail = qfalse;

	if (KeymapBase)
	{
		CloseLibrary(KeymapBase);
		KeymapBase = 0;
	}
}

void IN_Restart(void)
{
	IN_Shutdown();
	IN_Init();
}

static unsigned char scantokey[128] =
{
	'`','1','2','3','4','5','6','7',                                 // 7
	'8','9','0','-','=','\\',0,A_INSERT,                             // f
	'q','w','e','r','t','y','u','i',                                 // 17
	'o','p','[',']',0,A_END,A_KP_2,A_PAGE_DOWN,                   // 1f
	'a','s','d','f','g','h','j','k',                                 // 27
	'l',';','\'',0,0,A_KP_4,A_KP_5,A_KP_6,          		// 2f
	'<','z','x','c','v','b','n','m',                                 // 37
	',','.','/',0,A_KP_PERIOD,A_HOME,A_KP_8,A_PAGE_UP,        	 	// 3f
	A_SPACE,A_BACKSPACE,A_TAB,A_KP_ENTER,A_ENTER,A_ESCAPE,A_DELETE,0,   // 47
	0,0,A_KP_MINUS,0,A_CURSOR_UP,A_CURSOR_DOWN,A_CURSOR_RIGHT,A_CURSOR_LEFT, // 4f
	A_F1,A_F2,A_F3,A_F4,A_F5,A_F6,A_F7,A_F8,                         // 57
	A_F9,A_F10,0,0,0,0,0,A_F11,                                      // 5f
	A_SHIFT,A_SHIFT,0,A_CTRL,A_ALT,A_ALT,0,0,               	 // 67
	0,0,0,0,0,0,0,0,                                                 // 6f
	0,0,0,0,0,0,0,0,                                                 // 77
	0,0,0,0,0,0,0,0                                                  // 7f
};

static qboolean keyDown(UWORD code)
{
	if (code & IECODE_UP_PREFIX)
		return qfalse;
	
	return qtrue;
}

#if !defined(__PPC__)

static int XLateKey(struct IntuiMessage *ev)
{
	//Com_Printf("Xlate %d\n", ev->Code);
	return scantokey[ev->Code&0x7f];
}

static void IN_ProcessEvents(qboolean keycatch)
{
	struct IntuiMessage *imsg;
	struct InputEvent ie;
	WORD res;
	char buf[4];

	if (!Sys_EventPort)
		return;

	const ULONG msgTime = 0; //Sys_Milliseconds();

	while ((imsg = (struct IntuiMessage *)GetMsg(Sys_EventPort)))
	{
		switch (imsg->Class)
		{
			case IDCMP_RAWKEY:
			{
				if ( keycatch && imsg->Code == ( 0x63 & ~IECODE_UP_PREFIX ) ) // windowmode handler workaround
				{
					Sys_QueEvent(msgTime, SE_KEY, A_MOUSE1, keyDown(imsg->Code), 0, NULL);
					//Com_Printf ("mouse key RAWKEY\n"); //
				}

				else
				{	
					int key = XLateKey(imsg);

					//Com_Printf ("key encoded %d %d\n", imsg->Code, imsg->Qualifier);
					//Com_Printf ("key encoded $%04x $%04lx\n", imsg->Code, imsg->Qualifier);

					ie.ie_Class = IECLASS_RAWKEY;
					ie.ie_SubClass = 0;
					ie.ie_Code = imsg->Code;
					ie.ie_Qualifier = imsg->Qualifier;
					ie.ie_EventAddress = 0;

					if (key == '`')
					{
						key = A_CONSOLE;
						res = 0;
					}

					else
						res = MapRawKey(&ie, buf, 20, 0);

					Sys_QueEvent(msgTime, SE_KEY, key, keyDown(imsg->Code), 0, NULL);

					if (res == 1)
					{
						Sys_QueEvent(msgTime, SE_CHAR, buf[0], 0, 0, NULL);
					}
				}
			}
				
			break;
			
			case IDCMP_MOUSEMOVE:

				if (mouse_active)
				{
					//Com_Printf("sendkeyevents mousemove\n");

					mx = imsg->MouseX;
					my = imsg->MouseY;

					Sys_QueEvent(msgTime, SE_MOUSE, mx, my, 0, NULL);
				}

				break; // drop through ? Cowcat

			/*
			case IDCMP_EXTENDEDMOUSE:
				// FIXME: Add additional mouse buttons
				if (imsg->Code & IMSGCODE_INTUIWHEELDATA)
				{
					struct IntuiWheelData *iwd = (struct IntuiWheelData *)imsg->IAddress;
							
					if (iwd->WheelY > 0)
					{
						Com_QueueEvent(msgTime, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL);
						Com_QueueEvent(msgTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
					}
					else if (iwd->WheelY < 0)
					{
						Com_QueueEvent(msgTime, SE_KEY, K_MWHEELUP, qtrue, 0, NULL);
						Com_QueueEvent(msgTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
					}
				}
				break;
			*/

			case IDCMP_MOUSEBUTTONS:

				switch (imsg->Code & ~IECODE_UP_PREFIX)
				{

					case IECODE_LBUTTON:
						//Com_Printf("sendkeyevents lmousebutton\n");
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE1, keyDown(imsg->Code),0, NULL);
						break;

					case IECODE_RBUTTON:
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE2, keyDown(imsg->Code),0, NULL);
						break;

					case IECODE_MBUTTON:
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE3, keyDown(imsg->Code),0, NULL);
						break;
				}
		}

		ReplyMsg((struct Message *)imsg);
	}
}

#else // trying to reduce WOS context switches 

#pragma pack(push,2)
struct MsgStruct
{
	ULONG Class;
	UWORD Code;
	WORD MouseX;
	WORD MouseY;
	UWORD rawkey;
};
#pragma pack(pop)

extern "C" {
	int GetMessages68k( struct MsgPort *port, struct MsgStruct *msg, int maxmsg );
}

static int GetEvents(void *port, void *msgarray, int maxmsg)
{
	extern int GetMessages68k(struct MsgPort *port, struct MsgStruct *msg, int maxmsg);
	struct PPCArgs args;

	args.PP_Code = (APTR)GetMessages68k;
	args.PP_Offset = 0;
	args.PP_Flags = 0;
	args.PP_Stack = NULL;
	args.PP_StackSize = 0;
	args.PP_Regs[PPREG_A0] = (ULONG)msgarray;
	args.PP_Regs[PPREG_A1] = (ULONG)port;
	args.PP_Regs[PPREG_D0] = maxmsg;

	Run68K(&args);

	return args.PP_Regs[PPREG_D0];
}


static void IN_ProcessEvents( qboolean keycatch )
{
	UWORD res;
	int i;

	if(!Sys_EventPort)
		return;

	struct MsgStruct events[50];
	const ULONG msgTime = 0; //Sys_Milliseconds();

	int messages = GetEvents(Sys_EventPort, events, 50);

	//if (messages > 0)
		//Com_Printf("messages %d\n", messages);

	i = 0;

	while ( i < messages )
	{
		switch( events[i].Class )
		{
			case IDCMP_RAWKEY:

				if ( keycatch &&  events[i].Code == ( 0x63 & ~IECODE_UP_PREFIX ) ) // windowmode handler workaround
				{
					Sys_QueEvent(msgTime, SE_KEY, A_MOUSE1, keyDown(events[i].Code), 0, NULL);
					//Com_Printf ("mouse key RAWKEY\n"); //
				}

				else
				{
					int key = scantokey[ events[i].Code & 0x7f ];

					//Com_Printf ("key encoded %d \n", key); //

					if (key == '`') 
					{
						key = A_CONSOLE;
						res = 0;
					}

					else
						res = 1;

					Sys_QueEvent(msgTime, SE_KEY, key, keyDown(events[i].Code), 0, NULL);

					if (res == 1)
						Sys_QueEvent(msgTime, SE_CHAR, events[i].rawkey, 0, 0, NULL);
				}

				break;
				
			case IDCMP_MOUSEMOVE:

				if (mouse_active)
				{
					Sys_QueEvent(msgTime, SE_MOUSE, events[i].MouseX, events[i].MouseY, 0, NULL);
				}

				break;

			case IDCMP_MOUSEBUTTONS:

				#if 0

				switch (events[i].Code & ~IECODE_UP_PREFIX)
				{
					case IECODE_LBUTTON:
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE1, keyDown(events[i].Code), 0, NULL);
						break;

					case IECODE_RBUTTON:
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE2, keyDown(events[i].Code), 0, NULL);
						break;

					case IECODE_MBUTTON:
						Sys_QueEvent(msgTime, SE_KEY, A_MOUSE3, keyDown(events[i].Code), 0, NULL);
						break;
				}

				#else

				unsigned short b = 0;

				switch (events[i].Code & ~IECODE_UP_PREFIX)
				{
					case IECODE_LBUTTON: b = A_MOUSE1; break;
					case IECODE_RBUTTON: b = A_MOUSE2; break;
					case IECODE_MBUTTON: b = A_MOUSE3; break;
				}

				Sys_QueEvent(msgTime, SE_KEY, b, keyDown(events[i].Code), 0, NULL);

				#endif
			
		}

		i++;
	}

}

#endif

void install_grabs (void)
{
	//mglGrabFocus(GL_TRUE);
	mouse_active = qfalse;
}

void uninstall_grabs (void)
{
	//mglGrabFocus(GL_FALSE);
	//mx = 0;
	//my = 0;
	mouse_active = qtrue;
}


