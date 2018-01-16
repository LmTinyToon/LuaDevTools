//	Standard includes
#include <stdio.h>
#include <stdarg.h>

//	Includes
#include "ldevtools.h"
#include "lstate.h"

#ifdef _WIN32
	#include <windows.h>
#endif

//	Output char buffer
static char out_buff[1000];

//	Internal helpers
/*
		Logs message
		Params: format message, variadic args
		Return: none
*/
static void ldv_log(const char* format, ...)
{
	va_list args;
	va_start (args, format);
	vsprintf(out_buff, format, args);
	va_end (args);

#ifdef _WIN32
	OutputDebugStringA(out_buff);
#endif

	printf(out_buff);
}

//	Public API implementation
void (ldv_dump_stack)(lua_State* L)
{
	ldv_log("=======           LUA STACK DUMP           ==============\n");
	ldv_log("LUA STACK DUMP. address:%p size: %i objects. \n", L->stack, L->stacksize);
	for (int i = 0; i < L->stacksize; ++i)
	{
		StkId stack_elem = L->stack + i;
		ldv_log("Element index: %i \n", i);
		ldv_dump_value(L, stack_elem);
	}
	ldv_log("=========================================================\n");
}

void (ldv_dump_value)(lua_State* L, TValue* value)
{
	switch (ttype(value))
	{
		case LUA_TTABLE:		 ldv_dump_table(L, hvalue(value));					return;
		case LUA_TLCL:			 ldv_dump_lua_closure(L, clLvalue(value));			return;
		case LUA_TCCL:			 ldv_dump_c_closure(L, clCvalue(value));			return;
		case LUA_TLCF:			 ldv_dump_c_light_func(L, fvalue(value));			return;
		case LUA_TNUMFLT:		 ldv_dump_float_number(L, fltvalue(value));			return;
		case LUA_TNUMINT:		 ldv_dump_int_number(L, ivalue(value));				return;
		case LUA_TTHREAD:		 ldv_dump_thread(L, thvalue(value));				return;
		case LUA_TUSERDATA:		 ldv_dump_user_data(L, getudatamem(uvalue(value))); return;
		case LUA_TLIGHTUSERDATA: ldv_dump_light_user_data(L, pvalue(value));		return;
		default: 
			ldv_log("(ldv_dump_value func). Not recognized type: %s \n", ttypename(ttnov(value)));
			return;
	}
}

void (ldv_dump_table)(lua_State* L, Table* table)
{
	ldv_log("Type: TABLE \n");
}


void (ldv_dump_lua_closure)(lua_State* L, LClosure* lclosure)
{
	ldv_log("Type: LUA CLOSURE \n");
}

void (ldv_dump_c_closure)(lua_State* L, CClosure* cclosure)
{
	ldv_log("Type: C CLOSURE \n");
}

void (ldv_dump_c_light_func)(lua_State* L, lua_CFunction* light_func)
{
	ldv_log("Type: LIGHT C FUNCTION \n");
}

void (ldv_dump_thread)(lua_State* L, lua_State* lua_thread)
{
	ldv_log("Type: LUA THREAD \n");
}

void (ldv_dump_user_data)(lua_State* L, char* user_data)
{
	ldv_log("Type: USER DATA \n");
}

void (ldv_dump_light_user_data)(lua_State* L, void* light_user_data)
{
	ldv_log("Type: LIGHT USER DATA \n");
}

void (ldv_dump_int_number)(lua_State* L, lua_Integer int_num)
{
	ldv_log("Type: INTEGER NUMBER \n");
}

void (ldv_dump_float_number)(lua_State* L, lua_Number float_num)
{
	ldv_log("Type: FLOAT NUMBER \n");
}