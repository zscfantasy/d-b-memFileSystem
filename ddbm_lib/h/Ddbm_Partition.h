#ifndef __DDBM_PARTITION_H__
#define __DDBM_PARTITION_H__

#include "Ddbm_Common.h"

/*最大支持的文件系统数量(可加载磁盘的数量)*/
#define FS_NUM  2

#pragma pack(1)

typedef struct _STRUCT_BOOTSECTOR_          /*引导扇区信息*/
{
	uint8_ddbm    uc_FsName[32];		    /*文件系统名称*/
	uint8_ddbm    uc_DiskSerial[32];        /*磁盘序列号*/
	uint8_ddbm    uc_DiskName[32];          /*磁盘名称*/
	uint32_ddbm   ui_BbmSum;			    /*BBM总数，2的倍数*/
	uint32_ddbm   ui_DbiSum;				/*Dbi总数*/
	uint32_ddbm   ui_DbuSize;				/*DBU大小*/
	uint64_ddbm   ull_DdiStartSector;		/*DDI起始扇区号*/
	uint64_ddbm   ull_DdiEndSector;			/*DDI结束扇区号*/
	uint64_ddbm   ull_DbiStartSector;		/*DBI起始扇区号*/
	uint64_ddbm   ull_DbiEndSector;			/*DBI结束扇区号*/
	uint64_ddbm   ull_BbmStartSector;		/*BBM起始扇区号*/
	uint64_ddbm   ull_BbmEndSector;			/*BBM结束扇区号*/
	uint64_ddbm   ull_DbuStartSector;		/*DBU起始扇区号*/
	uint64_ddbm   ull_DbuEndSector;			/*DBU结束扇区号*/
	uint64_ddbm   ull_Bbm2StartSector;		/*BBM起始扇区号*/
	uint64_ddbm   ull_Bbm2EndSector;		/*BBM结束扇区号*/
	uint64_ddbm   ull_Dbi2StartSector;		/*DBI起始扇区号*/
	uint64_ddbm   ull_Dbi2EndSector;		/*DBI结束扇区号*/
	uint64_ddbm   ull_Ddi2StartSector;		/*DDI起始扇区号*/
	uint64_ddbm   ull_Ddi2EndSector;		/*DDI结束扇区号*/
	uint32_ddbm   ui_BbmMethod;				/*BBM存储策略*/
	uint32_ddbm   ui_FsWrtPermission;		/*文件系统写许可*/
	uint8_ddbm    uc_Reserved[276];			/*保留字节*/
	uint32_ddbm   ui_EndFlag;				/*结束标记*/
	uint32_ddbm   ui_Crc;					/*crc校验*/
}STRUCT_BOOTSECTOR;

#pragma pack()

extern STRUCT_BOOTSECTOR gst_BootConfig[FS_NUM];

#endif
