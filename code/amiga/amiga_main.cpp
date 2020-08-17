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

extern struct Library *SocketBase;


/*

*/

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

/*
=================
Sys_UnloadDll
=================
*/
#if 0
void Sys_UnloadDll( void *dllHandle )
{
	if( !dllHandle )
	{
		Com_Printf("Sys_UnloadDll(NULL)\n");
		return;
	}

	//Sys_UnloadLibrary(dllHandle);
	dllFreeLibrary(dllHandle);
}
#endif

#if 0

void *Sys_LoadDll(const char *name, qboolean useSystemLib)
{
	void *dllhandle = NULL;
	
	if(useSystemLib)
		Com_Printf("Trying to load \"%s\"...\n", name);
	
	#if 0
	//if(!useSystemLib || !(dllhandle = Sys_LoadLibrary(name)))
	if(!useSystemLib || !(dllhandle = dllLoadLibrary((char *)name)))
	{
		const char *topDir;
		char libPath[MAX_OSPATH];
        
		topDir = Sys_BinaryPath();
        
		if(!*topDir)
			topDir = ".";
        
		Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, topDir);
		Com_sprintf(libPath, sizeof(libPath), "%s%c%s", topDir, PATH_SEP, name);
        
		if(!(dllhandle = dllLoadLibrary(libPath)))
		{
			const char *basePath = Cvar_VariableString("fs_basepath");
			
			if(!basePath || !*basePath)
				basePath = ".";
			
			if(FS_FilenameCompare(topDir, basePath))
			{
				Com_Printf("Trying to load \"%s\" from \"%s\"...\n", name, basePath);
				Com_sprintf(libPath, sizeof(libPath), "%s%c%s", basePath, PATH_SEP, name);
				dllhandle = dllLoadLibrary(libPath);
			}
			
			if(!dllhandle)
			{
				Com_Printf("Loading \"%s\" failed\n", name);
			}
		}
	}
	
	#endif

	return dllhandle;
}

#endif

#if 0

char *Sys_GetDLLName( const char *name )
{
	return va("%s" ARCH_STRING DLL_EXT, name);
}

//void * QDECL Sys_LoadDll( const char *name, intptr_t(**entryPoint)(int, ...), intptr_t(*systemcalls)(intptr_t, ...) )
void *Sys_LoadDll( const char *name, char *fqpath, intptr_t(**entryPoint)(int, int, int, int ), intptr_t(*systemcalls)(intptr_t, ...) ) // Cowcat
{

	void	*libHandle;
	char	fname[MAX_OSPATH];
	char	*basepath;
	char	*gamedir;
	char	*fn;
	void	(*dllEntry)( intptr_t (*syscallptr)(intptr_t, ...) );

	Q_strncpyz(fname, Sys_GetDLLName(name), sizeof(fname));

	basepath = Cvar_VariableString( "fs_basepath" );
	gamedir = Cvar_VariableString( "fs_game" );

	//fn = FS_BuildOSPath( basepath, "", fname ); // fuck it - Cowcat
	fn = (char*)name;

	//Com_Printf( "name(%s)... \n", name );
	//Com_Printf( "fname(%s)... \n", fname );

	Com_Printf( "Sys_LoadDll(%s)... \n", fn );
	libHandle = dllLoadLibrary(fn, (char*)name );

	if (!libHandle)
		return NULL;

	dllEntry = dllGetProcAddress(libHandle, (char *)"dllEntry");
	*entryPoint = dllGetProcAddress(libHandle, (char *)"vmMain");

	if (!*entryPoint || !dllEntry)
	{
		dllFreeLibrary(libHandle);
		return NULL;
	}
	
	dllEntry(systemcalls);

	return libHandle;
}
#endif


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

#if 1

extern char *FS_BuildOSPath( const char *base, const char *game, const char *qpath );

void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);
	
	const char	*basepath;
	const char	*cdpath;
	const char	*gamedir;
	const char	*homepath;

#ifdef MACOS_X
    const char  *apppath;
#endif
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
	cdpath = Cvar_VariableString( "fs_cdpath" );
	gamedir = Cvar_VariableString( "fs_game" );

#ifdef MACOS_X
    apppath = Cvar_VariableString( "fs_apppath" );
#endif
	
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

#ifdef MACOS_X
    if (!game_library) {
		if( apppath[0] ) {
			Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			
			fn = FS_BuildOSPath( apppath, gamedir, gamename );
			game_library = Sys_LoadLibrary( fn, fn );
		}
	}
#endif
    
	#if 0
	if (!game_library)
	{
		if( cdpath[0] )
		{
			//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
			
			fn = FS_BuildOSPath( cdpath, gamedir, gamename );
			//game_library = Sys_LoadLibrary( fn );
			game_library = dllLoadLibrary( fn, 0 );
		}
	}
	#endif

//Now try in base. basepath -> homepath -> cdpath

	#if 0
	if (!game_library)
	{
		//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
		Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
		
		fn = FS_BuildOSPath( basepath, OPENJKGAME, gamename );
		//game_library = Sys_LoadLibrary( fn );
		game_library = dllLoadLibrary( fn, 0 );
	}

	if (!game_library)
	{
		if ( homepath[0] )
		{
			//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
			
			fn = FS_BuildOSPath( homepath, OPENJKGAME, gamename );
			//game_library = Sys_LoadLibrary( fn );
			game_library = dllLoadLibrary( fn, 0 );
		}
	}
	#endif

#ifdef MACOS_X
    if (!game_library) {
		if( apppath[0] ) {
			Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			
			fn = FS_BuildOSPath( apppath, OPENJKGAME, gamename );
			game_library = Sys_LoadLibrary( (char *)fn );
		}
	}
#endif
	#if 0
	if (!game_library)
	{
		if( cdpath[0] )
		{
			//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
			
			fn = FS_BuildOSPath( cdpath, OPENJKGAME, gamename );
			//game_library = Sys_LoadLibrary( fn );
			game_library = dllLoadLibrary( fn, 0 );
		}
	}
	#endif

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

#else

void *Sys_GetGameAPI (void *parms)
{
	void	*(*GetGameAPI) (void *);

	char	name[MAX_OSPATH];
	char	*curpath;
	char	*path;

	char *gamename = "jospgame" ARCH_STRING DLL_EXT;

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current debug directory first for development purposes
	curpath = Sys_Cwd();

	Com_Printf("------- Loading %s -------\n", gamename);
	Com_sprintf (name, sizeof(name), "%s/%s", curpath, gamename);

	game_library = dllLoadLibrary (name, name );

	if (game_library)
		Com_DPrintf ("LoadLibrary (%s)\n",name);
	else {
		Com_Printf( "LoadLibrary(\"%s\") failed\n", game_library);
		//Com_Printf( "...reason: '%s'\n", dlerror() );
		Com_Error( ERR_FATAL, "Couldn't load game" );
	}

	//GetGameAPI = (void *(*)(void *))dlsym (game_library, "GetGameAPI");
	GetGameAPI = (void *(*)(void *))dllGetProcAddress (game_library, "GetGameAPI");

	if (!GetGameAPI)
	{
		//Com_Error( ERR_FATAL, "dlsym GetGameAPI failed %s\n", dlerror());
		Com_Error( ERR_FATAL, "dlsym GetGameAPI failed\n");
		Sys_UnloadGame ();		
		return NULL;
	}

	return GetGameAPI (parms);
}

#endif

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
	if(SocketBase)
	{
		CloseLibrary(SocketBase);
		SocketBase = NULL;
	}

}
	
void Sys_Exit(int ex)
{
	//Sys_DestroyConsole();
	//Timer_Term(); // not used now - Cowcat

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
//	IN_Shutdown();

	Sys_Exit(0);
}


/*
==============
Sys_Print
==============
*/
void Sys_Print( const char *msg ) 
{
	//IExec->DebugPrintF("%s", msg);
	//Conbuf_AppendText( msg );
	if(!consoleoutput)
		return;

	fputs(msg, stdout);
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


#if 0 // old

#define MAX_QUED_EVENTS		256
#define MASK_QUED_EVENTS 	(MAX_QUED_EVENTS - 1)

sysEvent_t eventQue[MAX_QUED_EVENTS];
int eventHead, eventTail;
byte sys_packetReceived[MAX_MSGLEN];

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr )
{
	sysEvent_t	*ev;

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

	// bk000305 - was missing
	if ( eventHead - eventTail >= MAX_QUED_EVENTS )
	{
	  	Com_Printf("Sys_QueEvent: overflow\n");
	  	// we are discarding an event, but don't leak memory
	  	if ( ev->evPtr ) {
	    		Z_Free( ev->evPtr );
	  	}

	  	eventTail++;
	}

	eventHead++;

	if ( time == 0 ) {
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

sysEvent_t Sys_GetEvent(void)
{
	//printf ("\n------- Sys_GetEvent -------\n");

	sysEvent_t	ev;
	msg_t		netmsg;
	
	if (eventHead > eventTail) 
	{
		eventTail++;
		return eventQue [(eventTail - 1) &MASK_QUED_EVENTS ];
	}
	
	//Sys_SendKeyEvents();

	#if 0 // Console Commands
	char 		*s;

	if (s)
	{
		char *b;
		int len;
		
		len = strlen(s) + 1;
		b = (char *)malloc(len);
		strcpy (b, s);
		Sys_QueEvent(0, SE_CONSOLE, 0, 0, len, b);
	}
	#endif

	MSG_Init(&netmsg, sys_packetReceived, sizeof(sys_packetReceived));

	// return if we have data
	if ( eventHead > eventTail ) 
	{
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}
	
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();

	return ev;
}	

#else // new Cowcat

#define MAX_QUED_EVENTS		256
#define MASK_QUED_EVENTS 	(MAX_QUED_EVENTS - 1)

static sysEvent_t	eventQue[MAX_QUED_EVENTS] = {};
static sysEvent_t	*lastEvent = nullptr;
static uint32_t		eventHead = 0, eventTail = 0;

sysEvent_t Sys_GetEvent(void)
{
	sysEvent_t	ev;
	
	if (eventHead > eventTail) 
	{
		eventTail++;
		return eventQue [(eventTail - 1) &MASK_QUED_EVENTS ];
	}

	#if 0 // Console Commands
	char 	*s;

	if (s)
	{
		char *b;
		int len;
		
		len = strlen(s) + 1;
		b = (char *)malloc(len);
		strcpy (b, s);
		Sys_QueEvent(0, SE_CONSOLE, 0, 0, len, b);
	}
	#endif

	// return if we have data
	if ( eventHead > eventTail ) 
	{
		eventTail++;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}
	
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


	if ( evType == SE_MOUSE && lastEvent && lastEvent->evType == SE_MOUSE )
	{
	  	if ( eventTail == eventHead )
		{
			lastEvent->evValue = value;
			lastEvent->evValue2 = value2;
			eventTail--;
		}
			
		else
		{
			lastEvent->evValue += value;
			lastEvent->evValue2 += value2;
		}

		lastEvent->evTime = evTime;
		return;
	}

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

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


#endif

#define DEFAULT_BASEDIR Sys_BinaryPath()

#define fgetenv() ({ float env; asm("mffs %0" : "=f" (env)); env; })
#define fsetenv(env) ({ double d = (env); asm("mtfsffs 0xff, %0" : : "f" (d)); })


void Sys_Sleep(int msec) // used in common.c/Com_Frame - never reached? - Cowcat
{
	// Just in case - Cowcat
	if( msec == 0)
		return;

	if( msec < 0 )
		msec = 10;
	//

	usleep(1000 * msec);
}


int main(int argc, char **argv)
{
	char 	*cmdline;
	int 	i, len;

	//float oldround;
	//oldround = fgetenv(); // test Cowcat
	//asm("mtfsfi 7,1"); // rounding to zero ppc

	if(SocketBase == NULL)
		SocketBase = OpenLibrary("bsdsocket.library",0L);

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
		if( com_busyWait->integer )
		{
			bool shouldSleep = false;

			if( shouldSleep )
				Sys_Sleep(5);
		}

		//IN_Frame(); // future in qcommon/com_frame
		Com_Frame();
	}

	//fsetenv(oldround); // test Cowcat

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


