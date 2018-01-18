//	Standard includes
#include <stdio.h>
#include <stdarg.h>

//	Includes
#include "ldevtools.h"
#include "lstate.h"

#ifdef _WIN32
	#include <windows.h>
#endif

//	Types
//		LDV byte type
typedef unsigned int ldv_block_type;

//	Data
//		Maximal buffer size
#define MEM_BUFF_SIZE 1000000
//		Gets raw memory
#define RAW_MEMORY(x) ((ldv_block_type*)x)

/*	All states of head*/
enum StateType
{
	DataState,		/* Data state of head (garbage or not) */
	NextHeadState,  /* Border head state */
	PrevHeadState   /* Border head state */
};

/*	HeadType	*/
enum HeadType
{
	NextHead,	/*	Next head after current head.	*/
	PrevHead	/*	Previous head before current head.	*/
};

/*	Available statuses of states  */
enum StateStatus
{
	Error,		/*  Such status indicates corruption. It is error, and it is not supposed to be returned */

	/*	Possible states of DataState	*/
	Gem,		/*	Data is actual	*/
	Garbage,	/*	Data represent garbage. It has any meaning.	*/

	/*	Possible states of NextHeadState/PrevHeadState	*/
	MarginHead,	/*	Head is located at border of available memory pool. Any changes of such head/data is forbided.	*/
	MiddleHead	/*	Head is located within memory pool. You can change head/data.	*/
};

//		Output char buffer
static char out_buff[1000];
//		Index of first free block
static int  block_index = 0;
//		Memory buffer
static ldv_block_type mem_buf[MEM_BUFF_SIZE];

/*
        Helper structure to mark blocks in memory buffer. It is used by memory manager
*/
struct BlockHead
{
    /*  Index (offset) to next block    */
    ldv_block_type next_index;
    /*  Index (offset) to previous block    */
    ldv_block_type prev_index;
};

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

/*
		Checks, whether block is lied within memory pool
		Params: block
		Return: check result
*/
static int valid_block(ldv_block_type* block)
{
	return mem_buf <= block && block < mem_buf + MEM_BUFF_SIZE;
}

/*
		Gets data mask, which defines data layout in block info
		Params: none
		Return: block data mask
*/
static ldv_block_type block_data_mask(void)
{
	return (1 << sizeof(ldv_block_type) * 8) - 1;
}

/*
		Gets block flag mask
		Params: none
		Return: block flag mask
*/
static ldv_block_type block_flag_mask(void)
{
	return 1 << sizeof(ldv_block_type) * 8;
}

/*
        Sets next/prev head to BlockHead
        Params: blocks info, head type, index
        Return: none
*/
static void set_head(BlockHead* binfo, HeadType head_type, ldv_block_type head)
{
	int data_mask = block_data_mask();
    ldv_block_type* index_ptr = head_type == NextHead ? &binfo->next_index : &binfo->prev_index;
    *index_ptr = (valid_block(RAW_MEMORY(binfo) + (*index_ptr & data_mask)) ? data_mask : 0) | head & block_data_mask();
}

/*
		Gets head of head type
		Params: block head, head type
		Return: head of head type
*/
static ldv_block_type get_head_offset(BlockHead* bhead, HeadType head_type)
{
	int data_mask = block_data_mask();
	return head_type == NextHead ? bhead->next_index & data_mask : bhead->prev_index & data_mask;
}

/*
        Gets status of current state type
        Params: block info, state type
        Return: state status
*/
StateStatus status(BlockHead* binfo, StateType flag)
{
	int mask = block_flag_mask();
	switch (flag)
	{
		case DataState: 
			return binfo->prev_index & mask ? Gem : Garbage;
		case NextHeadState: 
			return binfo->next_index & mask ? MiddleHead : MarginHead;
		case PrevHeadState:
			return binfo->prev_index & (~mask) != 0 ? MiddleHead : MarginHead;
	}
	return Error;
}

/*
		Sets state status of block head
		Params: block head
		Return: none
*/
void set_status(BlockHead* binfo, StateStatus status)
{
	int mask = block_flag_mask();
	switch (status)
	{
		case Gem:
			binfo->prev_index = binfo->prev_index | mask;
			break;
		case Garbage:
			binfo->prev_index = binfo->prev_index & (~mask);
			break;
	}
}

/*
		Gets blocks info at raw pointer
		Params: raw pointer
		Return: blocks info
*/
static BlockHead* blocks_info(void* ptr)
{
	return (BlockHead*)(ptr) - 2;
}

/*
		Gets block head with given offset
		Params: block, block offset
		Return: offset block head
*/
static BlockHead* get_bhead(BlockHead* block, ldv_block_type offset)
{
	return (BlockHead*)(RAW_MEMORY(block) + offset);
}

/*
		Frees memory allocated via ldv_malloc
		Params: pointer to memory
		Return: none
*/
static void ldv_free(void* ptr)
{
	BlockHead* b_info = blocks_info(ptr);
	set_status(b_info, Garbage);
	ldv_block_type bheads_offsets[2] = {0, get_head_offset(b_info, PrevHead)};
	for (int i = 0; i < 2; ++i)
	{
		BlockHead* bhead = get_bhead(b_info, bheads_offsets[i]);
		if (status(bhead, DataState) == Garbage && status(bhead, NextHeadState) == MiddleHead)
		{
			ldv_block_type next_offset = get_head_offset(bhead, NextHead);
			BlockHead* next_head = get_bhead(bhead, next_offset);
			if (status(next_head, DataState) == Garbage)
			{
				ldv_block_type next_next_offset = get_head_offset(next_head, NextHead);
				set_head(bhead, NextHead, next_offset + next_next_offset);
				if (status(next_head, NextHeadState) == MiddleHead)
				{
					set_head(get_bhead(bhead, next_offset + next_next_offset), PrevHead, -next_offset - next_next_offset);
				}
			}
		}
	}
}

/*
		Finds best fitted memory to alloc
		Params: pointer to memory, fit size
		Return: pointer to free memory
*/
static BlockHead* find_fit_head(void* ptr, size_t nsize)
{
	BlockHead* best_fit_head = 0;
	BlockHead* start_head = blocks_info(mem_buf);
	while (status(start_head, NextHeadState) == MiddleHead)
	{
		ldv_block_type next_offset = get_head_offset(start_head, NextHead);
		if (status(start_head, DataState) == Garbage)
		{
			if ((next_offset - 1) * sizeof(ldv_block_type) >= nsize)
			{
				if (best_fit_head == 0)
					best_fit_head = start_head;
				best_fit_head = get_head_offset(best_fit_head, NextHead) > next_offset ? start_head : best_fit_head;
			}
		}
		start_head = get_bhead(start_head, next_offset);
	}
	return best_fit_head;
}

/*
		Frees memory allocated via ldv_malloc
		Params: pointer to memory
		Return: none
*/
static void* ldv_malloc(void* ptr, size_t nsize)
{
	BlockHead* fit_head = find_fit_head(ptr, nsize);
	if (fit_head == 0)
		return 0;
	/*	Allocation of memory */
	return 0;
}

/*
		LDV frealloc function
		Params: user data, ptr to data, original size, new size
		Return: ptr to freallocated memory

		NOTE: Freallocation works inside static memory pool.
		Such behaviour "simulates" "fixed" pointers during lua session.
*/
static void* ldv_frealloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	(void)(ud); (void)(osize);	/*Unused parameters*/
	if (nsize == 0)
	{
		ldv_free(ptr);
		return 0;
	}
	if (osize != 0)
	{
		ldv_free(ptr);
	}
	return ldv_malloc(ptr, nsize);
}

/*
		Initializes memory buffer
		Params: none
		Return: error code
*/
static int ldv_init_memory()
{
	if (MEM_BUFF_SIZE > block_data_mask())
	{
		ldv_log("Too big memory buffer size");
		return 1;
	}
	for (int i = 0; i < MEM_BUFF_SIZE; ++i)
		mem_buf[i] = 0;
	block_index = 0;
	mem_buf[block_index] = 1 << sizeof(ldv_block_type) | MEM_BUFF_SIZE;
	return 0;
}

//	Public API implementation
lua_State* ldv_new_debug_lua_state()
{
	if (ldv_init_memory() != 0)
		return 0;
	return lua_newstate(ldv_frealloc, 0);
}

void ldv_dump_call_infos(lua_State* L)
{
	ldv_log("=======           LUA CALL INFO DUMP       ==============\n");
	CallInfo* ci = &(L->base_ci);
	int index = 0;
	while (ci != 0)
	{
		ldv_log("Call info: %p, level: %i, results: %i \n", ci, index, ci->nresults);
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
		ldv_log("STACK ELEMENT:   Address: %p Index: %i \n", stack_elem, i);
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
	ldv_log("Stack address %p, Top element %p Call Infos num %i", lua_thread->stack, lua_thread->top, lua_thread->nci);

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
