/*-----------------------------------------------------------
**
** 版权:    中国航空无线电电子研究所, 2016年
**
** 文件名:  DiskIO.c
**
** 描述:    定义了DDBM文件系统IO操作函数
**
**
** 设计注记:
**
** 作者:
**     章诗晨, 2018年10月开始编写本文件
**
** 更改历史:
**
**-----------------------------------------------------------
*/

#ifndef __DISKIO_H__
#define __DISKIO_H__

#include <stdio.h>
#include "Ddbm_Common.h"
  
#define DISKIO_OK     0
#define DISKIO_ERROR  -1

/*磁盘基本配置区*/
#define BOOT_START_SECTOR       32
#define SECTOR_SIZE  			512
#define DBI_MAX_NUM 			256          					    /*最大的DBI数量*/
#define DBI_SIZE 				(DBI_MAX_NUM*SECTOR_SIZE)    	    /*DBI区域的大小*/
#define DBI_START_SECTOR		128			  					    /*DBI从第128个扇区开始，也就是第2个64k*/
#define ONE_BBM_SECTOR_NUM		1			  					    /*每个BBM占用的扇区大小*/
#define DBU_SIZE	 			(64*1024)       					/*64KB*/ 
#define ONE_DBU_SECTOR_NUM	    (64*2)          					/*每个DBU占用的扇区大小*/

/*磁盘基本配置区*/
typedef struct _BLK_DEV_INFO_
{
    uint32_ddbm disk_size;  			/*虚拟块设备可用存储空间大小*/
    uint32_ddbm disk_total_size;        /*虚拟块设备总大小*/
	uint32_ddbm boot_start_sector;      /*boot扇区起始数量*/
	uint32_ddbm sector_size;			/*扇区大小*/
	uint32_ddbm dbi_max_num;            /*DBI数量*/
    uint32_ddbm bbm_num;		        /*BBM的数量*/
	uint32_ddbm dbi_size;				/*DBI区域的大小*/
	uint32_ddbm dbi_start_sector;		/*DBI从第128个扇区开始，也就是第2个64k*/
	uint32_ddbm one_bbm_sector_num;		/*每个BBM占用的扇区大小*/
	uint32_ddbm dbu_size;				/*DBU大小64KB*/
	uint32_ddbm one_dbu_sector_num;		/*每个DBU占用的扇区大小*/
}BLK_DEV_INFO;

typedef int (*IO_FUNCPTR) (void *,uint64_ddbm,uint64_ddbm,unsigned char*);
typedef struct _MEM_BLK_DEV_        /*MEM_BLK_DEV*/
{

	uint8_ddbm  uc_BlkName[32];		    /*设备名称*/
	uint32_ddbm disk_valid;             /*当前设备有效性*/
	BLK_DEV_INFO dev_info; 				/*虚拟块设备基本信息*/
	//uint32_ddbm disk_size;              /*虚拟块设备大小*/
	/*uint8_ddbm *disk_base_addr;*/		/*虚拟块设备基地址*/
    FILE *vFileDev_fp;			        /*虚拟块设备文件句柄，最关键的参数*/
	IO_FUNCPTR	bd_blkRd;
	IO_FUNCPTR  bd_blkWrt;

}MEM_BLK_DEV;


extern int rd_sector(void *pDev, uint8_ddbm* buf, uint64_ddbm start, uint64_ddbm nSectors);
extern int wrt_sector(void *pDev, uint8_ddbm* buf, uint64_ddbm start, uint64_ddbm nSectors);
extern MEM_BLK_DEV* getMemBlkDev(const uint8_ddbm* devs_name);
extern MEM_BLK_DEV* Ddbm_Disk_Create(const uint8_ddbm* devs_name, FILE *vFileDev_fp);
extern uint32_ddbm Ddbm_Disk_Unload(const uint8_ddbm* devs_name);


void mem_ddbm_test();

#endif
