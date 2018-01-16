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
	StkId stack = L->stack;
	const int stack_size = L->stacksize;
	ldv_log("LUA STACK DUMP. address:%p size: %i objects. \n", L->stack, stack_size);
	for (int i = 0; i < stack_size; ++i)
	{
		StkId stack_elem = stack + i;
		int type = ttnov(stack_elem);
		ldv_log("Element: %i Type: %s \n", i, ttypename(type));
	}
}