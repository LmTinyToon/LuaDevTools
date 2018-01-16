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
		Dumps nil objects
		Params: lua state, nil object
		Return: none
*/
LUA_API void (ldv_dump_nil)(lua_State* L, TValue* nil_object);

/*
		Dumps boolean object
		Params: lua state, boolean object
		Return: none
*/
LUA_API void (ldv_dump_boolean)(lua_State* L, int bool_val);

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
LUA_API void (ldv_dump_c_light_func)(lua_State* L, lua_CFunction light_func);

/*
		Dumps lua thread
		Params: lua state, lua thread
		Return: none
*/
LUA_API void (ldv_dump_thread)(lua_State* L, lua_State* lua_thread);

/*
		Dumps user data
		Params: lua state, user data
		Return: none
*/
LUA_API void (ldv_dump_user_data)(lua_State* L, char* user_data);

/*
		Dumps light user data
		Params: lua state, light user data
		Return: none
*/
LUA_API void (ldv_dump_light_user_data)(lua_State* L, void* light_user_data);

/*
		Dumps integer lua anumber
		Params: lua state, lua integer number
		Return: none
*/
LUA_API void (ldv_dump_int_number)(lua_State* L, lua_Integer int_num);

/*
		Dumps float lua anumber
		Params: lua state, lua float number
		Return: none
*/
LUA_API void (ldv_dump_float_number)(lua_State* L, lua_Number float_num);

/*
		Dumps short string object
		Params: lua state, short lua string
		Return: none
*/
LUA_API void (ldv_dump_short_string)(lua_State* L, TString* string);

/*
		Dumps long string object
		Params: lua state, long lua string
		Return: none
*/
LUA_API void (ldv_dump_long_string)(lua_State* L, TString* string);

#endif