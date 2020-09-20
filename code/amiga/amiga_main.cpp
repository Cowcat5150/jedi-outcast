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
#include "../rd-vanilla/tr_local.h"
#include "../client/client.h"

#include "amiga_local.h"

#include <unistd.h> // usleep - Cowcat

#define __USE_BASETYPE__

#pragma pack(push,2)

#include <exec/exec.h>
#include <exec/ports.h>
#include <intuition/intuition.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <proto/intuition.h>

#pragma pack(pop)

#include "dll.h"

#ifdef __VBCC__
#define CreateExtIO(p,s) CreateIORequest(p,s)
#define DeletePort(p) DeleteMsgPort(p)
#define DeleteExtIO(p) DeleteIORequest(p)
#endif

//extern struct Library *SocketBase;

#define MEM_THRESHOLD 96*1024*1024

cvar_t	*com_maxfps; // new Cowcat

static qboolean consoleoutput = qfalse;

cvar_t *sys_nostdout;
static BPTR amiga_stdout;


qboolean Sys_LowPhysicalMemory() // Always true for Amiga ??? - Cowcat
{
	return qtrue;
}


static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };

/*
=================
Sys_SetBinaryPath
=================
*/
void Sys_SetBinaryPath(const char *path)
{
	Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

/*
=================
Sys_BinaryPath
=================
*/
char *Sys_BinaryPath(void)
{
	return binaryPath;
}

/*
=================
Sys_SetDefaultInstallPath
=================
*/
void Sys_SetDefaultInstallPath(const char *path)
{
	Q_strncpyz(installPath, path, sizeof(installPath));
}

/*
=================
Sys_DefaultInstallPath
=================
*/
char *Sys_DefaultInstallPath(void)
{
	if (*installPath)
		return installPath;
	else
		return Sys_Cwd();
}

/*
=================
Sys_DefaultAppPath
=================
*/
char *Sys_DefaultAppPath(void)
{
	return Sys_BinaryPath();
}


static void *game_library;

/*
=================
Sys_UnloadGame
=================
*/

void Sys_UnloadGame (void)
{
	if (game_library) 
		dllFreeLibrary (game_library);

	game_library = NULL;
}


/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/

extern char *FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	
	const char	*basepath;
	const char	*gamedir;
	const char	*homepath;

	//const char	*fn;
	char	*fn;
	
#ifdef JK2_MODE
	const char *gamename = "jospgame" ARCH_STRING DLL_EXT;
	//char *gamename = "jospgame" ARCH_STRING DLL_EXT;
#else
	const char *gamename = "jagame" ARCH_STRING DLL_EXT;
#endif
	
	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without calling Sys_UnloadGame");
	
	// check the current debug directory first for development purposes
	homepath = Cvar_VariableString( "fs_homepath" );
	basepath = Cvar_VariableString( "fs_basepath" );
	gamedir = Cvar_VariableString( "fs_game" );
	
	fn = FS_BuildOSPath( basepath, gamedir, gamename );
	//fn = (char *)gamename ;
	
	//game_library = Sys_LoadLibrary( fn );	game_library = dllLoadLibrary( fn, 0 );
	
//First try in mod directories. basepath -> homepath -> cdpath
	if (!game_library)
	{
		if (homepath[0])
		{
			//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			//Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn); // Cowcat
			
			fn = FS_BuildOSPath( homepath, gamedir, gamename);
			//game_library = Sys_LoadLibrary( fn );
			game_library = dllLoadLibrary( fn, 0 );
		}
	}
    
//Still couldn't find it.
	if (!game_library)
	{
		//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
		Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
		Com_Error( ERR_FATAL, "Couldn't load game" );
	}
	
	Com_Printf ( "Sys_GetGameAPI(%s): succeeded ...\n", fn );
	
	//GetGameAPI = (void *(*)(void *))Sys_LoadFunction (game_library, "GetGameAPI");
	GetGameAPI = (void *(*)(void *))dllGetProcAddress(game_library, (char *)"GetGameAPI");

	if (!GetGameAPI)
	{
		Sys_UnloadGame ();
		//Com_Error( ERR_FATAL, "dllGetProcAddress failed\n");
		return NULL;
	}

	return GetGameAPI (parms);
}


/*
 =================
 Sys_LoadCgame
 
 Used to hook up a development dll
 =================
 */

void *Sys_LoadCgame( intptr_t (**entryPoint)(int, ...), intptr_t (*systemcalls)(intptr_t, ...) )
{
	void	(*dllEntry)( intptr_t (*syscallptr)(intptr_t, ...) );
    
	//dllEntry = ( void (*)( intptr_t (*)( intptr_t, ... ) ) )Sys_LoadFunction( game_library, "dllEntry" );
	//*entryPoint = (intptr_t (*)(int,...))Sys_LoadFunction( game_library, "vmMain" );
	dllEntry = ( void (*)( intptr_t (*)( intptr_t, ... ) ) )dllGetProcAddress( game_library, (char *)"dllEntry" );
	*entryPoint = (intptr_t (*)(int,...))dllGetProcAddress( game_library, (char *)"vmMain" );

	if ( !*entryPoint || !dllEntry )
	{
		Com_Printf ( "Sys_LoadCGame no entrypoint/dllentry\n" );
		//Sys_UnloadLibrary( game_library );
		dllFreeLibrary( game_library );
		return NULL;
	}
    
	dllEntry( systemcalls );

	return game_library;
}


/*****************************************************************************/


void Sys_BeginProfiling( void ) 
{
}


void LeaveAmigaLibs(void)
{
	/*
	if(SocketBase)
	{
		CloseLibrary(SocketBase);
		SocketBase = NULL;
	}
	*/
}
	
void Sys_Exit(int ex)
{
	//Sys_DestroyConsole();
	//NET_Shutdown();

	LeaveAmigaLibs();

	//printf("Sys_Exit\n");

	exit(ex);
}

/*
=============
Sys_Error

Show the early console as an error dialog
=============
*/
void QDECL Sys_Error( const char *error, ... )
{
	va_list		argptr;
	char		string[1024];

	CL_Shutdown();

	va_start (argptr, error);
	vsprintf (string, error, argptr);
	va_end (argptr);
	fprintf(stderr, "Sys_Error: %s\n", string);
	
	//Conbuf_AppendText( text ); // Cowcat
	//Sys_ShowConsole( 1, qtrue ); //

	//IExec->DebugPrintF("Sys_Error: %s\n", text);
	//fprintf(stderr, "Sys_Error: %s\n", text);
	
	Sys_Exit(1);
}	


/*
==============
Sys_Quit
==============
*/
void Sys_Quit( void ) 
{
	//CL_Shutdown(); // new - disabled Cowcat
	//IN_Shutdown();

	Sys_Exit(0);
}


/*
==============
Sys_Print
==============
*/
void Sys_Print( const char *msg ) 
{
	char cmsg[4096] = { 0 }; // MAXPRINTMSG

	//Conbuf_AppendText( msg );
	if(!consoleoutput)
		return;

	Q_strncpyz( cmsg, msg, sizeof( cmsg ) );
	Q_StripColor( cmsg );

	fputs(cmsg, stdout);
}


/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
void Sys_In_Restart_f( void ) 
{
	IN_Restart();
}


/*
================
Sys_Init

Called after the common systems (cvars, files, etc)
are initialized
================
*/

void Sys_Init( void )
{
	//char *cpuidstr;
	
	Cmd_AddCommand ("in_restart", Sys_In_Restart_f);

	Cvar_Set("arch", "amigaos");
	
	com_maxfps = Cvar_Get("com_maxfps", "60", CVAR_ARCHIVE); // new Cowcat

	// Cowcat
	sys_nostdout = Cvar_Get("sys_nostdout", "1", CVAR_ARCHIVE);

	if(!sys_nostdout->value)  // Cowcat
	{
		amiga_stdout = Output();
		consoleoutput = qtrue;
	}
	//

	//IN_Init(); // now in amiga_glimp - Cowcat
}

qboolean Sys_CheckCD( void ) 
{
  	return qtrue;
}


void Sys_InitStreamThread( void )
{
}

void Sys_ShutdownStreamThread( void )
{
}

void Sys_BeginStreamedFile( fileHandle_t f, int readAhead )
{
}

void Sys_EndStreamedFile( fileHandle_t f )
{
}

int Sys_StreamedRead( void *buffer, int size, int count, fileHandle_t f )
{
  	return FS_Read( buffer, size * count, f );
}

void Sys_StreamSeek( fileHandle_t f, int offset, int origin )
{
  	FS_Seek( f, offset, origin );
}

char *Sys_GetClipboardData(void)
{
  	return NULL;
}


#define MAX_QUED_EVENTS		128
#define MASK_QUED_EVENTS 	( MAX_QUED_EVENTS - 1 )

static sysEvent_t	eventQue[ MAX_QUED_EVENTS ];
static sysEvent_t	*lastEvent = eventQue + MAX_QUED_EVENTS - 1;
static uint32_t		eventHead = 0, eventTail = 0;

extern void IN_ProcessEvents(void);

sysEvent_t Sys_GetEvent(void)
{
	sysEvent_t	ev;

	// return if we have data
	if ( eventHead - eventTail > 0 )
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];

	//IN_Frame(); // it was on Com_Frame - Cowcat
	IN_ProcessEvents();

	// create an empty event on return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/

void Sys_QueEvent( int evTime, sysEventType_t evType, int value, int value2, int ptrLength, void *ptr )
{
	sysEvent_t	*ev;

	if ( evTime == 0 )
		evTime = Sys_Milliseconds();

	if ( evType == SE_MOUSE && lastEvent->evType == SE_MOUSE && eventHead != eventTail )
	{
		lastEvent->evValue += value;
		lastEvent->evValue2 += value2;
		lastEvent->evTime = evTime;
		return;
	}
	
	ev = &eventQue[	eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS )
	{
		Com_Printf("Com_QueueEvent: overflow\n");

		// we are discarding an event, but don't leak memory
		if ( ev->evPtr )
		{
			Z_Free( ev->evPtr );
		}

		eventTail++;
	}

	eventHead++;

	ev->evTime = evTime;
	ev->evType = evType;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;

	lastEvent = ev;
}


#define DEFAULT_BASEDIR Sys_BinaryPath()

void Sys_Sleep(int msec) // used in common.c/Com_Frame - never reached? - Cowcat
{
	if( msec == 0)
		return;

	if( msec < 0 )
		msec = 10;

	usleep(1000 * msec);
}


int main(int argc, char **argv)
{
	char 	*cmdline;
	int 	i, len;

	//if(SocketBase == NULL)
		//SocketBase = OpenLibrary("bsdsocket.library",0L);

	Sys_Milliseconds();
	
	//Sys_SetBinaryPath( Sys_Dirname(argv[0]) ); // nope
	Sys_SetDefaultInstallPath( DEFAULT_BASEDIR );

	// merge the command line, this is kinda silly
	for (len = 1, i = 1; i < argc; i++)
		len += strlen(argv[i]) + 1;

	cmdline = (char *)malloc(len);
	*cmdline = 0;

	for (i = 1; i < argc; i++)
	{
		if (i > 1)
			strcat(cmdline, " ");

		strcat(cmdline, argv[i]);
	}

	Com_Init(cmdline);

	free(cmdline);
	cmdline = NULL;

	while( 1 ) 
	{
		IN_Frame(); // Check windowmode/mousehandler
		Com_Frame();
	}

	return 0; 
}

char *Sys_ConsoleInput(void) // Cowcat
{
	return NULL;
}

void Sys_ShowConsole(int level, qboolean quitOnClose) // Cowcat
{
	return 0;
}


