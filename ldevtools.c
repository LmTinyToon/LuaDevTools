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
void (ldv_dump_call_infos)(lua_State* L)
{
	ldv_log("=======           LUA CALL INFO DUMP       ==============\n");
	CallInfo* ci = &(L->base_ci);
	int index = 0;
	while (ci != 0)
	{
		ldv_log("Call info: %p, index: %i, results: %i \n", ci, index, ci->nresults);
		ldv_log("Func: %p, Top stack elem: %p \n", ci->func, ci->top);
		ci = ci->next;
		++index;
	}
	ldv_log("=========================================================\n");
}

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
		
		case LUA_TNIL:			 ldv_dump_nil(L, value);							return;
		case LUA_TBOOLEAN:		 ldv_dump_boolean(L, bvalue(value));				return;
		case LUA_TTABLE:		 ldv_dump_table(L, hvalue(value));					return;	
		case LUA_TLCL:			 ldv_dump_lua_closure(L, clLvalue(value));			return;	
		case LUA_TCCL:			 ldv_dump_c_closure(L, clCvalue(value));			return;
		case LUA_TLCF:			 ldv_dump_c_light_func(L, fvalue(value));			return;
		case LUA_TNUMFLT:		 ldv_dump_float_number(L, fltvalue(value));			return;
		case LUA_TNUMINT:		 ldv_dump_int_number(L, ivalue(value));				return;
		case LUA_TTHREAD:		 ldv_dump_thread(L, thvalue(value));				return;
		case LUA_TUSERDATA:		 ldv_dump_user_data(L, getudatamem(uvalue(value))); return;
		case LUA_TLIGHTUSERDATA: ldv_dump_light_user_data(L, pvalue(value));		return;
		case LUA_TSHRSTR:		 ldv_dump_short_string(L, tsvalue(value));			return;
		case LUA_TLNGSTR:		 ldv_dump_long_string(L, tsvalue(value));			return;
		default: 
			ldv_log("(ldv_dump_value func). Not recognized type: %s \n", ttypename(ttnov(value)));
			return;
	}
}

void (ldv_dump_nil)(lua_State* L, TValue* nil_object)
{
	ldv_log("Type: NIL OBJECT \n");
}

void (ldv_dump_boolean)(lua_State* L, int bool_val)
{
	ldv_log("Type: BOOLEAN \n");
	ldv_log("Value: %i \n", bool_val);
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

void (ldv_dump_c_light_func)(lua_State* L, lua_CFunction light_func)
{
	ldv_log("Type: LIGHT C FUNCTION \n");
	ldv_log("Value: %p \n", light_func);
}

void (ldv_dump_thread)(lua_State* L, lua_State* lua_thread)
{
	ldv_log("Type: LUA THREAD \n");
}

void (ldv_dump_user_data)(lua_State* L, char* user_data)
{
	ldv_log("Type: USER DATA \n");
	ldv_log("Value: %p \n", user_data);
}

void (ldv_dump_light_user_data)(lua_State* L, void* light_user_data)
{
	ldv_log("Type: LIGHT USER DATA \n");
	ldv_log("Value: %p \n", light_user_data);
}

void (ldv_dump_int_number)(lua_State* L, lua_Integer int_num)
{
	ldv_log("Type: INTEGER NUMBER \n");
	ldv_log("Value: %i \n", int_num);
}

void (ldv_dump_float_number)(lua_State* L, lua_Number float_num)
{
	ldv_log("Type: FLOAT NUMBER \n");
	ldv_log("Value: %f \n", float_num);
}

void (ldv_dump_short_string)(lua_State* L, TString* string)
{
	ldv_log("Type: SHORT STRING \n");
}

void (ldv_dump_long_string)(lua_State* L, TString* string)
{
	ldv_log("Type: LONG STRING \n");
}