//	Standard includes
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

//	Includes
#include "ldevtools.h"
#include "lstate.h"
#include "ltable.h"
#include "lauxlib.h"

#ifdef _WIN32
	#include <windows.h>
#endif

//	Types
//		LDV byte type
typedef unsigned int ldv_block_type;

//	Data
#define LDV_UNUSED(x) (void)(x);
//		Maximal buffer size
#define MEM_BUFF_SIZE 1000000
//		Gets raw memory
#define RAW_MEMORY(x) ((ldv_block_type*)x)

/*	All states of head*/
typedef enum StateType
{
	DataState,		/* Data state of head (garbage or not) */
	NextHeadState,  /* Border head state */
	PrevHeadState   /* Border head state */
}   StateType;

/*	HeadType	*/
typedef enum HeadType
{
	NextHead,	/*	Next head after current head.	*/
	PrevHead	/*	Previous head before current head.	*/
} HeadType;

/*	Available statuses of states  */
typedef enum StateStatus
{
	Error,		/*  Such status indicates corruption. It is error, and it is not supposed to be returned */

	/*	Possible states of DataState	*/
	Gem,		/*	Data is actual	*/
	Garbage,	/*	Data represent garbage. It has any meaning.	*/

	/*	Possible states of NextHeadState/PrevHeadState	*/
	MarginHead,	/*	Head is located at border of available memory pool. Any changes of such head/data is forbided.	*/
	MiddleHead	/*	Head is located within memory pool. You can change head/data.	*/
} StateStatus;

//		Output char buffer
static char out_buff[1000];
//		Memory buffer
static ldv_block_type mem_buf[MEM_BUFF_SIZE] = { MEM_BUFF_SIZE };
  
/*
        Helper structure to mark blocks in memory buffer. It is used by memory manager
*/
typedef struct BlockHead
{
    /*  Index (offset) to next block    */
    ldv_block_type next_index;
    /*  Index (offset) to previous block    */
    ldv_block_type prev_index;
} BlockHead;

//	PUBLIC LUA API FUNCTIONS
/*===========PUBLIC LUA API BEGIN============*/
/*
		Prints dumpt of ldv memory heap
		Params: none
		Return: none
*/
static int dumpHeap(lua_State* L)
{
	LDV_UNUSED(L)
	ldv_dump_heap();
	return 0;
}

/*
		Checks ldv heap on erros
		Params: none
		Return: error code
*/
static int checkHeap(lua_State* L)
{
	LDV_UNUSED(L)
	const int err_code = ldv_check_heap(); 
	lua_pushinteger(L, err_code);
	return 1;
}

/*
		Dumps object
		Params: object to dump
		Return: none
		NOTE: (alex) in current implementation it is unsafe way to dump object
*/
static int dumpObject(lua_State* L)
{
	ldv_dump_value(L, L->top - 1);
	return 0;
}
/*===========PUBLIC LUA API END==============*/

//              Public functions available from LUA script
static const luaL_Reg ldv_tab_funcs[] =
{
  {"dumpHeap", dumpHeap},
  {"checkHeap", checkHeap},
  {"dumpObject", dumpObject},
  {NULL, NULL}
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
	return (1 << (sizeof(ldv_block_type) * 8) - 1) - 1;
}

/*
		Gets block flag mask
		Params: none
		Return: block flag mask
*/
static ldv_block_type block_flag_mask(void)
{
	return 1 << (sizeof(ldv_block_type) * 8 - 1);
}

/*
        Sets next/prev head to BlockHead
        Params: blocks info, head type, index
        Return: none
*/
static void set_head(BlockHead* binfo, HeadType head_type, ldv_block_type head)
{
	const int data_mask = block_data_mask();
	const int flag_mask = block_flag_mask();
	const ldv_block_type cl_head = head & data_mask;
	if (head_type == NextHead)
	{
		binfo->next_index = (valid_block(RAW_MEMORY(binfo) + cl_head) ? flag_mask : 0) | cl_head;
	}
	else
	{
		binfo->prev_index = (valid_block(RAW_MEMORY(binfo) - cl_head) ? cl_head : 0) | (flag_mask & binfo->prev_index);
	}
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
		Computes hold data size in bytes
		Params: block head
		Return: size of block head data
*/
static ldv_block_type data_size(BlockHead* bhead)
{
	return ((bhead->next_index & block_data_mask()) - 2) * sizeof(ldv_block_type);
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
			return (binfo->prev_index & (~mask)) != 0 ? MiddleHead : MarginHead;
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
		default:
			/*	TODO: (alex) add assertion here?	*/
			break;
	}
}

/*
		Gets block head with given offset
		Params: block, offset type, block offset
		Return: offset block head
*/
static BlockHead* raw_move_head(BlockHead* head, HeadType head_type, ldv_block_type offset)
{
	if (head_type == NextHead)
		return (BlockHead*)(RAW_MEMORY(head) + offset);
	return (BlockHead*)(RAW_MEMORY(head) - offset);
}

/*
		Frees memory allocated via ldv_malloc
		Params: pointer to memory
		Return: none
*/
static void ldv_free(void* ptr)
{
	if (ptr == NULL)
		return;
	BlockHead* b_info = (BlockHead*)(RAW_MEMORY(ptr) - 2);
	set_status(b_info, Garbage);
	ldv_block_type bheads_offsets[2] = {0, get_head_offset(b_info, PrevHead)};
	for (int i = 0; i < 2; ++i)
	{
		BlockHead* bhead = raw_move_head(b_info, PrevHead, bheads_offsets[i]);
		if (status(bhead, DataState) == Garbage && status(bhead, NextHeadState) == MiddleHead)
		{
			ldv_block_type next_offset = get_head_offset(bhead, NextHead);
			BlockHead* next_head = raw_move_head(bhead, NextHead, next_offset);
			if (status(next_head, DataState) == Garbage)
			{
				ldv_block_type next_next_offset = get_head_offset(next_head, NextHead);
				set_head(bhead, NextHead, next_offset + next_next_offset);
				if (status(next_head, NextHeadState) == MiddleHead)
				{
					set_head(raw_move_head(bhead, NextHead, next_offset + next_next_offset), PrevHead, next_offset + next_next_offset);
				}
			}
		}
	}
}

/*
		Finds best fitted memory to alloc
		Params: fit size
		Return: pointer to free memory
*/
static BlockHead* find_fit_head(size_t nsize)
{
	BlockHead* best_fit_head = 0;
	BlockHead* start_head = (BlockHead*)mem_buf;
	for (;;)
	{
		ldv_block_type next_offset = get_head_offset(start_head, NextHead);
		if (status(start_head, DataState) == Garbage)
		{
			const ldv_block_type start_d_size = data_size(start_head);
			if (start_d_size >= nsize)
			{
				if (best_fit_head == 0)
					best_fit_head = start_head;
				const ldv_block_type fit_d_size = data_size(best_fit_head);
				best_fit_head =  fit_d_size > start_d_size ? start_head : best_fit_head;
			}
		}
		if (status(start_head, NextHeadState) == MarginHead)
			break;
		start_head = raw_move_head(start_head, NextHead, next_offset);
	}
	return best_fit_head;
}

/*
		Allocates memory with size
		Params: size of needed memory
		Return: none
*/
static void* ldv_malloc(size_t nsize)
{
	BlockHead* fit_head = find_fit_head(nsize);
	if (fit_head == 0)
		return 0;
	set_status(fit_head, Gem);
	ldv_block_type next_head_off = get_head_offset(fit_head, NextHead);
	ldv_block_type next_block_off = 2 + nsize / sizeof(ldv_block_type) + ( nsize % sizeof(ldv_block_type) == 0 ? 0 : 1);
	/*	TODO: (alex) think about best way to solve problem (next_head - next_block == 1)*/
	if (next_block_off + 1 == next_head_off)
		next_block_off = next_head_off;
	BlockHead* next_block = raw_move_head(fit_head, NextHead, next_block_off);
	if (next_block_off < next_head_off)
	{	
		set_status(next_block, Garbage);
		set_head(next_block, NextHead, next_head_off - next_block_off);
		if (status(next_block, NextHeadState) == MiddleHead)
		{
			set_head(raw_move_head(fit_head, NextHead, next_head_off), PrevHead, next_head_off - next_block_off);
		}
	}
	set_head(fit_head, NextHead, next_block_off);
	if (status(fit_head, NextHeadState) == MiddleHead)
	{
		set_head(next_block, PrevHead, next_block_off);
	}
	return RAW_MEMORY(fit_head) + 2;
}

/*
	Loads ldv library
	Params: lua state
	Return: returns count
*/
static int ldv_load_libf(lua_State* L)
{
	luaL_newlib(L, ldv_tab_funcs);
  	return 1;
}

//	Public API implementation
void ldv_load_lib(lua_State* L)
{
	luaL_requiref (L, "ldv", ldv_load_libf, 1);
}

void ldv_clear_heap()
{
	if (MEM_BUFF_SIZE > block_data_mask())
	{
		ldv_log("Too big memory buffer size");
		return;
	}
	for (int i = 0; i < MEM_BUFF_SIZE; ++i)
		mem_buf[i] = 0;
	set_head((BlockHead*)(mem_buf), NextHead, MEM_BUFF_SIZE);
}

int ldv_check_heap()
{
	int code = 0;
	/*	Performs local check on valid heads	*/
	ldv_log("======	 Checking LDV HEADS  ============\n");
	BlockHead* start_head = (BlockHead*)mem_buf;
        for (unsigned int heads_count = 0; ; ++heads_count)
        {
                const ldv_block_type prev = get_head_offset(start_head, PrevHead);
                const ldv_block_type next = get_head_offset(start_head, NextHead);
		if (prev > MEM_BUFF_SIZE)
		{
			ldv_log("Corrupted prev index of block %p %i %i\n", start_head, heads_count, prev);
			code = 1;
		}
		if (next > MEM_BUFF_SIZE)
		{
			ldv_log("Corrupted next index of block %p %i %i\n", start_head, heads_count, next);
			code = 1;
		}
                if (status(start_head, NextHeadState) == MarginHead)
                        break;
                start_head = raw_move_head(start_head, NextHead, next);
		const ldv_block_type next_prev = get_head_offset(start_head, PrevHead);
		if (next != next_prev)
		{
			ldv_log("Inconsistent prev/next %i %i links (next head %p %i)", next, next_prev, start_head, heads_count);
			code = 1;
		}
        }
	ldv_log("=======================================\n");
	return code;
}

void* ldv_frealloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
	LDV_UNUSED(ud)
	if (nsize == 0)
	{
		ldv_free(ptr);
		return 0;
	}
	void* all_mem = ldv_malloc(nsize);
	if (ptr != 0 && osize != 0)
	{
		memcpy(all_mem, ptr, osize < nsize ? osize : nsize);
		ldv_free(ptr);
	}
	return all_mem;
}

void ldv_dump_heap()
{
	ldv_portion_dump(0, MEM_BUFF_SIZE);
}

void ldv_portion_dump(const unsigned int fst_head, const unsigned int count)
{
	BlockHead* start_head = (BlockHead*)mem_buf;
	ldv_log("======  LDV memory layout [%p, %p)         ===============\n", mem_buf, mem_buf + MEM_BUFF_SIZE);
	for (unsigned int heads_count = 0; ; ++heads_count)
	{
		if (heads_count < fst_head)
			continue;
		if (heads_count >= fst_head + count)
			break;
		ldv_block_type prev = get_head_offset(start_head, PrevHead);
		ldv_block_type next = get_head_offset(start_head, NextHead);
		const char* data_status = status(start_head, DataState) == Gem ? "GEM" : "GARBAGE";
		ldv_log("HEAD %i, address %p, prev %i, next %i, data type %s \n", heads_count, start_head, prev, next, data_status);
		if (status(start_head, NextHeadState) == MarginHead)
			break;
		start_head = raw_move_head(start_head, NextHead, next);
	}
	ldv_log("==========================================================\n");

}

void (ldv_dump_ldv_heap_at_mem)(const void* ptr)
{
	LDV_UNUSED(ptr)
	/* Not implemented */
}

void (ldv_dump_hash_strtable)(lua_State* L)
{
	stringtable* hash_string_table = &G(L)->strt;
	ldv_log("======= HASH STRING TABLE DUMP (nuse: %i) (size: %i) ==============\n", hash_string_table->nuse, hash_string_table->size);
	for (int i = 0; i < hash_string_table->size; ++i)
	{
		TString **p = &hash_string_table->hash[i];
		if (*p == NULL)
			continue;
		ldv_log("=========	List %i of elements	========\n", i);
		while (*p != NULL) 
		{
			ldv_dump_short_string(L, *p);
			p = &(*p)->u.hnext;
		}
		ldv_log("=======================================\n");
	}
	ldv_log("==========================================================\n");
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
	LDV_UNUSED(L) LDV_UNUSED(nil_object)
	ldv_log("Type: NIL OBJECT \n");
}

void (ldv_dump_boolean)(lua_State* L, int bool_val)
{
	LDV_UNUSED(L)
	ldv_log("Type: BOOLEAN \n");
	ldv_log("Value: %i \n", bool_val);
}

void (ldv_dump_table)(lua_State* L, Table* table)
{
	const int nsize = sizenode(table);
	ldv_log("Type: TABLE size(%i) \n", nsize + table->sizearray);
	ldv_log("===========  TABLE CONTENTS ===========\n");
	for (int i = 0; i < table->sizearray; ++i) 
	{
		ldv_log("Array key %i \n", i);
		ldv_dump_value(L, &table->array[i]);
    	}
	for (int i = 0; i < nsize; ++i)
	{
		Node* node = &table->node[i];
		ldv_log("Key of node: \n");
		ldv_dump_value(L, gkey(node));
		ldv_log("Value of node: \n");
		ldv_dump_value(L, gval(node));
	}
	ldv_log("===========  TABLE CONTENTS ===========\n");
}


void (ldv_dump_lua_closure)(lua_State* L, LClosure* lclosure)
{
	LDV_UNUSED(L) LDV_UNUSED(lclosure)
	ldv_log("Type: LUA CLOSURE \n");
}

void (ldv_dump_c_closure)(lua_State* L, CClosure* cclosure)
{
	LDV_UNUSED(L) LDV_UNUSED(cclosure)
	ldv_log("Type: C CLOSURE \n");
}

void (ldv_dump_c_light_func)(lua_State* L, lua_CFunction light_func)
{
	LDV_UNUSED(L)
	ldv_log("Type: LIGHT C FUNCTION \n");
	ldv_log("Value: %p \n", light_func);
}

void (ldv_dump_thread)(lua_State* L, lua_State* lua_thread)
{
	LDV_UNUSED(L)
	ldv_log("Type: LUA THREAD \n");
	ldv_log("Stack address %p, Top element %p Call Infos num %i", lua_thread->stack, lua_thread->top, lua_thread->nci);

}

void (ldv_dump_user_data)(lua_State* L, char* user_data)
{
	LDV_UNUSED(L)
	ldv_log("Type: USER DATA \n");
	ldv_log("Value: %p \n", user_data);
}

void (ldv_dump_light_user_data)(lua_State* L, void* light_user_data)
{
	LDV_UNUSED(L)
	ldv_log("Type: LIGHT USER DATA \n");
	ldv_log("Value: %p \n", light_user_data);
}

void (ldv_dump_int_number)(lua_State* L, lua_Integer int_num)
{
	LDV_UNUSED(L)
	ldv_log("Type: INTEGER NUMBER \n");
	ldv_log("Value: %i \n", int_num);
}

void (ldv_dump_float_number)(lua_State* L, lua_Number float_num)
{
	LDV_UNUSED(L)
	ldv_log("Type: FLOAT NUMBER \n");
	ldv_log("Value: %f \n", float_num);
}

void (ldv_dump_str)(lua_State* L, TString* str)
{
	/*	What should be located here???*/
	ldv_dump_short_string(L, str);
}

void (ldv_dump_short_string)(lua_State* L, TString* string)
{
	LDV_UNUSED(L)
	ldv_log("Type: SHORT STRING (%i) \n", tsslen(string));
	ldv_log("Value: %s \n", getstr(string));
}

void (ldv_dump_long_string)(lua_State* L, TString* string)
{
	LDV_UNUSED(L)
	ldv_log("Type: LONG STRING (%i) \n", tsslen(string));
	ldv_log("Value: %s \n", getstr(string));
}
