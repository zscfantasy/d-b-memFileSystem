#ifndef _MemBlk_LIB_h
#define _MemBlk_LIB_h

/*---------------------------------
**         函数声明
**---------------------------------
*/
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
**
**.EH-------------------------------------------------------------
*/
extern int Ddbm_MemBlk_Create(unsigned int const BlkSum, unsigned int const BlkLen_Long);

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
**                          else, 申请到的动态内存单元首地址
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
extern void * Ddbm_MemBlk_Alloc(unsigned int const MemBlk_Id);

/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_MemBlk_Free
**
** 描述: 动态内存存储单元释放
**
** 输入参数:
**          MemBlk_Id  : unsigned int const, 动态内存序号
**          Blk_Address: void *, 动态内存单元首地址
**
** 输出参数:
**          返回值: int, 0~Blk_SUM-1, 释放的动态内存存储单元序号
**                       -1, 输入动态内存序号或动态内存存储单元地址无效
**
** 设计注记:
**
**.EH-------------------------------------------------------------
*/
extern int Ddbm_MemBlk_Free(unsigned int const MemBlk_Id, void * const Blk_Address);

#endif
