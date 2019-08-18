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

#include <stdlib.h>
#include <string.h>
#include "DiskIO.h"
#include "Ddbm_Common.h"
#include "Ddbm_Corelib.h"
#include "Ddbm_Partition.h"
#include "Ddbm_Bbm.h"

static MEM_BLK_DEV memBlkDev[2];

static int mem_blkRd(MEM_BLK_DEV* pDev,uint64_ddbm start,uint64_ddbm nSectors,uint8_ddbm* buf)
{

	/*
	for(int index = 0;index<nSectors;index++)
	{
		memcpy(buf + index*SECTOR_SIZE, pDev->disk_base_addr + start*SECTOR_SIZE + index*SECTOR_SIZE, SECTOR_SIZE);
	}
	*/
	
	if(pDev->vFileDev_fp)
	{
		fseek(pDev->vFileDev_fp, start*SECTOR_SIZE, SEEK_SET);
		fread((void*)buf, SECTOR_SIZE, nSectors, pDev->vFileDev_fp);
		return DISKIO_OK;
	}
	else
		return DISKIO_ERROR;
	
}

static int mem_blkWrt(MEM_BLK_DEV* pDev,uint64_ddbm start,uint64_ddbm nSectors,uint8_ddbm* buf)
{
	/*
	for(int index = 0;index<nSectors;index++)
	{
		memcpy(pDev->disk_base_addr + start*SECTOR_SIZE + index*SECTOR_SIZE, buf + index*SECTOR_SIZE, SECTOR_SIZE);
	}
	*/
	if(pDev->vFileDev_fp)
	{
		fseek(pDev->vFileDev_fp, start*SECTOR_SIZE, SEEK_SET);
		fwrite((void*)buf, SECTOR_SIZE, nSectors, pDev->vFileDev_fp);
		return DISKIO_OK;
	}
	else
		return DISKIO_ERROR;
	
}

/************************************************
**函数名称：rd_sector
**
**功能：读扇区数据
**
**输入参数：pDev：虚拟文件设备句柄 buf：读内容存放指针；start：虚拟块设备起始扇区；nSectors：扇区个数；
**
**输出参数：成功 >=0，失败0
*/
int rd_sector(void *pDev, uint8_ddbm* buf, uint64_ddbm start, uint64_ddbm nSectors)
{

	int stats;

	if(pDev == NULL)
	{
		return DISKIO_ERROR;
	}

	stats = ((MEM_BLK_DEV*)pDev)->bd_blkRd((MEM_BLK_DEV*)pDev,start,nSectors,buf);

	
	if(stats == DISKIO_ERROR || buf == NULL)
	{
		return DISKIO_ERROR;
	}

	return DISKIO_OK;

}

/************************************************
**函数名称：wrt_sector
**
**功能：写扇区数据
**
**输入参数：pDev：虚拟文件设备句柄 buf：读内容存放指针；start：虚拟块设备起始扇区；nSectors：扇区个数；
**
**输出参数：成功 >=0，失败0
*/
int wrt_sector(void *pDev, uint8_ddbm* buf, uint64_ddbm start, uint64_ddbm nSectors)
{
	int stats;
	if(buf == NULL)
	{
		return DISKIO_ERROR;
	}
	if(pDev == NULL)
	{
		return DISKIO_ERROR;
	}
	
	stats = ((MEM_BLK_DEV*)pDev)->bd_blkWrt((MEM_BLK_DEV*)pDev,start,nSectors,buf);

	if(stats == DISKIO_ERROR)
	{
		return DISKIO_ERROR;
	}

	return DISKIO_OK;
} 


/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_Disk_Create
**
** 描述: 创建虚拟块设备
**
** 输入参数:
**          devs_name： 块设备的名称(完整的路径)，dev_info: 块设备基本信息， vFileDev_fp:虚拟文件句柄
**          
**
** 输出参数:
**          返回值:  创建成功, 返回模拟磁盘基地址
**                        -1, 创建失败
** 
** 备注：
**          通过内存模拟虚拟磁盘的函数,申请空间
**.EH-------------------------------------------------------------
*/
extern MEM_BLK_DEV* Ddbm_Disk_Create(const uint8_ddbm* devs_name ,FILE *vFileDev_fp)
{
    int re;
	uint32_ddbm index;
	MEM_BLK_DEV* pBlkDev = NULL;
    BLK_DEV_INFO dev_info;

	/*如果有重名设备，则直接返回*/
	for(index = 0;index < 2;index++)
	{
		if(strcmp((const char*)(memBlkDev[index].uc_BlkName),(const char*)devs_name)==0)
			return NULL;
	}

    /*遍历文件系统可用的的设备槽(memBlkDev)，有的话就把设备往里面填(index控制)*/
	for(index = 0;index < 2;index++)
	{
		if(memBlkDev[index].uc_BlkName[0] == '\0' &&  memBlkDev[index].disk_valid == 0)
			break;
	}

	pBlkDev = &memBlkDev[index];
    //进行了这一步操作之后，初始化文件系统才能成功，表示找到了这个名字可以初始化
	strcpy((char*)pBlkDev->uc_BlkName,(const int8_ddbm*)devs_name);
	pBlkDev->disk_valid = 1;
	pBlkDev->bd_blkRd = (IO_FUNCPTR)mem_blkRd;
	pBlkDev->bd_blkWrt = (IO_FUNCPTR)mem_blkWrt;
    pBlkDev->vFileDev_fp = vFileDev_fp;

    //每次插入设备之前的初始化文件系统操作，这样gst_BootConfig[index]等全局变量才是有效的
    re = Ddbm_Init_FileSystem((const char*)devs_name);
    if(re == -1)
    {
        printf("init re =%d falied when create dev\n",re);
        return NULL;
    }


    //以下信息之后初始化文件系统之后才能获取
    dev_info.disk_size = (gst_BootConfig[index].ui_DbuSize)*(gst_BootConfig[index].ui_BbmSum);
    dev_info.boot_start_sector = BOOT_START_SECTOR;      /*boot扇区起始数,定死32*/
    dev_info.sector_size = SECTOR_SIZE;			/*扇区大小*/
    dev_info.dbi_max_num = gst_BootConfig[index].ui_DbiSum;            /*DBI数量*/
    dev_info.dbi_size = (gst_BootConfig[index].ui_DbiSum)*SECTOR_SIZE;				/*DBI区域的大小*/
    dev_info.dbi_start_sector = gst_BootConfig[index].ull_DbiStartSector;		/*DBI从第128个扇区开始，也就是第2个64k*/
    dev_info.one_bbm_sector_num = ONE_BBM_SECTOR_NUM;		/*每个BBM占用的扇区个数*/
    dev_info.dbu_size = gst_BootConfig[index].ui_DbuSize;				/*DBU大小64KB*/
    dev_info.one_dbu_sector_num = (gst_BootConfig[index].ui_DbuSize)/SECTOR_SIZE;		/*每个DBU占用的扇区个数*/
    dev_info.bbm_num = gst_BootConfig[index].ui_BbmSum;   /*BBm总数*/

    memcpy(&(pBlkDev->dev_info), &dev_info, sizeof(dev_info));

    //printf("dev_info.bbm_num = %d\n",dev_info.bbm_num);
    //printf("pBlkDev->dev_info.bbm_num = %d\n",pBlkDev->dev_info.bbm_num);

	return pBlkDev;
	/*
	pBlkDev->disk_size = BlkSum*1024*SECTOR_SIZE*2;
	pBlkDev->disk_base_addr = (uint8_ddbm *)malloc(pBlkDev->disk_size);
	if(!pBlkDev->disk_base_addr ) return NULL;
	else
	{
		memset((void *)pBlkDev->disk_base_addr,0x00,pBlkDev->disk_size);
		return pBlkDev;
	}
	*/

}

/*.BH-------------------------------------------------------------
**
** 函数名: getMemBlkDev
**
** 描述: 获取虚拟块设备
**
** 输入参数:
**          devs_name： 块设备的名称
**
** 输出参数:
**          返回值:        获取成功, 返回模拟磁盘基地址
**                        NULL, 获取失败
** 
**.EH-------------------------------------------------------------
*/
extern MEM_BLK_DEV* getMemBlkDev(const uint8_ddbm* devs_name)
{
	uint32_ddbm index;
	if(devs_name == NULL)  return NULL;
	for(index = 0;index < 2;index++)
	{
		if((strcmp((const char*)memBlkDev[index].uc_BlkName,(const char*)devs_name)==0) &&  memBlkDev[index].disk_valid == 1)
			break;
	}
	if(index<2)
		return &memBlkDev[index];
	else
		return NULL;

}

/*.BH-------------------------------------------------------------
**
** 函数名: Ddbm_Disk_Unload
**
** 描述: 卸载虚拟块设备
**
** 输入参数:
**          devs_name： 块设备的名称
**
** 输出参数:
**          返回值: int, >= 0, 卸载成功
**                        -1, 卸载失败
** 
** 备注：
**          通过内存模拟虚拟磁盘的函数,申请空间
**.EH-------------------------------------------------------------
*/
extern uint32_ddbm Ddbm_Disk_Unload(const uint8_ddbm* devs_name)
{
	uint32_ddbm index;
	MEM_BLK_DEV* pBlkDev = NULL;

	if(devs_name == NULL)  return -1;

	for(index = 0;index < 2;index++)
	{
		if((strcmp((const char*)memBlkDev[index].uc_BlkName,(const char*)devs_name)==0) &&  memBlkDev[index].disk_valid == 1)
			break;
	}
	if(index >=2)  return -1;

	pBlkDev = &memBlkDev[index];

	memset((char*)pBlkDev->uc_BlkName,0x00,32);
	pBlkDev->uc_BlkName[0] = '\0';
	pBlkDev->disk_valid = 0;
	pBlkDev->bd_blkRd = NULL;
	pBlkDev->bd_blkWrt = NULL;

	pBlkDev->dev_info.disk_size = 0;
	/*
	if(pBlkDev->disk_base_addr)
		free(pBlkDev->disk_base_addr);
	*/
	if( 0 == fclose(pBlkDev->vFileDev_fp))
		return 0;   //关闭成功
	else
		return -1;  //关闭失败


}
