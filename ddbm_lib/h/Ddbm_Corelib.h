
#ifndef __DDBM_CORELIB_H__
#define __DDBM_CORELIB_H__

#include "Ddbm_Common.h"
#include "DiskIO.h"

/*错误代码*/
#define LOAD_FAIL      -1
#define UNLOAD_FAIL    -2
#define DISK_FULL      -3
#define NAME_ERR       -4
#define REOPEN_ERR     -5
#define CRC_ERR        -6
#define DEV_NOTFOUND   -7
#define DEL_ERR        -8
#define MALLOC_ERR     -9
#define DBI_RECYCLE    -10
#define TIME_ERR       -11
#define NEED_FORMAT    -12


#define FILE_CLOSED 0
/*文件打开模式*/
#define FILE_RDONLY 1
#define FILE_RDWRT  2
#define FILE_APPEND 4
#define FILE_UPDATE 8

#define READ_PERMIT_MODE    0x01
#define WRITE_PERMIT_MODE   0x02


/*文件指针信息*/
typedef struct _STRUCT_FILE_
{
	int8_ddbm     c_FileName[32];
	uint64_ddbm   ull_FileSize;				/*文件大小*/
	uint32_ddbm   ui_FsNum;                 /*文件系统编号*/
	uint32_ddbm   ui_Mode;					/*文件模式，只读或可读写*/
	uint32_ddbm   ui_BbmIdx;				/*当前读或写的BBM索引号*/
	uint32_ddbm   ui_DbiIdx;				/*文件对应的DBI索引号*/   
	uint64_ddbm   ull_Position;			    /*当前文件在当前BBM的读写指针，相对BBM对应的起始扇区号*/
	SEM_ID_DDBM   sem_Pending;              /*添加信号量，保护文件更改*/
	uint8_ddbm    uc_SectorCache[512];		/*一个扇区的缓冲，用于文件的连续读写*/

    //uint32_ddbm   ui_StartBbmIdx;				/*当前文件BBM链的首个BBM索引号*/
    //uint32_ddbm   ui_StartBbmIdx;				/*当前文件BBM链的最后一个BBM索引号*/
}STRUCT_FILE;


/*文件事件信息*/
typedef struct _STRUCT_TIME_
{
	uint16_ddbm  year;      /*年*/
	uint8_ddbm   month;		/*月*/
	uint8_ddbm   day;       /*日*/
	uint8_ddbm   hour;		/*时*/
	uint8_ddbm   minute;	/*分*/
	uint8_ddbm   second;	/*秒*/   

}STRUCT_TIME;

/*文件基本属性信息*/
typedef struct _STRUCT_ATTR_
{
    uint32_ddbm   ui_DbiIdx;				/*DBI索引号*/
    int8_ddbm     uc_FileName[256];			/*文件名称*/
    uint32_ddbm   ui_FirstBbmIdx;			/*分配的起始BBM编号*/
    uint32_ddbm   ui_BbmChainSize;			/*分配的BBM链大小*/
    uint64_ddbm   ui_CreateTime;			/*创建时间*/
    uint64_ddbm   ui_ModifyTime;			/*最近一次修改时间*/
    uint64_ddbm   ui_CloseTime;				/*最近一次关闭时间*/
    uint64_ddbm   ull_FileSize;				/*文件大小*/
}STRUCT_ATTR;



extern int32_ddbm Ddbm_Init_FileSystem(const int8_ddbm * pc_devsName);
extern void *Ddbm_Open_File(const int8_ddbm *c_DevsName, const int8_ddbm *c_Name, int32_ddbm i_Flag);
extern int32_ddbm Ddbm_Read_File(void * buf, const uint32_ddbm size, const uint32_ddbm count, STRUCT_FILE * const fp);
extern int32_ddbm Ddbm_Write_File(const void *buf, const uint32_ddbm size, const uint32_ddbm count, STRUCT_FILE * const fp);
extern int32_ddbm Ddbm_Delete_File(uint32_ddbm fs_index, const int8_ddbm *c_Name);
extern int32_ddbm Ddbm_Close_File(STRUCT_FILE *pFd);
extern int32_ddbm Ddbm_Query_File(uint32_ddbm fs_index, const int8_ddbm *c_Name);
extern int32_ddbm Ddbm_Seek_File(STRUCT_FILE *pFd, uint32_ddbm offset);
extern int32_ddbm Ddbm_Rename_File(const char *oldname, const char *newname);
extern int32_ddbm Ddbm_Error_Get();
extern uint64_ddbm Ddbm_Get_Time();
extern int32_ddbm Ddbm_Update_Time(uint16_ddbm year,uint8_ddbm mounth,uint8_ddbm day,uint8_ddbm hour,uint8_ddbm minute,uint8_ddbm second);
extern int32_ddbm Ddbm_File_BbmInfo(const int8_ddbm * pc_devsName, const int8_ddbm *c_Name, uint32_ddbm **bbmChain);
extern int32_ddbm Ddbm_GetFileSystem_Index(const int8_ddbm * pc_devsName);
extern uint32_ddbm Ddbm_TraverseFiles(const int8_ddbm *pc_devsName, uint32_ddbm **dbiChain);
extern STRUCT_ATTR *Ddbm_getFileAttr(const int8_ddbm *pc_devsName, int32_ddbm dbiIdx);
extern int32_ddbm Ddbm_Format(const uint8_ddbm* disk_name,BLK_DEV_INFO dev_info);

#endif
