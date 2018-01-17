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
#define NEXT_BLOCK 1
#define PREV_BLOCK 0

#define DATA_FLAG 0
#define NEXT_FLAG 1
#define PREV_FLAG 2

//		Output char buffer
static char out_buff[1000];
//		Index of first free block
static int  block_index = 0;
//		Memory buffer
static ldv_block_type mem_buf[MEM_BUFF_SIZE];

/*
        Helper structure to mark blocks in memory buffer. It is used by memory manager
*/
struct BlocksInfo
{
    /*  Index (offset) to next block    */
    ldv_block_type next_index;
    /*  Index (offset) to previous block    */
    ldv_block_type prev_index;
};

/*
        Sets new index with flag to blocks info
        Params: blocks info, next index flag, index, val flag
        Return: none
*/
void set_index(BlocksInfo* binfo, int is_next, ldv_block_type index, int flag)
{
    ldv_block_type* index_ptr = is_next ? &binfo->next_index : &binfo->prev_index;
    *index_ptr = flag ? block_flag_mask() | index : index;
}

/*
        Checks, whether blocks info is empty
        Params: blocks info
        Return: check result
*/
int is_empty(BlocksInfo* binfo)
{
    return binfo->next_index & block_flag_mask();
}

/*
        Checks, whether specified flag is set
        Params: block info, flag
        Return: check result
*/
int check(BlocksInfo* binfo, int flag)
{
    /*  TODO: (alex) add usage of enums */
    int mask = block_flag_mask();
    if (flag == DATA_FLAG)
        return binfo->prev_index & mask;
    if (flag == NEXT_FLAG)
        return binfo->next_index & mask;
    return binfo->prev_index & ~mask == 0;
}

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
		Gets blocks info at raw pointer
		Params: raw pointer
		Return: blocks info
*/
static BlocksInfo* blocks_info(void* ptr)
{
	return (BlocksInfo*)(ptr) - 2;
}

/*
		Checks, whether block is valid
		Params: block
		Return: check result
*/
static int BlocksInfo(BlocksInfo* block)
{
	return mem_buf <= (ldv_block_type*)block && (ldv_block_type*)block < mem_buf + MEM_BUFF_SIZE;
}

/*
		Next blocks count
		Params: block
		Return: next blocks count
*/
static ldv_block_type nblock(ldv_block_type* block)
{
	return block[0] & block_data_mask();
}

/*
		Prev blocks count
		Params: block
		Return: prev blocks count
*/
static ldv_block_type pblock(ldv_block_type* block)
{
	return block[1] & block_data_mask();
}

/*
		Gets previous block info
		Params: block info
		Return: prev block info
*/
static BlocksInfo* prev_block_info(BlocksInfo* block)
{
	BlocksInfo* prev_block = (BlocksInfo*)((ldv_block_type*)block - (block[1] & block_data_mask()));
	return block_is_valid((BlocksInfo*)prev_block) ? prev_block : 0;
}


/*
		Gets next block info
		Params: block info
		Return: next block info
*/
static ldv_block_type* next_block_info(ldv_block_type* block)
{
	ldv_block_type* next_block = block + (block[0] & block_data_mask());
	return block_is_valid(next_block) ? next_block : 0;
}

/*
		Frees memory allocated via ldv_malloc
		Params: pointer to memory
		Return: none
*/
static void ldv_free(void* ptr)
{
	BlocksInfo* b_info = blocks_info(ptr);
	ldv_block_type* n_block = next_block_info(block);
	if (block_is_valid(n_block) && check(next_block, DATA_FLAG))
	{
        set_index(b_info,  NEXT_BLOCK, nblock(b_info) + nblock(next_block), DATA);
        ldv_block_type* nnbinfo = next_block_info(n_block);
        if (block_is_valid(nnbinfo))
        {
            set_index(nnbinfo, PREV_BLOCK, pblock(nnbinfo) - pblock(n_block, DATA));
        }
	}
	/*  Implement handling left "merging"   */
}

/*
		Frees memory allocated via ldv_malloc
		Params: pointer to memory
		Return: none
*/
static void* ldv_malloc(void* ptr, size_t nsize)
{

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
		//	Deletion of block
		return 0;
	}
	if (osize != 0)
	{
		//	Release memory
	}
	//	Creation new memory
	return 0 /*new created ptr*/;
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
