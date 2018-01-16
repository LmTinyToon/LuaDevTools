/*
			Tiny lua dev tools, supplied for easy debugging of lua internals
*/

#ifndef LUA_DEV_TOOLS_INCLUDED_H__
#define LUA_DEV_TOOLS_INCLUDED_H__

#include "lua.h"

//	Public API
/*
		Dumps stack of lua state
		Params: lua state
		Return: none
*/
LUA_API void (ldv_dump_stack)(lua_State* L);

#endif