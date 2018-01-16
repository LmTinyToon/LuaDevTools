/*
			Tiny lua dev tools, supplied for easy debugging of lua internals
*/

#ifndef LUA_DEV_TOOLS_INCLUDED_H__
#define LUA_DEV_TOOLS_INCLUDED_H__

#include "lua.h"
#include "lobject.h"

//	Public API
/*
		Dumps stack of lua state
		Params: lua state
		Return: none
*/
LUA_API void (ldv_dump_stack)(lua_State* L);

/*
		Dumps object
		Params: lua state, value
		Return: none
*/
LUA_API void (ldv_dump_value)(lua_State* L, TValue* value);

/*
		Dumps table
		Params: lua state, table
		Return: none
*/
LUA_API void (ldv_dump_table)(lua_State* L, Table* table);

/*
		Dumps lua closure
		Params: lua state, lua closure
		Return: none
*/
LUA_API void (ldv_dump_lua_closure)(lua_State* L, LClosure* lclosure);

/*
		Dumps c closure (c function with upvalues)
		Params: lua state, c closure
		Return: none
*/
LUA_API void (ldv_dump_c_closure)(lua_State* L, CClosure* cclosure);

/*
		Dumps light c function (vanilla c function without any upvalues)
		Params: lua state, light c function
		Return: none
*/
LUA_API void (ldv_dump_c_light_func)(lua_State* L, lua_CFunction* light_func);

/*
	Dumpts lua thread
	Params: lua state, lua thread
	Return: none
*/
LUA_API void (ldv_dump_thread)(lua_State* L, lua_State* lua_thread);

#endif