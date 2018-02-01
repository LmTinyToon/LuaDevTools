/*
			Tiny lua dev tools, supplied for easy debugging of lua internals
*/

#ifndef LUA_DEV_TOOLS_INCLUDED_H__
#define LUA_DEV_TOOLS_INCLUDED_H__

#include "lua.h"
#include "lobject.h"

//	Public API
/*
		Loads LDV library (to use functions from library)
		Params: lua state
		Return: none
*/
LUA_API void (ldv_load_lib)(lua_State* L);

/*
		Clears custom ldv heap 
		Params: none
		Return: none
*/
LUA_API void (ldv_clear_heap)();

/*
		Performs local checks on ldv heap
		Params: none
		Return: error code (0 - success)
*/
LUA_API int (ldv_check_heap)();

/*
		Checks pointers in lua objects
		Params: lua state
		Return: error code
*/
LUA_API int (ldv_check_ptrs)(lua_State* L);

/*
		LDV frealloc function
		Params: user data, ptr to data, original size, new size
		Return: ptr to freallocated memory

		NOTE: Freallocation works inside static memory pool.
		Such behaviour "simulates" "fixed" pointers during lua session.
*/
LUA_API void* (ldv_frealloc)(void* ud, void* ptr, size_t osize, size_t nsize);

/*
		Dumps layout of ldv heap
		Params: none
		Return: none
*/
LUA_API void (ldv_dump_heap)();

/*
		Dumps portion layout of ldv heap by heads
		Params: index of start head, heads count
		Return: none
*/
LUA_API void (ldv_portion_dump)(const unsigned int start_head, const unsigned int heads_count);

/*
		Dumps ldv heap at memory
		Params: pointer
		Return: none
*/
LUA_API void (ldv_dump_ldv_heap_at_mem)(const void* ptr);

/*
		Dumpts hash table for strings of global state
		Params: Lua state
		Return: none
*/
LUA_API void (ldv_dump_hash_strtable)(lua_State* L);

/*
		Dumps information about backtrace
		Params: lua state
		Return: none
*/
LUA_API void ldv_bt(lua_State* L);

/*
		Dumps info frame at specified index, depth
		Params: lua state, frame index, depth
		Return: none
*/
LUA_API void ldv_f(lua_State* L, const int frame_index, const int depth);

/*
		Dumpts tops values from lua stack
		Params: lua state, count of top values
		Return: none
*/
LUA_API void (ldv_dump_tops)(lua_State* L, const int tops);

/*
		Dumps upvalue 
		Params: lua state, upvalue
		Params: none
*/
LUA_API void (ldv_dump_upvalue)(lua_State* L, const UpVal* upval);

/*
		Dumps object
		Params: dump depth, lua state, value
		Return: none
*/
LUA_API void ldv_dump_value(const int depth, lua_State* L, const TValue* value);

/*
		Dumps nil objects
		Params: lua state, nil object
		Return: none
*/
LUA_API void ldv_dump_nil(const int depth, lua_State* L, const TValue* nil_object);

/*
		Dumps boolean object
		Params: lua state, boolean object
		Return: none
*/
LUA_API void ldv_dump_boolean(const int depth, lua_State* L, const int bool_val);

/*
		Dumps table
		Params: lua state, table
		Return: none
*/
LUA_API void ldv_dump_table(const int depth, lua_State* L, const Table* table);

/*
		Dumps lua closure
		Params: lua state, lua closure
		Return: none
*/
LUA_API void ldv_dump_lua_closure(const int depth, lua_State* L, const LClosure* lclosure);

/*
		Dumps function prototype
		Params: lua state, proto
		Return: none
*/
LUA_API void ldv_dump_proto(const int depth, lua_State* L, const Proto* proto);

/*
		Dumps c closure (c function with upvalues)
		Params: lua state, c closure
		Return: none
*/
LUA_API void ldv_dump_c_closure(const int depth, lua_State* L, const CClosure* cclosure);

/*
		Dumps light c function (vanilla c function without any upvalues)
		Params: lua state, light c function
		Return: none
*/
LUA_API void ldv_dump_c_light_func(const int depth, lua_State* L, const lua_CFunction light_func);

/*
		Dumps lua thread
		Params: space indent, lua state, lua thread
		Return: none
*/
LUA_API void ldv_dump_thread(const int depth, lua_State* L, lua_State* lua_thread);

/*
		Dumps user data
		Params: lua state, user data
		Return: none
*/
LUA_API void ldv_dump_user_data(const int depth, lua_State* L, const char* user_data);

/*
		Dumps light user data
		Params: lua state, light user data
		Return: none
*/
LUA_API void ldv_dump_light_user_data(const int depth, lua_State* L, const void* light_user_data);

/*
		Dumps integer lua anumber
		Params: lua state, lua integer number
		Return: none
*/
LUA_API void ldv_dump_int_number(const int depth, lua_State* L, const lua_Integer int_num);

/*
		Dumps float lua anumber
		Params: lua state, lua float number
		Return: none
*/
LUA_API void ldv_dump_float_number(const int depth, lua_State* L, const lua_Number float_num);

/*
		Dumps short string object
		Params: lua state, short lua string
		Return: none
*/
LUA_API void ldv_dump_short_string(const int depth, lua_State* L, const TString* string);

/*
		Dumps long string object
		Params: lua state, long lua string
		Return: none
*/
LUA_API void ldv_dump_long_string(const int depth, lua_State* L, const TString* string);

#endif
