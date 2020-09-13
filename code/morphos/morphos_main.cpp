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

#include <exec/exec.h>
#include <exec/system.h>
#include <intuition/intuition.h>
#include <dos/dosextens.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/dos.h>
//#include <proto/utility.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//#include <dlfcn.h>
#include "dll.h"

#undef Write

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../rd-vanilla/tr_local.h"
#include "../client/client.h"

#include "morphos_in.h"
#include "morphos_glimp.h"

#include <unistd.h> // usleep - Cowcat

#define MAXIMSGS 32

int __stack = 1024*1024;

static int consoleinput;
static int consoleoutput;
static int consoleoutputinteractive;

static BPTR olddir;

int use_altivec;
//struct Library *SocketBase;
//struct Library *DynLoadBase;

/* For NET_Sleep */
int stdin_active = 0;

//BPTR in = Input();

static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };

cvar_t	*com_maxfps; // new Cowcat

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


void Sys_Quit()
{
	//CL_Shutdown();
	//IN_Shutdown(); // now in glimp - Cowcat

	/*
	if (SocketBase)
		CloseLibrary(SocketBase);

	if (DynLoadBase)
		CloseLibrary(DynLoadBase);
	*/

	if (consoleinput && consoleoutputinteractive)
		SetMode(Input(), 0);

	if (olddir)
	{
		UnLock(CurrentDir(olddir));
	}

	exit(0);
}

static void ErrorMessage(char *string)
{
	char msg[4096];

	snprintf(msg, sizeof(msg), "%s", string);

	if (consoleoutput)
		fprintf(stderr, "%s", msg);

	else
	{
		struct EasyStruct es;

		if (msg[strlen(msg)-1] == '\n')
			msg[strlen(msg)-1] = 0;

		es.es_StructSize = sizeof(es);
		es.es_Flags = 0;
		es.es_Title = (unsigned char *)"Jedi Outcast";
		es.es_TextFormat = (unsigned char *)msg;
		es.es_GadgetFormat = (unsigned char *)"Quit";

		EasyRequest(0, &es, 0, 0);
	}
}

void Sys_Error(const char *fmt, ...)
{
	va_list va;
	char msg[4096];

	strcpy(msg, "Sys_Error: ");

	va_start(va, fmt);
	vsnprintf(msg+strlen(msg), sizeof(msg)-strlen(msg), fmt, va);
	va_end(va);

	ErrorMessage(msg);

	Sys_Quit();
}

void Sys_Print(const char *msg)
{
	char cmsg[4096] = { 0 }; // MAXPRINTMSG

	if (!consoleoutput)
		return;

	Q_strncpyz( cmsg, msg, sizeof( cmsg ) );
	Q_StripColor( cmsg );

	fputs( cmsg, stdout );
}

qboolean Sys_LowPhysicalMemory()
{
	/* This is only used for touching memory, there's really no need for that... */

	return qtrue;
}

void Sys_BeginProfiling()
{
}

void Sys_InitStreamThread()
{
}

void Sys_ShutdownStreamThread()
{
}

void Sys_BeginStreamedFile(fileHandle_t f, int readAhead)
{
}

void Sys_EndStreamedFile(fileHandle_t f)
{
}

int Sys_StreamedRead(void *buffer, int size, int count, fileHandle_t f)
{
	return FS_Read( buffer, size * count, f );
}

void Sys_StreamSeek(fileHandle_t f, int offset, int origin)
{
	FS_Seek(f, offset, origin);
}

char *Sys_GetClipboardData()
{
	return 0;
}

int putenv __P((const char *name))
{
	return 1;
}


#if 0
sysEvent_t Sys_GetEvent(void)
{
	sysEvent_t	ev;

	#if 0

	if (eventHead > eventTail) 
	{
		eventTail++;
		return eventQue [(eventTail - 1) &MASK_QUED_EVENTS ];
	}

	// Console Commands
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
#endif


sysEvent_t Sys_GetEvent(void) // morphos way
{
	sysEvent_t ev;
	
	memset(&ev, 0, sizeof(ev));
	ev.evTime = Sys_Milliseconds();
	
	IN_GetEvent(&ev);

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

#if 0 // not used 

#define MAX_QUED_EVENTS		256
#define MASK_QUED_EVENTS 	(MAX_QUED_EVENTS - 1)

static sysEvent_t	eventQue[MAX_QUED_EVENTS] = {};
static sysEvent_t	*lastEvent = nullptr;
static uint32_t		eventHead = 0, eventTail = 0;

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

static void *game_library;


/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (game_library) 
		//dlclose(game_library);
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
	
	//game_library = Sys_LoadLibrary( fn );	//game_library = dlopen( fn, RTLD_NOW );
	game_library = dllLoadLibrary( fn, 0 );

//First try in mod directories. basepath -> homepath
	if (!game_library)
	{
		Com_Printf( "Sys_GetGameAPI(%s) failed uno:\n", fn);

		if (homepath[0])
		{
			//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
			Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
			
			fn = FS_BuildOSPath( homepath, gamedir, gamename);
			//game_library = Sys_LoadLibrary( fn );
			//game_library = dlopen( fn, RTLD_NOW );
			game_library = dllLoadLibrary( fn, 0 );
		}
	}

//Still couldn't find it.
	if (!game_library)
	{
		Com_Printf( "Sys_GetGameAPI(%s) failed dos:\n", fn);

		//Com_Printf( "Sys_GetGameAPI(%s) failed: \"%s\"\n", fn, Sys_LibraryError() );
		Com_Printf( "Sys_GetGameAPI(%s) failed:\n", fn);
		Com_Error( ERR_FATAL, "Couldn't load game" );
	}
	
	Com_Printf ( "Sys_GetGameAPI(%s): succeeded ...\n", fn );

	//GetGameAPI = (void *(*)(void *))Sys_LoadFunction (game_library, "GetGameAPI");
	//GetGameAPI = (void *(*)(void *))dlsym(game_library, (char *)"GetGameAPI");
	GetGameAPI = (void *(*)(void *))dllGetProcAddress(game_library, (char *)"GetGameAPI");

	if (!GetGameAPI)
	{
		Sys_UnloadGame ();
		//Com_Error( ERR_FATAL, "dlsym GetGameAPI failed\n");
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
	//dllEntry = ( void (*)( intptr_t (*)( intptr_t, ... ) ) )dlsym( game_library, (char *)"dllEntry" );
	//*entryPoint = (intptr_t (*)(int,...))dlsym( game_library, (char *)"vmMain" );
	dllEntry = ( void (*)( intptr_t (*)( intptr_t, ... ) ) )dllGetProcAddress( game_library, (char *)"dllEntry" );
	*entryPoint = (intptr_t (*)(int,...))dllGetProcAddress( game_library, (char *)"vmMain" );

	if ( !*entryPoint || !dllEntry )
	{
		//Sys_UnloadLibrary( game_library );
		//dlclose( game_library );
		dllFreeLibrary( game_library );
		return NULL;
	}
    
	dllEntry( systemcalls );
	return game_library;
}

/*****************************************************************************/


/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetCGameAPI (void)
{
	return (NULL);
}


void *Sys_GetRefAPI (void *parms) 
{
	return (NULL);
#if 0
	return (void *)GetRefAPI(REF_API_VERSION, parms);
#endif
}


static char *ProcessorName()
{
	ULONG CPUType = MACHINE_PPC;

	NewGetSystemAttrs(&CPUType,sizeof(CPUType),SYSTEMINFOTYPE_MACHINE,TAG_DONE);

	if (CPUType == MACHINE_PPC)
	{
		ULONG CPUVersion, CPURevision;

		if (NewGetSystemAttrs(&CPUVersion,sizeof(CPUVersion),SYSTEMINFOTYPE_PPC_CPUVERSION,TAG_DONE)
		 && NewGetSystemAttrs(&CPURevision,sizeof(CPURevision),SYSTEMINFOTYPE_PPC_CPUREVISION,TAG_DONE))
		{
			switch (CPUVersion)
			{
				case 0x0001:
					return (char *)"601";
				case 0x0003: 
					return (char *)"603";
				case 0x0004:
					return (char *)"604";
				case 0x0006:
					return (char *)"603E";
				case 0x0007:
					return (char *)"603R/603EV";
				case 0x0008:
					if ((CPURevision & 0xf000) == 0x2000)
					{
						if (CPURevision >= 0x2214)
							return (char *)"750CXE (G3)";
						else
							return (char *)"750CX (G3)";
					}
					else
						return (char *)"740/750 (G3)";
				case 0x0009:
					return (char *)"604E";
				case 0x000A:
					return (char *)"604EV";
				case 0x000C:
					if (CPURevision & 0x1000)
						return (char *)"7410 (G4)";
					else
						return (char *)"7400 (G4)";
				case 0x0039:
					return (char *)"970 (G5)";
				case 0x003C:
					return (char *)"970FX (G5)";
				case 0x8000:
					if (CPURevision > 0x0200)
						return (char *)"7451 (G4)";
					else
						return (char *)"7441/7450 (G4)";
				case 0x8001:
					return (char *)"7445/7455 (G4)";
				case 0x8002:
					return (char *)"7447/7457 (G4)";
				case 0x8003:
					return (char *)"7447A (G4)";
				case 0x8004:
					return (char *)"7448 (G4)";
				case 0x800C:
					return (char *)"7410 (G4)";
				default:
					return (char *)"Unknown";
			}
		}
	}

	return (char *)"Unknown";
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


void Sys_Init()
{
	Cvar_Set("arch", "morphos ppc");

	Cvar_Set("sys_cpustring", ProcessorName());

	Cmd_AddCommand ("in_restart", Sys_In_Restart_f);

	com_maxfps = Cvar_Get("com_maxfps", "60", CVAR_ARCHIVE); // originally 125 fps - Cowcat

	//IN_Init(); // now in glimp - Cowcat
}

void Sys_Sleep(int msec) // used in common.c/Com_Frame - never reached? - Cowcat
{
	if( msec == 0)
		return;

	if( msec < 0 )
		msec = 10;

	usleep(1000 * msec);
}

extern struct WBStartup *_WBenchMsg;

int main(int argc, char **argv)
{
	struct Resident *morphos;
	int r1, r2;

	int i, len;

	char *cmdline;
	void SetProgramPath( char *path);

	BPTR l;

	BPTR Lock;

	if (!_WBenchMsg)
	{
		consoleoutput = 1;

		if (IsInteractive(Output()))
			consoleoutputinteractive = 1;
	}

	l = Lock("PROGDIR:", ACCESS_READ);

	if (l)
		olddir = CurrentDir(l);

	morphos = FindResident("MorphOS");

	r1 = NewGetSystemAttrs(&r2, sizeof(r2), SYSTEMINFOTYPE_PPC_ALTIVEC);

	if (r1 == sizeof(r2) && r2 != 0 && morphos && (morphos->rt_Flags&RTF_EXTENDED) && (morphos->rt_Version > 1 || (morphos->rt_Version == 1 && morphos->rt_Revision >= 5)))
		use_altivec = 1;

	signal(SIGINT, SIG_IGN);

	//SocketBase = OpenLibrary("bsdsocket.library", 0);
	//DynLoadBase = OpenLibrary("dynload.library", 0);

#ifdef DEDICATED
	if (SocketBase == 0)
	{
		printf("Unable to open bsdsocket.library\n");
		return 0;
	}
#endif

	SetProgramPath(argv[0]);

	len = 1;

	for(i=1;i<argc;i++)
		len+= strlen(argv[i])+1;

	cmdline = (char *)malloc(len);

	if (cmdline == 0)
		Sys_Quit();

	cmdline[0] = 0;

	for(i=1;i<argc;i++)
	{
		strcat(cmdline, argv[i]);

		if (i != argc-1)
			strcat(cmdline, " ");
	}

	Com_Init(cmdline);

	//if (SocketBase)
		//NET_Init();

	if (!_WBenchMsg && IsInteractive(Input()))
	{
		consoleinput = 1;

		if (consoleoutputinteractive)
			SetMode(Input(), 1);
	}

	while((SetSignal(0, 0)&SIGBREAKF_CTRL_C) == 0)
	{
		Com_Frame();
	}

	Sys_Quit();

	return 0;
}

/*
cpuFeatures_t Sys_GetProcessorFeatures( void )
{
	return 0; //TODO: should return altivec if available
}
*/

char *Sys_ConsoleInput(void) // Cowcat
{
	return NULL;
}

qboolean Sys_RandomBytes( byte *string, int len ) // Cowcat
{
	return qfalse;
}
