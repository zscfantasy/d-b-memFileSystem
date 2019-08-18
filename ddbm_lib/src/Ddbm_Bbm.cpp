/*-----------------------------------------------------------
**
** 版权:    中国航空无线电电子研究所, 2016年
**
** 文件名:  Ddbm_Bbm.c
**
** 描述:    定义了DDBM文件系统为BBM驱动层
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Ddbm_Bbm.h"
#include "Ddbm_Partition.h"
#include "DiskIO.h"

#define BBM_SEM_CREATE(MUTEX)   
#define BBM_SEM_TAKE(MUTEX)
#define BBM_SEM_GAVE(MUTEX)
#define BBM_SEM_DELETE(MUTEX)
/*BBM先入先出队列*/
typedef struct _STRUCT_BBM_QUEUE_
{
    uint32_ddbm *pui_BbmQueue;      /*BBM节点具体信息*/
	uint32_ddbm ui_GetNodeIdx;    	/*BBM读下标,未读*/
    uint32_ddbm ui_PutNodeIdx;    	/*BBM写下标，未写*/
    uint32_ddbm ui_Count;          	/*BBM队列中BBM节点个数*/
}STRUCT_BBM_QUEUE;

/*全局静态变量*/
typedef struct {
	/*BBM空间*/
	STRUCT_BBM *gst_BbmInfo;  
	/*BBM队列*/
	STRUCT_BBM_QUEUE st_BbmQueue;
	/*初始化标志*/
	int32_ddbm gst_BbmInitFlag;
	/*需定义信号量变量*/
	int32_ddbm sem_BbmQueue;
	#define  FS_BBMINIT   0x01
}STRUCT_FS_BBM;

static STRUCT_FS_BBM st_FsBbm[FS_NUM] = {{NULL, {}, 0x00, 0x00},{NULL, {}, 0x00, 0x00}};

static uint32_ddbm ui_CrcTbl[256] = {/* CRC余式表 */   
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,   
  0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,   
  0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,   
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,   
  0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,   
  0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,   
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,    
  0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,   
  0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,   
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,   
  0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,   
  0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,   
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,   
  0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,   
  0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,   
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,   
  0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,   
  0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,   
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,   
  0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,   
  0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,   
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,   
  0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,   
  0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,   
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,   
  0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,   
  0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,   
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,   
  0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,   
  0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,   
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,   
  0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d   
 };

/************************************************
**函数名称：Ddbm_UnMount_BbmPartiton
**
**功能：BBM资源释放
**
**输入参数：fs_index :文件系统编号
**
**输出参数：无
**
**注释：
**
*/
static void Ddbm_UnMount_BbmPartiton(uint32_ddbm fs_index)
{
	/*清空队列*/
	free(st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue);
	/*清空BBM区结构信息*/
	free(st_FsBbm[fs_index].gst_BbmInfo);
}

/************************************************
**函数名称：Ddbm_BbmQueue_Init
**
**功能：BBM队列初始化
**
**输入参数：fs_index :文件系统编号
**
**输出参数：成功 0，失败-1
**
**注释：
**
*/
static int32_ddbm Ddbm_BbmQueue_Init(uint32_ddbm fs_index)
{
	st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx = 0;
	st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx = 0;
	st_FsBbm[fs_index].st_BbmQueue.ui_Count = 0;
	if(!st_FsBbm[fs_index].sem_BbmQueue)
    {
        BBM_SEM_CREATE(st_FsBbm[fs_index].sem_BbmQueue);
    }

	st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue = (uint32_ddbm*)malloc(gst_BootConfig[fs_index].ui_BbmSum * sizeof(gst_BootConfig[fs_index].ui_BbmSum));

	if(st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue == NULL)
    {
        return -1;
    }
	return 0;
}

/************************************************
**函数名称：Ddbm_BbmQueue_Send
**
**功能：将BBM放到队列中
**
**输入参数：fs_index :文件系统编号
          ui_BbmIdx：BBM编号
**
**输出参数：成功 0，失败-1
**
**注释：
**
*/
static int32_ddbm Ddbm_BbmQueue_Send(uint32_ddbm fs_index, uint32_ddbm ui_BbmIdx) 
{
	/*
	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -1;
	*/
	
	if(ui_BbmIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;
	/*队列满*/
	if((st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx == st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx) && (st_FsBbm[fs_index].st_BbmQueue.ui_Count == gst_BootConfig[fs_index].ui_BbmSum))
	{
		return -2;
	}

	BBM_SEM_TAKE(st_FsBbm[fs_index].sem_BbmQueue);
	st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue[st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx] = ui_BbmIdx;
	st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx = (st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx + 1) % gst_BootConfig[fs_index].ui_BbmSum;
	st_FsBbm[fs_index].st_BbmQueue.ui_Count += 1;
	BBM_SEM_GAVE(st_FsBbm[fs_index].sem_BbmQueue);
	return 0;
}

/************************************************
**函数名称：Ddbm_BbmQueue_Receive
**
**功能：从队列中取BBM编号
**
**输入参数：fs_index :文件系统编号
**
**输出参数：BBM编号: >=0，失败:-1
**
**注释：
**
*/
static int32_ddbm Ddbm_BbmQueue_Receive(uint32_ddbm fs_index)
{
	uint32_ddbm index;
	/*
	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -1;
	*/
	/*队列空*/
	if((st_FsBbm[fs_index].st_BbmQueue.ui_PutNodeIdx == st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx) && (st_FsBbm[fs_index].st_BbmQueue.ui_Count == 0))
	{
		return -2;
	}
	//printf("st_BbmQueue.ui_GetNodeIdx = %d\t",st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx);
	BBM_SEM_TAKE(st_FsBbm[fs_index].sem_BbmQueue);
	index = st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue[st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx]; 
	st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx = (st_FsBbm[fs_index].st_BbmQueue.ui_GetNodeIdx + 1) % gst_BootConfig[fs_index].ui_BbmSum;
	st_FsBbm[fs_index].st_BbmQueue.ui_Count -= 1;
	BBM_SEM_GAVE(st_FsBbm[fs_index].sem_BbmQueue);
	//printf("GetNodeIdx's index = %d\n",index);
	return index;
}

/************************************************
**函数名称：Ddbm_Get_BbmList_Tail
**
**功能：获得BBM链表的尾BBM节点编号
**
**输入参数：fs_index :文件系统编号
          ui_BbmHeadIdx :文件链鞡BM头节点
**
**输出参数：BBM编号: >=0，失败:-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Get_BbmList_Tail(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx)
{
	uint32_ddbm ui_BbmIdx;
	

	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -1;
	if(ui_BbmHeadIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;

	ui_BbmIdx = ui_BbmHeadIdx;
	while(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx != 0xFFFFFFFF)
	{
		/*检查下一个BBM节点是否被当前文件占用*/
		if(0 == Ddbm_Bbm_Used(fs_index, st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx))
		{
			ui_BbmIdx = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx;
		}
		else/*下一个BBM节点没有被文件占用，必须截断*/
		{
			/*记录当前扇区内容到日志*/
			
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx = 0xFFFFFFFF;
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmIdx), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc));
			/*BBM的起始扇区+BBM编号 = 当前BBM扇区*/
			/*BBM写盘*/
			Ddbm_Write_BbmInfo(pDev, fs_index, ui_BbmIdx);
		}
	}
    return ui_BbmIdx;
}

/************************************************
**函数名称：Ddbm_Get_BbmList_Size
**
**功能：获得BBM链表的所包含BBM的数量
**
**输入参数：fs_index :文件系统编号
          ui_BbmHeadIdx :文件链表BBM头节点
**
**输出参数：BBM链表的所包含BBM的数量: >=0，失败:-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Get_BbmChain_Size(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx)
{
	uint32_ddbm ui_BbmIdx;
	uint32_ddbm ui_BbmChainSize;

	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -1;
	if(ui_BbmHeadIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;

	ui_BbmIdx = ui_BbmHeadIdx;
	ui_BbmChainSize = 1;
	while(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx != 0xFFFFFFFF)
	{
		/*检查下一个BBM节点是否被当前文件占用*/
		if(0 == Ddbm_Bbm_Used(fs_index, st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx))/*下一个BBM节点属于当前文件*/
		{
			ui_BbmIdx = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx;
			ui_BbmChainSize++;
		}
		else/*下一个BBM节点没有被文件占用，必须截断*/
		{
			/*记录当前扇区内容到日志*/	
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx = 0xFFFFFFFF;
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmIdx), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc));
			/*BBM的起始扇区+BBM编号 = 当前BBM扇区*/
			/*BBM写盘*/
			Ddbm_Write_BbmInfo(pDev, fs_index, ui_BbmIdx);
			
		}
	}
    return ui_BbmChainSize;
}
/************************************************
**函数名称：Ddbm_Append_BbmList
**
**功能：在当前节点后面添加新节点
**
**输入参数：fs_index :文件系统编号
          ui_BbmCurrentIdx :BBM节点编号
**
**输出参数：成功: 0，失败:-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Append_BbmList(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx)
{
	int32_ddbm i_Ret;


	i_Ret = Ddbm_BbmQueue_Receive(fs_index);
	if(i_Ret < 0)
		return -1;
	if(ui_BbmCurrentIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;

	/*更新当前BBM内容*/
	st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx = i_Ret;
	st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmCurrentIdx), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_Crc));
	/*BBM的起始扇区+BBM编号 = 当前BBM扇区*/
	/*BBM写盘*/
	Ddbm_Write_BbmInfo(pDev, fs_index, ui_BbmCurrentIdx);

	/*更新下一个BBM内容*/
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_BbmIdx = i_Ret;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_DbiIdx = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_DbiIdx;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_DataType = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_DataType;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_PreBbmIdx = ui_BbmCurrentIdx;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_NextBbmIdx = 0xFFFFFFFF;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ull_WrtPosition = 0;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ull_SequenceNum = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ull_SequenceNum + 1;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_UsedFlag = 1;
	st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + i_Ret), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[i_Ret].ui_Crc));

	/*BBM写盘*/
	Ddbm_Write_BbmInfo(pDev, fs_index, i_Ret);
	return 0;
}

/************************************************
**函数名称：Ddbm_Judge_Bbm
**
**功能：判断当前磁盘是否有空余的BBM可写
**
**输入参数：fs_index :文件系统编号
**
**输出参数：有: 0，没有:-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Spare_Bbm(uint32_ddbm fs_index)
{
	
	if(st_FsBbm[fs_index].st_BbmQueue.ui_Count > 0)
		return 0;
	else
		return -1;
}

/************************************************
**函数名称：Ddbm_Del_BbmList
**
**功能：删除整条BBM链表
**
**输入参数：fs_index :文件系统编号
**        BBM头节点编号
**
**输出参数：成功: 0，失败:-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Del_BbmList(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmHeadIdx)
{
	int32_ddbm i_Ret;
	uint32_ddbm ui_BbmIdx;
	uint32_ddbm ui_PreBbmIdx;


	i_Ret = Ddbm_Get_BbmList_Tail(pDev, fs_index, ui_BbmHeadIdx);
	if(i_Ret < 0)
		return -1;

	ui_BbmIdx = i_Ret;
	/*有效的时候进行循环删除*/
	do{
		ui_PreBbmIdx = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_PreBbmIdx;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_DbiIdx = 0xFFFFFFFF;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_DataType = 0xFFFFFFFF;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_PreBbmIdx = 0xFFFFFFFF;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_NextBbmIdx = 0xFFFFFFFF;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ull_WrtPosition = 0;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ull_SequenceNum = 0xFFFFFFFF;
		st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_UsedFlag = 0;
        st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmIdx), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmIdx].ui_Crc));

		/*BBM写盘*/
        Ddbm_Write_BbmInfo(pDev, fs_index, ui_BbmIdx);
		/*将释放出来的BBM节点放入BBM队列*/
		i_Ret = Ddbm_BbmQueue_Send(fs_index, ui_BbmIdx);
		if(i_Ret < 0)
		{
			return -1;
		}
		ui_BbmIdx = ui_PreBbmIdx;  //zsc add
		
	}while(ui_PreBbmIdx < gst_BootConfig[fs_index].ui_BbmSum);
	return 0;
}

/************************************************
**函数名称：Ddbm_New_BbmList
**
**功能：新建BBM链表
**
**输入参数：fs_index :文件系统编号
**
**输出参数：BBM编号；失败：-1；
**
**注释：文件新建，第一次写入数据时调用。上层应用调用了这个函数之后，一定要把BBM的PreIdx置0XFFFFFFF5？(zsc)
**
*/
extern int32_ddbm Ddbm_New_BbmList(uint32_ddbm fs_index)
{
	int32_ddbm i_Ret;

	i_Ret = Ddbm_BbmQueue_Receive(fs_index);
	if(i_Ret < 0)
		return -1;
		
	return i_Ret;
}

/************************************************
**函数名称：Ddbm_Get_One_Bbm
**
**功能：从BBM队列中获取一个有效的BBM
**
**输入参数：fs_index :文件系统编号
**
**输出参数：BBM编号；失败：-1；
**
**注释：文件新建，第一次写入数据时调用。上层应用调用了这个函数之后，一定要把BBM的PreIdx置0XFFFFFFF5？(zsc)
**
*/
extern int32_ddbm Ddbm_Get_One_Bbm(uint32_ddbm fs_index)
{
	int32_ddbm i_Ret;

	i_Ret = Ddbm_BbmQueue_Receive(fs_index);
	if(i_Ret < 0)
		return -1;
		
	return i_Ret;
}

/************************************************
**函数名称：Ddbm_Bbm_Used
**
**功能：判断当前BBM是否在BBM链中或者能够放入BBM链中,头节点和其他节点分别进行判断
**
**输入参数：fs_index :文件系统编号
**        ui_BbmCurrentIdx :BBM节点
**
**输出参数：0，BBM被文件占用，不能放BBM队列中; -1，该BBM没有被文件占用，可以放到BBM队列中;-2，其他错误，同样不能放到BBM队列
**
**注释：BBM节点的有效性判断包括本身的有效性，以及链是否准确（BBM链的头节点的前向节点为0xFFFFFFF0,64*4P），不包括CRC校验（上电已经做了一致性检椋珻RC校验和分区与备份分区是否一致）
**
*/
extern int32_ddbm Ddbm_Bbm_Used(uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx)
{
	uint32_ddbm index;

	if(fs_index >= FS_NUM)
		return -2;
	
	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -2;

	if(ui_BbmCurrentIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -2;

	/*BBM节点无效性判断*/
	if(!st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_UsedFlag)
		return -1;

	index = st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_PreBbmIdx;

	/*如果是BBM链的头节点，需要做特殊判断*/
	if(index == 0xFFFFFFF5)
  		return 0;

  
	/*BBM指向的前节点无效性判断*/
	if(index >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;

	/*BBM的前后节点一致性判断*/
	if(st_FsBbm[fs_index].gst_BbmInfo[index].ui_NextBbmIdx != ui_BbmCurrentIdx)
		return -1;

	return 0;
}
/************************************************
**函数名称：Ddbm_Write_BbmInfo
**
**功能：更新BBM扇区（1个扇区）
**
**输入参数：fs_index :文件系统编号
**        BBM节点编号
**
**输出参数：成功: BBM地址，失败:NULL
**
**注释：按照小端存储
**
*/
extern void Ddbm_Write_BbmInfo(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx)
{
	uint64_ddbm ull_Sector;
	/*BBM写盘*/
	ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector + ui_BbmCurrentIdx;
	wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmCurrentIdx), ull_Sector, 1);
	/*备份BBM写盘*/
	ull_Sector = gst_BootConfig[fs_index].ull_Bbm2StartSector + ui_BbmCurrentIdx;
	wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmCurrentIdx), ull_Sector, 1);
}

/************************************************
**函数名称：Ddbm_Read_BbmInfo
**
**功能：获得BBM信息
**
**输入参数：fs_index :文件系统编号
**         BBM节点编号
**
**输出参数：成功: BBM地址，失败:NULL
**
**注释：
**
*/
extern STRUCT_BBM * Ddbm_Read_BbmInfo(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx)
{
	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return NULL;
	if( ui_BbmCurrentIdx >= gst_BootConfig[fs_index].ui_BbmSum || ui_BbmCurrentIdx <0)  /*zsc add <0*/
		return NULL;
    return (st_FsBbm[fs_index].gst_BbmInfo + ui_BbmCurrentIdx);
}


/************************************************
**函数名称：Ddbm_Get_NextBbm
**
**功能：获得当前BBM下一个节点的索引号
**
**输入参数：fs_index :文件系统编号
**         ui_BbmCurrentIdx：当前BBM编号
**
**输出参数：-1：失败，成功：>=0
**
**注释：无
**
*/
extern int32_ddbm Ddbm_Get_NextBbm(void *pDev, uint32_ddbm fs_index, uint32_ddbm ui_BbmCurrentIdx)
{


	if(!st_FsBbm[fs_index].gst_BbmInitFlag)
		return -1;

	if(ui_BbmCurrentIdx >= gst_BootConfig[fs_index].ui_BbmSum)
		return -1;

	if(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx == 0xFFFFFFFF)
		return -1;
#if 0
	/*检验下一个BBM节点无效 modify by zsc*/
	/*Ddbm_Bbm_Used返回0，表示BBM有效正在使用不能放到队列中，返回-1，表示BBM没有被使用可以放到队列中*/
	/*如果当前BBM的下一个BBM没有在使用，返回-1*/
	if(Ddbm_Bbm_Used(fs_index, st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx))
	{
		/*当前BBM的下一个BBM没有越界*/
		if(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx != 0xFFFFFFFF)
		{
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx = 0xFFFFFFFF;
			st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + ui_BbmCurrentIdx), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_Crc));
			/*BBM写盘*/
			Ddbm_Write_BbmInfo(pDev, fs_index, ui_BbmCurrentIdx);
		}
		return -1;
	}
#endif 

	return (st_FsBbm[fs_index].gst_BbmInfo[ui_BbmCurrentIdx].ui_NextBbmIdx);
}
/************************************************
**函数名称：Ddbm_Init_BbmPartiton
**
**功能：BBM区初始化
**
**输入参数：fs_index :文件系统编号
**
**输出参数：成功 0，失败-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Init_BbmPartiton(void *pDev, uint32_ddbm fs_index)
{
	int32_ddbm i_Ret;
    uint32_ddbm index;

	uint32_ddbm ui_Crc1;
	uint32_ddbm ui_Crc2;
	uint64_ddbm ull_Sector;
	uint64_ddbm ull_Length;
	
	if(st_FsBbm[fs_index].gst_BbmInitFlag == 1)
	{
		Ddbm_UnMount_BbmPartiton(fs_index);
	}

	i_Ret = Ddbm_BbmQueue_Init(fs_index);
	if(i_Ret < 0)
		return -1;
	/*这一步操作涉及申请大量空间内存*/
	st_FsBbm[fs_index].gst_BbmInfo = (STRUCT_BBM *)malloc(sizeof(STRUCT_BBM) * gst_BootConfig[fs_index].ui_BbmSum);
	if(st_FsBbm[fs_index].gst_BbmInfo == NULL)
	{
		free(st_FsBbm[fs_index].st_BbmQueue.pui_BbmQueue);
		return -1;
	}
	/*初始化新开辟的BBM内存区域*/
	/*前半段BBM内存空间*/
	ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector;
	ull_Length = gst_BootConfig[fs_index].ui_BbmSum/2;
	rd_sector(pDev, (unsigned char*)st_FsBbm[fs_index].gst_BbmInfo, ull_Sector, ull_Length);

	ull_Sector = gst_BootConfig[fs_index].ull_Bbm2StartSector;
	rd_sector(pDev, (unsigned char*)(st_FsBbm[fs_index].gst_BbmInfo + gst_BootConfig[fs_index].ui_BbmSum/2), ull_Sector, ull_Length); 

	for(index = 0; index < gst_BootConfig[fs_index].ui_BbmSum/2; index++)
	{
		ui_Crc1 = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));
		ui_Crc2 = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + gst_BootConfig[fs_index].ui_BbmSum/2 + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));
		if(ui_Crc1 != st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc)
		{
			if(ui_Crc2 == st_FsBbm[fs_index].gst_BbmInfo[index + gst_BootConfig[fs_index].ui_BbmSum/2].ui_Crc)
			{	/*备份BBM替换BBM写盘*/
				ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector + index;
				wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index + gst_BootConfig[fs_index].ui_BbmSum/2), ull_Sector, 1);
			}
			else
			{
				
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_DbiIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_DataType = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_PreBbmIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_NextBbmIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ull_WrtPosition = 0;
				st_FsBbm[fs_index].gst_BbmInfo[index].ull_SequenceNum = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_UsedFlag = 0;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));

				/*BBM写盘*/
				Ddbm_Write_BbmInfo(pDev, fs_index,index);
			}
		}
		else if((ui_Crc2 != st_FsBbm[fs_index].gst_BbmInfo[index + gst_BootConfig[fs_index].ui_BbmSum/2].ui_Crc)||(ui_Crc1 != ui_Crc2))
		{
			/*BBM替换备份BBM写盘*/
			ull_Sector = gst_BootConfig[fs_index].ull_Bbm2StartSector + index;
			wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), ull_Sector, 1);
		}
	}
	/*后半段BBM内存空间*/
	ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector + gst_BootConfig[fs_index].ui_BbmSum/2;
	ull_Length = gst_BootConfig[fs_index].ui_BbmSum/2;
	rd_sector(pDev, (unsigned char*)st_FsBbm[fs_index].gst_BbmInfo, ull_Sector, ull_Length);

	ull_Sector = gst_BootConfig[fs_index].ull_Bbm2StartSector + gst_BootConfig[fs_index].ui_BbmSum/2;

	rd_sector(pDev, (unsigned char*)(st_FsBbm[fs_index].gst_BbmInfo + gst_BootConfig[fs_index].ui_BbmSum/2), ull_Sector, ull_Length); 

	for(index = 0; index < gst_BootConfig[fs_index].ui_BbmSum/2; index++)
	{
		ui_Crc1 = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));
		ui_Crc2 = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + gst_BootConfig[fs_index].ui_BbmSum/2 + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));
		if(ui_Crc1 != st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc)
		{
			if(ui_Crc2 == st_FsBbm[fs_index].gst_BbmInfo[index + gst_BootConfig[fs_index].ui_BbmSum/2].ui_Crc)
			{	/*BBM写盘*/
				ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector + gst_BootConfig[fs_index].ui_BbmSum/2 + index;
				wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index + gst_BootConfig[fs_index].ui_BbmSum/2), ull_Sector, 1);
			}
			else
			{
				
				/*风险1：某个BBM失效，该BBM被清空，导致某个文件失效且被截断。风险2：该BBM后续链表没有被释放，导致BBM空间浪费。（改写清空修改BBM）*/
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_DbiIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_DataType = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_PreBbmIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_NextBbmIdx = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ull_WrtPosition = 0;
				st_FsBbm[fs_index].gst_BbmInfo[index].ull_SequenceNum = 0xFFFFFFFF;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_UsedFlag = 0;
				st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), sizeof(STRUCT_BBM) - sizeof(st_FsBbm[fs_index].gst_BbmInfo[index].ui_Crc));

				/*BBM写盘*/
				Ddbm_Write_BbmInfo(pDev, fs_index, index);
			}
		}
		else if((ui_Crc2 != st_FsBbm[fs_index].gst_BbmInfo[index + gst_BootConfig[fs_index].ui_BbmSum/2].ui_Crc)||(ui_Crc1 != ui_Crc2))
		{
			/*备份BBM写盘*/
			ull_Sector = gst_BootConfig[fs_index].ull_Bbm2StartSector + gst_BootConfig[fs_index].ui_BbmSum/2 + index;
			wrt_sector(pDev, (uint8_ddbm *)(st_FsBbm[fs_index].gst_BbmInfo + index), ull_Sector, 1);
		}
	}

	ull_Sector = gst_BootConfig[fs_index].ull_BbmStartSector;
	ull_Length = gst_BootConfig[fs_index].ui_BbmSum;
	rd_sector(pDev, (unsigned char*)st_FsBbm[fs_index].gst_BbmInfo, ull_Sector, ull_Length);

	for(index = 0; index < gst_BootConfig[fs_index].ui_BbmSum; index++)
	{
		if(!st_FsBbm[fs_index].gst_BbmInfo[index].ui_UsedFlag)
			i_Ret = Ddbm_BbmQueue_Send(fs_index, index);
	}
	st_FsBbm[fs_index].gst_BbmInitFlag = 1;
	return 0;
}


/************************************************
**函数名称：Ddbm_Crc32
**
**功能：计算CRC校验和
**
**输入参数：ptr：无符号字符指针；len：无符号字符长度
**
**输出参数：CRC校验和
**
**注释：无
**
*/
extern uint32_ddbm Ddbm_Crc_Check(uint8_ddbm *ptr, uint32_ddbm len)
{
	uint32_ddbm ui_Crc = 0xffffffff;
	uint8_ddbm uc_CrcTblIdx;
	while(len-- != 0)
	{
        uc_CrcTblIdx = ui_Crc^*ptr++;
        ui_Crc >>= 8;
        ui_Crc ^= ui_CrcTbl[uc_CrcTblIdx];
	}
	return ui_Crc;
}


/*
void Ddbm_Get_Locked()
{
	if()
	{	SEMD_TAKE() //阻塞锁（一个,局部）
        SEME_TAKE()
	    SEMD_GAVE()

		if()
			SEMA_TAKE() //阻塞锁（一个,局部）
			SEMB_TAKE() //共享锁（多个,局部）
			SEMA_GAVE() //释放阻塞锁
			//进行读操作
			SEMB_GAVE() //释放共享锁
		else
		{
			SEMA_TAKE() //获得阻塞锁
			SEMC_TAKE() //升级为保留锁（一个局部，共享锁被全部释放）
			//进行写操作,关闭文件
			SEMC_GAVE()
			SEMA_GAVE()
		}
		SEME_GAVE()
	}
	else
	{
		SEMD_TAKE()
        SEMF_TAKE() //E释放完
		close
		SEMF_GAVE()
		SEMD_GAVE()
}
*/

