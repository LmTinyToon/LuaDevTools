//	Standard includes
#include <stdio.h>

//	Includes
#include "ldevtools.h"

//	Internal helpers
#define LDV_LOG(x) printf(x);

//	Public API implementation
LUA_API void (ldv_dump_stack)(lua_State* L)
{
	LDV_LOG("Hey, that's dumped stack")
	for (int i = 0; i < stacksize; ++i)
	{
		int type = isvalid(o) ? ttnov(o) : LUA_TNONE;
		LDV_LOG("%s \n", ttypename(type))
	}
}