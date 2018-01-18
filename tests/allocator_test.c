/*	Standard includes	*/
#include <stdio.h>
#include <stdarg.h>

/*	Includes	*/
#include "Lua.h"
#include "ldevtools.h"

int main()
{
	/*	Initializes ldv heap */
	ldv_initialize_heap();
	/*	Dumping ldv heap layout to test, whether it's ok*/
	ldv_dump_ldv_heap_layout();
	void* ptr1 = ldv_frealloc(0, 0, 0, 4);
	ldv_dump_ldv_heap_layout();
	void* ptr2 = ldv_frealloc(0, 0, 0, 4);
	ldv_dump_ldv_heap_layout();
	ldv_frealloc(0, ptr1, 4, 0);
	ldv_dump_ldv_heap_layout();
	return 0;
}
