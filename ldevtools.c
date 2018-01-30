//	Standard includes
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

//	Includes
#include "ldevtools.h"
#include "lstate.h"
#include "ltable.h"
#include "lauxlib.h"
#include "lfunc.h"

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
//		Maximal dumping depth
#define MAX_DUMP_DEPTH 100000
//		Size of indent in characters
#define INDENT_SIZE 2
//		LDV assertions
#define LDV_ASSERT(x) assert(x);

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
	ldv_dump_value(0, MAX_DUMP_DEPTH, L, L->top - 1);
	return 0;
}

/*
		Check objects 
		Params: none
		Return: none
*/
static int checkObjects(lua_State* L)
{
	ldv_check_ptrs(L);
	return 0;
}
/*===========PUBLIC LUA API END==============*/

//              Public functions available from LUA script
static const luaL_Reg ldv_tab_funcs[] =
{
  {"dumpHeap", dumpHeap},
  {"checkHeap", checkHeap},
  {"dumpObject", dumpObject},
  {"checkObjects", checkObjects},
  {NULL, NULL}
};


//	Internal helpers
/*
		Logs message
		Params: indent value, format message, variadic args
		Return: none
*/
static void ldv_log(const int indent, const char* format, ...)
{
	for (int i = 0; i < indent; ++i)
		out_buff[i] = ' ';
	va_list args;
	va_start (args, format);
	vsprintf(out_buff + indent, format, args);
	va_end (args);

#ifdef _WIN32
	OutputDebugStringA(out_buff);
#endif

	printf(out_buff);
}

/*
		Checks, whether ptr is valid
		Params: none
		Return: valid flag (zero means not valid ptr)
*/
static int check_ptr(const void* ptr)
{
	const int check_res = mem_buf <= ptr && ptr < mem_buf + MEM_BUFF_SIZE;
	LDV_ASSERT(check_res)
	return check_res;
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
	LDV_ASSERT(check_ptr(binfo))
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
	LDV_ASSERT(check_ptr(bhead))
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
	LDV_ASSERT(check_ptr(bhead))
	return ((bhead->next_index & block_data_mask()) - 2) * sizeof(ldv_block_type);
}

/*
		Checks data, pointed with "sized" ptr
		Params: pointer to raw data, min size of data
		Return: check result
*/
static int check_sized_ptr(const void* ptr, const unsigned int min_size)
{
	ldv_block_type* raw_data = RAW_MEMORY(ptr) - 2;
	const int check_location = mem_buf <= raw_data && raw_data < mem_buf + MEM_BUFF_SIZE;
	LDV_ASSERT(check_location)
	if (!check_location)
		return 0;
	const ldv_block_type next = get_head_offset((BlockHead*)raw_data, NextHead);
	const int check_size = next * sizeof(ldv_block_type) >= min_size + sizeof(ldv_block_type) * 2;
	LDV_ASSERT(check_size)
	return check_size;
}

/*
        Gets status of current state type
        Params: block info, state type
        Return: state status
*/
StateStatus status(BlockHead* binfo, StateType flag)
{
	LDV_ASSERT(check_ptr(binfo))
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
	LDV_ASSERT(check_ptr(binfo))
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
	LDV_ASSERT(check_ptr(head))
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
	LDV_ASSERT(check_ptr(ptr))
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

/*			PRIVATE FUNCS				*/
/*
		Checks validity of table 
		Params: lua state, table
		Return: valid state
*/
int check_table(lua_State* L, const Table* table)
{
	if (table->array && !check_ptr(table->array) || table->node && !check_ptr(table->node))
		return 0;
	for (unsigned int i = 0; i < table->sizearray; ++i)
	{
		if (!check_ptr(table->array + i))
			return 0;
	}
	for (unsigned int i = 0; i < table->lsizenode; ++i)
	{
		if (!check_ptr(table->node + i))
			return 0;	
	}
	if (table->metatable && !check_ptr(table->metatable))
		return 0;
	return table->metatable && check_table(L, table->metatable);
}

/*
		Checks upvalue 
		Params: lua state, upvalue
		Return: valid state
*/
int check_upvalue(lua_State* L, const UpVal* upval)
{
	const UpVal* val = upval;
	while (val != NULL)
	{
		if (!check_ptr(val) || !check_ptr(val->v))
			return 0;
		val = upisopen(val) ? val->u.open.next : NULL;
	}
	return 1;
}

/*
		Checks validity of cclosure 
		Params: lua state, cclosure
		Return: valid state
*/
int check_cclosure(lua_State* L, const CClosure* cclosure)
{
	return 1;
}

/*
		Checks validity of string 
		Params: lua state, string
		Return: valid state
*/
int check_string(lua_State* L, const TString* str)
{
	return 1;
}

/*
		Checks validity of proto 
		Params: lua state, proto
		Return: valid state
*/
int check_proto(lua_State* L, const Proto* proto)
{
	if (proto->source && !check_ptr(proto->source))
		return 0;
	if (proto->cache && !check_ptr(proto->cache))
		return 0;
	for (int i = 0; i < proto->sizep; ++i)
	{
		if (!check_ptr(proto->p + i) || proto->p[i] && !check_ptr(proto->p[i]))
			return 0;
	}
	for (int i = 0; i < proto->sizecode; ++i)
	{
		if (!check_ptr(proto->code + i))
			return 0;
	}
	for (int i = 0; i < proto->sizelocvars; ++i)
	{
		if (!check_ptr(proto->locvars + i))
			return 0;
	}
	for (int i = 0; i < proto->sizeupvalues; ++i)
	{
		if (!check_ptr(proto->upvalues + i))
			return 0;
	}
	return 1;
}

/*
		Checks validity of lua closure
		Params: lua state, lua closure
		Return: valid state
*/
int check_lclosure(lua_State* L, const LClosure* lclosure)
{
	for (unsigned int i = 0; i < lclosure->nupvalues; ++i)
	{
		if (!check_ptr(lclosure->upvals + i))
			return 0;
		check_upvalue(L, lclosure->upvals[i]);
	}
	return check_proto(L, lclosure->p);
}

/*
		Check gc object on validness
		Params: lua state, gc object
		Return: valid state
*/
int check_gcobject(lua_State* L, const GCObject* gcobj)
{
	
	if (!check_sized_ptr(gcobj, sizeof(GCObject)))
		return 0;
	switch (gcobj->tt)
	{
		case LUA_TTABLE:
		{
			const Table* table = gco2t(gcobj);
			return check_sized_ptr(table, sizeof(Table)) ? check_table(L, table) : 0;
		}
		case LUA_TLCL:
		{
			const LClosure* lclosure = gco2lcl(gcobj);
			return check_sized_ptr(lclosure, sizeof(LClosure)) ? check_lclosure(L, lclosure) : 0;
		}
		case LUA_TCCL:
		{	
			const CClosure* cclosure = gco2ccl(gcobj);
			return check_sized_ptr(cclosure, sizeof(CClosure)) ? check_cclosure(L, cclosure) : 0;
		}
		case LUA_TUSERDATA:
		{
			/*	TODO: (alex) how to check?	*/
			return 1;
		}
		case LUA_TLCF:
		{
			/*	TODO: (alex) how to check?	*/
			return 1;
		}
		case LUA_TSTRING:
		{
			const TString* str = gco2ts(gcobj);
			return check_sized_ptr(str, sizeof(TString)) ? check_string(L, str) : 0;
		}
		case LUA_TLNGSTR:
		{
			const TString* str = gco2ts(gcobj);
			return check_sized_ptr(str, sizeof(TString)) ? check_string(L, str) : 0;
		}
		case LUA_TPROTO:
		{
			const Proto* proto = gco2p(gcobj);
			return check_sized_ptr(proto, sizeof(Proto)) ? check_proto(L, proto) : 0;
		}
		default:
			ldv_log(0, "(check_gcobject func). Not recognized type: %i \n", gcobj->tt);
			return 0;
	}
}
/*			PRIVATE FUNCS END			*/	

//	Public API implementation
void ldv_load_lib(lua_State* L)
{
	luaL_requiref (L, "ldv", ldv_load_libf, 1);
	lua_pop(L, 1);  /* remove lib */
}

void ldv_clear_heap()
{
	if (MEM_BUFF_SIZE > block_data_mask())
	{
		ldv_log(0, "Too big memory buffer size");
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
	ldv_log(0, "======	 Checking LDV HEADS  ============\n");
	BlockHead* start_head = (BlockHead*)mem_buf;
        for (unsigned int heads_count = 0; ; ++heads_count)
        {
                const ldv_block_type prev = get_head_offset(start_head, PrevHead);
                const ldv_block_type next = get_head_offset(start_head, NextHead);
		if (prev > MEM_BUFF_SIZE)
		{
			ldv_log(0, "Corrupted prev index of block %p %i %i\n", start_head, heads_count, prev);
			code = 1;
		}
		if (next > MEM_BUFF_SIZE)
		{
			ldv_log(0, "Corrupted next index of block %p %i %i\n", start_head, heads_count, next);
			code = 1;
		}
                if (status(start_head, NextHeadState) == MarginHead)
                        break;
                start_head = raw_move_head(start_head, NextHead, next);
		const ldv_block_type next_prev = get_head_offset(start_head, PrevHead);
		if (next != next_prev)
		{
			ldv_log(0, "Inconsistent prev/next %i %i links (next head %p %i)", next, next_prev, start_head, heads_count);
			code = 1;
		}
        }
	ldv_log(0, "=======================================\n");
	return code;
}

int ldv_check_ptrs(lua_State* L)
{
	if (!check_ptr(L))
		return 0;
	global_State* g = G(L);
	if (!check_ptr(g))
		return 0;
	GCObject* gobjects[10] = { g->allgc, g->sweepgc == NULL ? NULL : *(g->sweepgc), g->finobj, 
							   g->gray, g->grayagain, g->weak, g->ephemeron, 
							   g->allweak, g->tobefnz, g->fixedgc};
	for (int i = 0; i < 10; ++i)
	{
		GCObject* gcobj = gobjects[i];
		while (gcobj != NULL)
		{
			if (!check_gcobject(L, gcobj))
				return 0;
			gcobj = gcobj->next;
		}
	}
	return 1;
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
	LDV_ASSERT(check_ptr(all_mem))
	if (ptr != 0 && osize != 0)
	{
		LDV_ASSERT(check_ptr(ptr))
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
	ldv_log(0, "======  LDV memory layout [%p, %p)         ===============\n", mem_buf, mem_buf + MEM_BUFF_SIZE);
	for (unsigned int heads_count = 0; ; ++heads_count)
	{
		if (heads_count < fst_head)
			continue;
		if (heads_count >= fst_head + count)
			break;
		ldv_block_type prev = get_head_offset(start_head, PrevHead);
		ldv_block_type next = get_head_offset(start_head, NextHead);
		const char* data_status = status(start_head, DataState) == Gem ? "GEM" : "GARBAGE";
		ldv_log(0, "HEAD %i, address %p, prev %i, next %i, data type %s \n", heads_count, start_head, prev, next, data_status);
		if (status(start_head, NextHeadState) == MarginHead)
			break;
		start_head = raw_move_head(start_head, NextHead, next);
	}
	ldv_log(0, "==========================================================\n");

}

void (ldv_dump_ldv_heap_at_mem)(const void* ptr)
{
	LDV_UNUSED(ptr)
	/* Not implemented */
}

void (ldv_dump_hash_strtable)(lua_State* L)
{
	stringtable* hash_string_table = &G(L)->strt;
	ldv_log(0, "======= HASH STRING TABLE DUMP (nuse: %i) (size: %i) ==============\n", hash_string_table->nuse, hash_string_table->size);
	for (int i = 0; i < hash_string_table->size; ++i)
	{
		TString **p = &hash_string_table->hash[i];
		if (*p == NULL)
			continue;
		ldv_log(INDENT_SIZE, "=========	List %i of elements	========\n", i);
		while (*p != NULL) 
		{
			ldv_dump_short_string(INDENT_SIZE, 1, L, *p);
			p = &(*p)->u.hnext;
		}
		ldv_log(INDENT_SIZE, "=======================================\n");
	}
	ldv_log(0, "==========================================================\n");
}

void ldv_dump_call_infos(lua_State* L)
{
	ldv_log(0, "=======           LUA CALL INFO DUMP       ==============\n");
	CallInfo* ci = &(L->base_ci);
	int index = 0;
	while (ci != 0)
	{
		ldv_log(0, "Call info: %p, level: %i, results: %i \n", ci, index, ci->nresults);
		ldv_log(0, "Func: %p, Top stack elem: %p \n", ci->func, ci->top);
		ci = ci->next;
		++index;
	}
	ldv_log(0, "=========================================================\n");
}

void ldv_dump_stack(lua_State* L, const int depth)
{
	ldv_log(0, "=======           LUA STACK DUMP           ==============\n");
	ldv_log(0, "LUA STACK DUMP. address:%p size: %i objects. \n", L->stack, L->stacksize);
	for (int i = 0; i < L->stacksize; ++i)
	{
		StkId stack_elem = L->stack + i;
		ldv_log(0, "STACK ELEMENT:   Address: %p Index: %i \n", stack_elem, i);
		ldv_dump_value(0, depth, L, stack_elem);
	}
	ldv_log(0, "=========================================================\n");
}

void ldv_dump_tops(lua_State* L, const int tops, const int depth)
{
	ldv_log(0, "=========TOPS: %i===========\n", tops);
	for (int i = 1; i < tops; ++i)
		ldv_dump_value(0, depth, L, L->top - i);
	ldv_log(0, "===============================\n");
}

void ldv_dump_upvalue(const int indent, const int depth, lua_State* L, const UpVal* upval)
{
	if (depth == 0)
		return;
	ldv_log(indent, "Type: UPVALUE, REF COUNT: %i, OPEN: %i\n", upval->refcount, upisopen(upval));
	ldv_dump_value(indent, depth - 1, L, upval->v);
}

void ldv_dump_value(const int indent, const int depth, lua_State* L, const TValue* value)
{
	if (depth == 0)
		return;
	const int next_indent = indent + INDENT_SIZE;
	switch (ttype(value))
	{

		case LUA_TNIL:			 ldv_dump_nil(next_indent, depth - 1, L, value);							return;
		case LUA_TBOOLEAN:		 ldv_dump_boolean(next_indent, depth - 1, L, bvalue(value));				return;
		case LUA_TTABLE:		 ldv_dump_table(next_indent, depth - 1, L, hvalue(value));					return;
		case LUA_TLCL:			 ldv_dump_lua_closure(next_indent, depth - 1, L, clLvalue(value));			return;
		case LUA_TCCL:			 ldv_dump_c_closure(next_indent, depth - 1, L, clCvalue(value));			return;
		case LUA_TLCF:			 ldv_dump_c_light_func(next_indent, depth - 1, L, fvalue(value));			return;
		case LUA_TNUMFLT:		 ldv_dump_float_number(next_indent, depth - 1, L, fltvalue(value));			return;
		case LUA_TNUMINT:		 ldv_dump_int_number(next_indent, depth - 1, L, ivalue(value));				return;
		case LUA_TTHREAD:		 ldv_dump_thread(next_indent, depth - 1, L, thvalue(value));				return;
		case LUA_TUSERDATA:		 ldv_dump_user_data(next_indent, depth - 1, L, getudatamem(uvalue(value))); return;
		case LUA_TLIGHTUSERDATA: ldv_dump_light_user_data(next_indent, depth - 1, L, pvalue(value));		return;
		case LUA_TSHRSTR:		 ldv_dump_short_string(next_indent, depth - 1, L, tsvalue(value));			return;
		case LUA_TLNGSTR:		 ldv_dump_long_string(next_indent, depth - 1, L, tsvalue(value));			return;
		default:
			ldv_log(next_indent, "(ldv_dump_value func). Not recognized type: %s \n", ttypename(ttnov(value)));
			return;
	}
}

void (ldv_dump_nil)(const int indent, const int depth, lua_State* L, const TValue* nil_object)
{
	LDV_UNUSED(L) LDV_UNUSED(nil_object)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: NIL OBJECT \n");
}

void ldv_dump_boolean(const int indent, const int depth, lua_State* L, const int bool_val)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: BOOLEAN \n");
	ldv_log(indent, "Value: %i \n", bool_val);
}

void ldv_dump_table(const int indent, const int depth, lua_State* L, const Table* table)
{
	if (depth == 0)
		return;
	const int nsize = sizenode(table);
	const int next_indent = indent + INDENT_SIZE;
	ldv_log(indent, "Type: TABLE size(%i) \n", nsize + table->sizearray);
	ldv_log(indent, "===========  TABLE CONTENTS ===========\n");
	for (unsigned int i = 0; i < table->sizearray; ++i) 
	{
		ldv_log(next_indent, "Array key %i \n", i);
		ldv_dump_value(next_indent, depth - 1, L, &table->array[i]);
    }
	for (int i = 0; i < nsize; ++i)
	{
		Node* node = &table->node[i];
		ldv_log(next_indent, "Key of node: \n");
		ldv_dump_value(next_indent, depth - 1, L, gkey(node));
		ldv_log(next_indent, "Value of node: \n");
		ldv_dump_value(next_indent, depth - 1, L, gval(node));
	}
	ldv_log(indent, "=======================================\n");
}


void (ldv_dump_lua_closure)(const int indent, const int depth, lua_State* L, const LClosure* lclosure)
{
	LDV_UNUSED(L) LDV_UNUSED(lclosure)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: LUA CLOSURE, MARKED: %i \n", lclosure->marked);
	for (unsigned int i = 0; i < lclosure->nupvalues; ++i)
	{
		ldv_dump_upvalue(indent, depth - 1, L, lclosure->upvals[i]);
	}
	ldv_dump_proto(indent, depth - 1, L, lclosure->p);
}

void ldv_dump_proto(const int indent, const int depth, lua_State* L, const Proto* proto)
{
	if (depth == 0)
		return;
	ldv_log(indent, "Type: PROTO, UPSIZE %i, CDSIZE %i, CONSTSIZE %i", proto->sizeupvalues, proto->sizecode, proto->sizek);
	ldv_log(0, "Upvalues description: \n");
	for (int i = 0; i < proto->sizeupvalues; ++i)
	{
		ldv_log(0, "STACK INDEX %i, INSTACK %i\n NAME (DEBUG PURPOSES)\n", proto->upvalues[i].idx, proto->upvalues[i].instack);
		ldv_dump_str(indent, depth - 1, L, proto->upvalues[i].name);
	}
}

void ldv_dump_c_closure(const int indent, const int depth, lua_State* L, const CClosure* cclosure)
{
	LDV_UNUSED(L) LDV_UNUSED(cclosure)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: C CLOSURE with func: %p\n", cclosure->f);
}

void (ldv_dump_c_light_func)(const int indent, const int depth, lua_State* L, const lua_CFunction light_func)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: LIGHT C FUNCTION (addr %p)\n", light_func);
}

void (ldv_dump_thread)(const int indent, const int depth, lua_State* L, lua_State* lua_thread)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: LUA THREAD \n");
	ldv_log(indent, "Stack address %p, Top element %p Call Infos num %i", lua_thread->stack, lua_thread->top, lua_thread->nci);

}

void (ldv_dump_user_data)(const int indent, const int depth, lua_State* L, const char* user_data)
{
	LDV_UNUSED(L)
	ldv_log(indent, "Type: USER DATA \n");
	ldv_log(indent, "Value: %p \n", user_data);
}

void (ldv_dump_light_user_data)(const int indent, const int depth, lua_State* L, const void* light_user_data)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: LIGHT USER DATA \n");
	ldv_log(indent, "Value: %p \n", light_user_data);
}

void (ldv_dump_int_number)(const int indent, const int depth, lua_State* L, const lua_Integer int_num)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: INTEGER NUMBER \n");
	ldv_log(indent, "Value: %i \n", int_num);
}

void (ldv_dump_float_number)(const int indent, const int depth, lua_State* L, const lua_Number float_num)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: FLOAT NUMBER \n");
	ldv_log(indent, "Value: %f \n", float_num);
}

void (ldv_dump_str)(const int indent, const int depth, lua_State* L, const TString* str)
{
	if (depth == 0)
		return;
	/*	What should be located here???*/
	ldv_dump_short_string(indent, depth, L, str);
}

void (ldv_dump_short_string)(const int indent, const int depth, lua_State* L, const TString* string)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: SHORT STRING (%i) \n", tsslen(string));
	ldv_log(indent, "Value: %s \n", getstr(string));
}

void (ldv_dump_long_string)(const int indent, const int depth, lua_State* L, const TString* string)
{
	LDV_UNUSED(L)
	if (depth == 0)
		return;
	ldv_log(indent, "Type: LONG STRING (%i) \n", tsslen(string));
	ldv_log(indent, "Value: %s \n", getstr(string));
}
