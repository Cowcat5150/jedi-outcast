#include <stdlib.h>
//#include <dlfcn.h>
#include "dll.h"

#if 1

#include "../../codeJK2/game/g_local.h"
#include "../../codeJK2/game/g_public.h"

intptr_t vmMain( intptr_t command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4, intptr_t arg5, intptr_t arg6, intptr_t arg7 );
void dllEntry( intptr_t (QDECL *syscallptr)( intptr_t arg, ... ) );
game_export_t * QDECL GetGameAPI( game_import_t *import );

dll_tExportSymbol DLL_ExportSymbols[]=
{
	{(game_export_t *)GetGameAPI, (char *)"GetGameAPI"},
	{(intptr_t *)vmMain, (char *)"vmMain"},
	{(void *)dllEntry, (char *)"dllEntry"},
	{0, 0}
};


dll_tImportSymbol DLL_ImportSymbols[] =
{
	{0, 0, 0, 0}
};

int DLL_Init()
{
	return 1;
}

void DLL_DeInit()
{
}


#endif
