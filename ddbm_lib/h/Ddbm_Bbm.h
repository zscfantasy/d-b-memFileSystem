/*-----------------------------------------------------------
**
** 版权:    中国航空无线电电子研究所 2016年
**
** 文件名:  Ddbm_Bbm.c
**
** 描述:    定义了DDBM文件系统底层为BBM驱动层
**
**
** 设计注记:
**
** 作者:
**     章诗晨 2018年10月开始编写本文件
**
** 更改历史:
**
**-----------------------------------------------------------
*/

#ifndef __DDBM_BBM_H__
#define __DDBM_BBM_H__

#include "Ddbm_Common.h"

#pragma pack(1)
/*BBM结构体*/
typedef struct _STRUCT_BBM_
{
	uint32_ddbm    ui_BbmIdx;                 /*BBM索引号*/
	uint32_ddbm    ui_DbiIdx;                 /*本BBM对应的DBI的索引号*/
	uint32_ddbm    ui_DataType;               /*数据类型*/
	uint64_ddbm    ull_DbuStartSector;        /*DBU起始扇区号*/
	uint64_ddbm    ull_DbuEndSector;		  /*DBU结束扇区号*/
	uint32_ddbm    ui_PreBbmIdx;			  /*前一Bbm索引号*/
	uint32_ddbm    ui_NextBbmIdx;			  /*后一Bbm索引号*/
	uint64_ddbm    ull_WrtPosition;			  /*写指针，相对扇区号*/
	uint64_ddbm    ull_SequenceNum;			  /*顺序流水号*/
	uint32_ddbm    ui_EncryptionId;			  /*加密算法ID*/
	uint8_ddbm     uc_EncryptionName[32];     /*加密算法名称*/
	uint32_ddbm    ui_UsedFlag;		          /*使用标记,0:被占用（在bbm队列中或者可以放到BBm队列中），1:未被占用（不在队列中，被dbi占用）*/
	uint8_ddbm     uc_Reserved[412];		  /*保留字节, 其他100个字节*/
	uint32_ddbm    ui_EndFlag;				  /*结束标记*/
	uint32_ddbm    ui_Crc;					  /*crc校验*/
}STRUCT_BBM;
#pragma pack()

extern int32_ddbm Ddbm_Get_BbmList_Tail(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx);
extern int32_ddbm Ddbm_Get_BbmChain_Size(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx);
extern int32_ddbm Ddbm_Append_BbmList(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx);
extern int32_ddbm Ddbm_Del_BbmList(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx);
extern int32_ddbm Ddbm_Spare_Bbm(uint32_ddbm fs_index);
extern int32_ddbm Ddbm_New_BbmList(uint32_ddbm fs_index);
extern int32_ddbm Ddbm_Bbm_Used(uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx);
extern void Ddbm_Write_BbmInfo(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx);
extern STRUCT_BBM * Ddbm_Read_BbmInfo(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx);
extern int32_ddbm Ddbm_Get_NextBbm(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx);
extern int32_ddbm Ddbm_Init_BbmPartiton(void *pDev, uint32_ddbm fs_index);
extern uint32_ddbm Ddbm_Crc_Check(uint8_ddbm *ptr, uint32_ddbm len);
extern int32_ddbm Ddbm_Get_One_Bbm(uint32_ddbm fs_index);

#endif
