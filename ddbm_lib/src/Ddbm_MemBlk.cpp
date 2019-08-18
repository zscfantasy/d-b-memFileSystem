#include <stdio.h>
#include <stdlib.h>
#include "Ddbm_MemBlk.h"

/*MemBlk总数,每一个设备对应一个*/
#define MEMBLK_TOTAL_SUM   10

typedef int       MUTEX_TYPE;
#define MUTEX_CREATE(mutex)
#define MUTEX_TAKE(mutex)
#define MUTEX_RELEASE(mutex)

typedef struct
{
	unsigned int  BlkSum;      /*动态内存单元数量*/
	unsigned int  BlkLen_Byte; /*动态内存单元长度, 单位:32位字*/
	char          *Base;       /*动态内存基地址, Base[BlkSum][BlkLen_Byte]*/
	int           *Available;  /*动态内存可用单元存储队列地址, Available[BlkSum + 1]*/

	/*动态内存可用单元存储队列读/写指针, Available[pR_Ava]与Available[pW_Ava]之间存储Base[]中可用单元的下标*/
	unsigned int pR_Ava, pW_Ava;
	/*多任务申请和释放缓存的互斥量*/
	MUTEX_TYPE Mutex;
} MEMBLK_STRUCT_TYPE;

static MEMBLK_STRUCT_TYPE MemBlk_Data[MEMBLK_TOTAL_SUM];
static unsigned int MemBlk_Index = 0;

/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_MemBlk_Create
**
** 描述: 动态内存创建和初始化接口函数
**
** 输入参数:
**          BlkSum     : unsigned int const, 动态内存单元数量
**          BlkLen_Long: unsigned int const, 动态内存单元长度(单位:32位字)
**
** 输出参数:
**          返回值: int, >= 0, 创建成功, 返回动态内存序号
**                         -1, 创建失败(动态内存总数定义不足或无效参数)
**                         -2, 创建失败(未申请到MemBlk存储空间)
**
** 设计注记:
**         malloc申请到的MemBlk空间分布
**               char Base[BlkSum][BlkLen_Long<<2]: 动态内存存储单元空间
**               int  Available[BlkSum + 1]       : 动态内存可用单元存储队列空间
**.EH-------------------------------------------------------------
*/
int Ddbm_MemBlk_Create(unsigned int const BlkSum, unsigned int const BlkLen_Long)
{
	int return_value;
	char *Base;
	MEMBLK_STRUCT_TYPE *pMemBlk;
	unsigned int i;

	if ((MemBlk_Index >= MEMBLK_TOTAL_SUM) || (BlkSum <= 1) || (BlkLen_Long < 1))
		return -1;
	if ((Base = (char *)malloc(((BlkSum * BlkLen_Long) << 2) + ((BlkSum + 1) * sizeof(int)))) == NULL) /*布尔表达式中设计有赋值语句*/
		return -2;

	return_value = MemBlk_Index++;
	pMemBlk = &MemBlk_Data[return_value];
	pMemBlk->Base        = Base;
	pMemBlk->BlkSum      = BlkSum;
	pMemBlk->BlkLen_Byte = BlkLen_Long << 2;
	pMemBlk->Available   = (int*)(Base + ((BlkSum * BlkLen_Long) << 2));

	for (i = 0; i < BlkSum; i++)
		pMemBlk->Available[i] = i;
	pMemBlk->pR_Ava = 0;
	pMemBlk->pW_Ava = BlkSum;
	MUTEX_CREATE(pMemBlk->Mutex);

	return return_value;
}
/* END of MemBlk_Create */


/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_MemBlk_Alloc
**
** 描述: 动态内存存储单元申请
**
** 输入参数:
**          MemBlk_Id: unsigned int const, 动态内存序号
**
** 输出参数:
**          返回值: void *, NULL, 输入动态内存序号无效或动态内存无存储单元可供申请
**                          else, 申请到的动态内存存储单元首地址
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
void * Ddbm_MemBlk_Alloc(unsigned int const MemBlk_Id)
{
	void * return_value = NULL;
	MEMBLK_STRUCT_TYPE *pMemBlk;

	if (MemBlk_Id < MemBlk_Index)
	{
		pMemBlk = &MemBlk_Data[MemBlk_Id];
		MUTEX_TAKE(pMemBlk->Mutex);
		if (pMemBlk->pR_Ava != pMemBlk->pW_Ava)
		{
			return_value = &pMemBlk->Base[pMemBlk->Available[pMemBlk->pR_Ava] * pMemBlk->BlkLen_Byte];
			pMemBlk->pR_Ava = (pMemBlk->pR_Ava + 1) % (pMemBlk->BlkSum + 1);
		}
		MUTEX_RELEASE(pMemBlk->Mutex);
	}
	return return_value;
}
/* END of MemBlk_Alloc */


/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_MemBlk_Free
**
** 描述: 动态内存存储单元释放
**
** 输入参数:
**          MemBlk_Id  : unsigned int const, 动态内存序号
**          Blk_Address: void * const, 动态内存单元首地址
**
** 输出参数:
**          返回值: int, 0~Blk_SUM-1, 释放的动态内存存储单元序号
**                       -1, 输入动态内存序号或动态内存存储单元地址无效
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
int Ddbm_MemBlk_Free(unsigned int const MemBlk_Id, void * const Blk_Address)
{
	MEMBLK_STRUCT_TYPE *pMemBlk;
	int MemBlk_Serial, pW_Plus, return_value = -1;

	/*MemBlk_Id是当前设备所指向的内存分配区的编号，如果MemBlk_Index是2，MemBlk_Id只能是1*/
	if (MemBlk_Id < MemBlk_Index)
	{
		pMemBlk = &MemBlk_Data[MemBlk_Id];
		MemBlk_Serial = (int)((char*)Blk_Address - pMemBlk->Base) / pMemBlk->BlkLen_Byte;
		if ((0 <= MemBlk_Serial) && ((unsigned int)MemBlk_Serial < pMemBlk->BlkSum))
		{
			MUTEX_TAKE(pMemBlk->Mutex);
			pW_Plus = (pMemBlk->pW_Ava + 1) % (pMemBlk->BlkSum + 1);
			if (pW_Plus != pMemBlk->pR_Ava)
			{
				pMemBlk->Available[pMemBlk->pW_Ava] = MemBlk_Serial;
				pMemBlk->pW_Ava = pW_Plus;
				return_value = MemBlk_Serial;
			}
			MUTEX_RELEASE(pMemBlk->Mutex);
		}
	}
	return return_value;
}
/* END of MemBlk_Free */

#undef MUTEX_TYPE
#undef MUTEX_CREATE
#undef MUTEX_TAKE
#undef MUTEX_RELEASE
#undef MEMBLK_TOTAL_SUM
