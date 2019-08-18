/*-----------------------------------------------------------
**
** 版权:    中国航空无线电电子研究所, 2016年
**
** 文件名:  Ddbm_Corelib.c
**
** 描述:    定义了DDBM文件系统DBI核心层
**
**
** 设计注记:
**
** 作者:
**     王夕臣，王博，章诗晨, 2018年10月开始编写本文件
**
** 更改历史:
**
**-----------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Ddbm_Corelib.h"
#include "Ddbm_Partition.h"
#include "Ddbm_Bbm.h"
#include "Ddbm_MemBlk.h"
#include "DiskIO.h"
/*
文件系统分为三层：
第一层：为用户层
第二层：为DBI核心层
第三层：为BBM驱动层
另需要磁盘的读写接口；
为了与挂在标准IO上，用户层需要做相应的改动
*/

#pragma pack(1)
/*DDI信息*/
typedef struct _STRUCT_DDI_					/*动态信息*/
{
    int32_ddbm   ui_LastDbiIdx;			    /*最新可分配的DBI编号*/
    uint64_ddbm   ull_FsBootNum;			/*文件系统启动次数*/
    uint8_ddbm    uc_Reserved[492];			/*保留字节*/
    uint32_ddbm   ui_EndFlag;				/*结束标记*/
    uint32_ddbm   ui_Crc;					/*CRC校验*/
}STRUCT_DDI;

/*DBI信息*/
typedef struct _STRUCT_DBI_
{
    uint32_ddbm   ui_DbiIdx;				/*DBI索引号*/
    uint32_ddbm   ui_ValidFlag;				/*有效标记*/
    int8_ddbm     uc_FileName[256];			/*文件名称*/
    uint32_ddbm   ui_FirstBbmIdx;			/*分配的起始BBM编号*/
    uint32_ddbm   ui_BbmChainSize;			/*分配的BBM链大小*/
    uint64_ddbm   ull_FsBootNum;			/*文件系统启动次数*/
    uint64_ddbm   ui_CreateTime;			/*创建时间*/
    uint64_ddbm   ui_ModifyTime;			/*最近一次修改时间*/
    uint64_ddbm   ui_CloseTime;				/*最近一次关闭时间*/
    uint64_ddbm   ull_FileSize;				/*文件大小*/
    uint32_ddbm   ui_ReferenceCnt;			/*同一DBI被多任务打开所引用的次数*/
    uint8_ddbm    uc_Reserved[188];			/*保留字节*/
    uint32_ddbm   ui_EndFlag;				/*结束标记*/
    uint32_ddbm   ui_Crc;					/*CRC校验*/
}STRUCT_DBI;

#pragma pack()
/*文件指针信息*/

/*阻塞锁*/
#define PENDING_SEM_CREATE(MUTEX)
#define PENDING_SEM_TAKE(MUTEX)   
#define PENDING_SEM_GIVE(MUTEX)
#define PENDING_SEM_DELETE(MUTEX)
/*共享锁，10个*/
#define SHARED_SEM_NUM   10
#define SHARED_SEM_CREATE(MUTEX)
#define SHARED_SEM_TAKE(MUTEX)   
#define SHARED_SEM_GIVE(MUTEX)

/*保留锁*/
#define RESERVED_SEM_CREATE(MUTEX)
#define RESERVED_SEM_TAKE(MUTEX)   
#define RESERVED_SEM_GIVE(MUTEX) 

#define FULL_NAME_LEN 256
#define HASH_SIZE 100

#define SD_HASH_FULLTAB	2	/* rehash when table gets this x full */
#define SD_HASH_GROWTAB 4	/* grow table by this factor */
#define SD_HASH_DEFAULT_SIZE 100 /* self explenatory */

/*全局变量*/
int32_ddbm ddbm_errno;
STRUCT_TIME ddbm_time;

typedef struct{
	int32_ddbm index;
	int32_ddbm status;
	SEM_ID_DDBM pending;
}STRUCT_HASH_DATA;

/*哈希表操作函数*/
typedef struct __sd_hash_ops {
    unsigned int	(*hash)		(const char*); /*哈希映射函数（乘法迭代运算+除留余数法）*/
    int			(*compare)	(const char*, const char*); /*关键字比较（字符串比较）*/
    void*		(*key_dup)	(const void*); /*关键字转换（key），或者称为加密*/
    void		(*key_free)	(void*);       /*关键字内存释放*/
    void*		(*data_dup)	(const void*); /*数据转换(data)，或者称为加密*/
    void		(*data_free)	(void*);   /*数据内存释放*/
}sd_hash_ops_t;

/*哈希单元*/
typedef struct hash_iter{
	int8_ddbm key[FULL_NAME_LEN];/*名称,关键字*/
	STRUCT_HASH_DATA data;/*DBI index数据*/
	void* hash; /*包含当前哈希单元的哈希表（用的时候强制类型转换sd_hash_t）*/ 
    uint32_ddbm __hkey; /*哈希值，关键字的映射值*/
    struct hash_iter *__next; /*下一个链域节点*/
    struct hash_iter *__prev; /*前一个链域节点*/
}STRUCT_FILE_HASH_ITER;

/*哈希表全局信息*/
typedef struct __sd_hash {
    uint32_ddbm	nelem;   /*哈希表插入总元素数量*/
    uint32_ddbm	size;    /*哈希表总大小*/
	uint32_ddbm fs_index; /*当前hash表所属的文件系统编号*/
	/*二维数组表示链域*/
    STRUCT_FILE_HASH_ITER** tab; /*哈希表，tab[i]是一个存储单元序号，表示一个链域*/
    const sd_hash_ops_t* ops; /*哈希表操作函数*/
}sd_hash_t;


/*除留余数，保证不会使得返回值超过整个哈希表的大小*/
#define hindex(h, n)	((h)%(n))	

typedef struct{
	/*信号量*/
	int32_ddbm  semPending;
	int32_ddbm  semShared;
	/*DDI信息*/
	STRUCT_DDI gst_DdiInfo;         /*备份DDI*/
    STRUCT_DDI gst_DdiPowerOnInfo;  /*主DDI*/
	/*DBI信息*/
	int32_ddbm ui_MemBlkIndex;    /*MemBlk内存编号(初始化时确定)*/
	sd_hash_t*  h_DbiTable;
	/*DEVS名字*/
	int8_ddbm c_DevsName[FULL_NAME_LEN];
	int32_ddbm *pDev;
}STRUCT_FS_CORE;


/*文件系统结构体信息，包括文件系统阻塞信号量和共享信号量、DDI信息*/
//static STRUCT_FS_CORE st_FsCore[FS_NUM];// = {{0, 0, {}, {}, -1, NULL, {0}, NULL}, {0, 0, {}, {}, -1, NULL, {0}, NULL}};
static STRUCT_FS_CORE st_FsCore[FS_NUM] = {{0, 0, {}, {}, -1, NULL, {0}, NULL}, {0, 0, {}, {}, -1, NULL, {0}, NULL}};
/*设备计数*/
static uint32_ddbm gui_DevsCount = 0;
/*全局信号量(设备计数保护)*/
static int32_ddbm sem_GlobalFsVar = 0;

STRUCT_BOOTSECTOR gst_BootConfig[FS_NUM];
/* DDBM文件系统在 vxWorks 驱动表中的设备号 */
int	ddbmDrvNum = DISKIO_ERROR; 

/*.BH--------------------------------------------------------------
**
** sd_hash_hash_string
** 
** 描述：哈希函数。TIME33算法
**
** 输入参数：a_string:关键字（字符串名字）
**
** 输出参数：无。
**
** 设计注记：计算哈希值，再对其映射。
**
**.EH--------------------------------------------------------------
*/
static unsigned int sd_hash_hash_string(const char* a_string)
{
    register unsigned int h;
    
    for (h = 0; *a_string != '\0'; a_string++)
		h = (unsigned int)*a_string + 33 * h;
    
    return h;
}
/*.BH--------------------------------------------------------------
**
** sd_hash_new
** 
** 描述：创建哈希表。
**
** 输入参数：a_size:哈希表大小
**         a_ops：哈希表操作结构体（包括哈希映射函数、哈希比较函数等）
**
** 输出参数：哈希表指针。
**
** 设计注记：默认的哈希操作是：字符串乘法迭代、字符串比较函数。
**
**.EH--------------------------------------------------------------
*/
static sd_hash_t* sd_hash_new(uint32_ddbm a_size, uint32_ddbm fs_index, const sd_hash_ops_t* a_ops)
{
    const static sd_hash_ops_t default_ops = {&sd_hash_hash_string, &strcmp, 0, 0, 0, 0};
    
    sd_hash_t* hash;
    STRUCT_FILE_HASH_ITER** tab;
    
	/*默认哈希大小为SD_HASH_DEFAULT_SIZE*/
    if (a_size == 0) a_size = SD_HASH_DEFAULT_SIZE;
    /*申请哈希表内存*/
    hash = (sd_hash_t*)calloc(1, sizeof(*hash));
    tab = (STRUCT_FILE_HASH_ITER**)calloc(a_size, sizeof(*tab));
    
    if (hash == 0 || tab == 0) {
		free(hash);
		free(tab);
		return 0;
    }
    /*初始化哈希表*/
    hash->nelem	= 0;
    hash->size	= a_size;
    hash->tab	= tab;
	hash->fs_index = fs_index;
    hash->ops	= a_ops != 0 ? a_ops : &default_ops;
    
    return hash;
}
/*.BH--------------------------------------------------------------
**
** sd_hash_lookup
** 
** 描述：哈希表查询。
**
** 输入参数：a_this:哈希表指针
**         a_key：关键字指针
**
** 输出参数：哈希表的存储单元，即链域结点指针。
**
** 设计注记：哈希地址计算：1、hash映射函数得到哈希值2、除留余数得到哈希地址。
**         目的是压缩映像，采用拉链法解决冲突。
**
**.EH--------------------------------------------------------------
*/
static STRUCT_FILE_HASH_ITER* sd_hash_lookup(sd_hash_t* a_this, const void* a_key)
{
    int h;
    STRUCT_FILE_HASH_ITER*	p;
    
	/*判断参数是否正确*/
    if (a_this == 0 || a_key == 0) return NULL;
    
	/*使用哈希表hash映射函数计算关键字的哈希值，除留余数，得到哈希记录位置*/
    h = hindex(a_this->ops->hash((const char*)a_key), a_this->size);
	/*每一个记录位置既是一个链域，tab[h]为链的头指针，比较给定关键字与存储单元的关键字是否一致*/
    for (p = a_this->tab[h]; p != 0; p = p->__next)
		if(a_this->ops->key_dup != 0){
			if (a_this->ops->compare((const char*)a_this->ops->key_dup(a_key), p->key) == 0) {
				return p;
			}
		}
		else
		{
			if (a_this->ops->compare((const char*)a_key, p->key) == 0) {
				return p;
			}
		}
			
    /*没有查询到则返回空*/
    return NULL;
}

/*.BH--------------------------------------------------------------
**
** sd_hash_lookadd
** 
** 描述：哈希表查询关键字，查到了就返回该关键字表示的哈希节点，查询不到则添加该关键字并返回节点。
**
** 输入参数：a_this:哈希表指针
**         a_key：关键字指针
**
** 输出参数：哈希表的存储单元，即链表节点指针。
**
** 设计注记：哈希表将数据散列开来，如果需要存储的数据（成员）较多，则需要扩展哈希表；
           哈希表的key不单独申请内存。
**
**.EH--------------------------------------------------------------
*/
static STRUCT_FILE_HASH_ITER* sd_hash_lookadd(sd_hash_t* a_this, const void* a_key)
{
    int h;
    STRUCT_FILE_HASH_ITER*	p;

    /*判断参数是否正确*/
    if (a_this == 0 || a_key == 0)			return 0;
	/*哈希表查询该关键字,如果key表示的文件名在哈希表中已经存在，则直接返回该文件本身。*/
    if ((p = sd_hash_lookup(a_this, a_key)) != 0)	return p;
	
	/*没有找到，准备创建哈希表存储单元（链表结点）*/
    if ((p = (STRUCT_FILE_HASH_ITER*)calloc(1, sizeof(*p))) == 0)		return 0;
    
	/*如果哈希表key_dup函数不为零，则关键字进行加密处理，否则直接复制不加密*/
    if (a_this->ops->key_dup != 0)
		;/*p->key = (a_this->ops->key_dup(a_key);*/
    else
		strcpy(p->key, (const char*)a_key);
    
	/*使用哈希表hash映射函数计算关键字的哈希值，此处不是p->key*/
    p->hash = a_this;
    p->__hkey = a_this->ops->hash((const char*)a_key);
    
	/*除留余数，得到哈希记录位置*/
    h = hindex(p->__hkey, a_this->size);
	/*在链表头插入该结点(新的节点插入在前面)*/
    p->__next = a_this->tab[h];
    a_this->tab[h] = p;
    if (p->__next != 0) p->__next->__prev = p;
    /*哈希存储累加*/
    a_this->nelem++;
    
    return p;
}

/*.BH--------------------------------------------------------------
**
** sd_hash_add
** 
** 描述：往哈希表中添加关键字和data成员。
**
** 输入参数：a_this:哈希表指针
**         a_key：关键字指针
**         a_data：数据成员指针，如appender、layout、category
** 输出参数：哈希表的存储单元，即链域结点指针。
**
** 设计注记：哈希表的key和data不单独申请内存。
**
**.EH--------------------------------------------------------------
*/
static STRUCT_FILE_HASH_ITER* sd_hash_add(sd_hash_t* a_this, const void* a_key, void * a_data)
{
    STRUCT_FILE_HASH_ITER* p;
    /*在哈希表中查询并添加关键字*/
    if ((p = sd_hash_lookadd(a_this, a_key)) == 0) return 0;
    /*释放表中存储单元中的data成员内存*/
    if (a_this->ops->data_free != 0) 
		;/*a_this->ops->data_free((p->data));*/
	else 
		;
    /*更新存储单元的data成员*/
    if (a_this->ops->data_dup != 0)
		;/*p->data = a_this->ops->data_dup(a_data);*/
    else{
		memcpy((char*)&p->data, a_data, sizeof(STRUCT_HASH_DATA));
    }
	
    return p;
}

/*.BH--------------------------------------------------------------
**
** sd_hash_clear
** 
** 描述：清空哈希表，释放哈希表每一个记录链表（释放链表节点内存，节点中成员释放不在此处）。
**
** 输入参数：a_this:哈希表指针
**
** 输出参数：无。
**
** 设计注记：谨慎调用，调用前首先需要释放其成员的的内存
   key_free和data_free为空，data和key在哈希表添加的时候没有单独申请内存, 
   遵循内存申请负责内存释放,其中key的内存申请是在data中。
**
**.EH--------------------------------------------------------------
*/
static void sd_hash_clear(sd_hash_t* a_this)
{
    int32_ddbm h;
    STRUCT_FILE_HASH_ITER*	p;
    STRUCT_FILE_HASH_ITER*	q;
    
    if (a_this == 0) return;

	/*释放关键字*/
	/*
	if (a_this->ops->key_free) 
		a_this->ops->key_free(p->key);  
	else
		;
	*/
	
	/*释放目标数据*/
	/*
	if (a_this->ops->data_free) 
		a_this->ops->data_free(p->data);
	else
		;
	*/
	
    for (h = 0; h < a_this->size; h++){      /*哈希表的记录单元，链域*/
		for (p = a_this->tab[h]; p; p = q) { /*哈希表的存储单元，链表节点*/
			q = p->__next;
			if(!p->data.pending)
				PENDING_SEM_DELETE(p->data.pending);
			/*释放链表节点*/
			Ddbm_MemBlk_Free(st_FsCore[a_this->fs_index].ui_MemBlkIndex, p);
			if(p != NULL)
			{
				free(p);  /*zsc add，对应st_hash_lookup函数中的p = calloc(1, sizeof(*p))*/
			}
			
			
		}
		a_this->tab[h] = NULL;
    }
    a_this->nelem = 0;
}

/*.BH--------------------------------------------------------------
**
** sd_hash_del_unit
** 
** 描述：根据关键字删除哈希单元
**
** 输入参数：a_this:哈希表指针
			a_key:关键字指针
			MemBlk_Id:内存序号
**
** 输出参数：无。
**
** 设计注记：无
**
**.EH--------------------------------------------------------------
*/
static void sd_hash_del_unit(sd_hash_t* a_this, const void* a_key, int MemBlk_Id){	
	int h;
	STRUCT_FILE_HASH_ITER *p;
	h = hindex(a_this->ops->hash((const char*)a_key), a_this->size);
	
	for (p = a_this->tab[h]; p != 0; p = p->__next)
		if (a_this->ops->compare((const char*)a_key, p->key) == 0)
				break;
	if(p == 0) return;
	/*释放关键字*/
	/*释放目标数据*/
	if(p->__next != 0) p->__next->__prev = p->__prev;
	if(p->__prev != 0) p->__prev->__next = p->__next;
	else a_this->tab[h] = p->__next;

	a_this->nelem--;
	/*释放链表节点*/
	Ddbm_MemBlk_Free(MemBlk_Id, p);
		
}

/*.BH--------------------------------------------------------------
**
** sd_hash_delete
** 
** 描述：删除哈希表，此处不对哈希表的记录（链表）节点数据内存释放。
**
** 输入参数：a_this:哈希表指针
**
** 输出参数：无。
**
** 设计注记：释放 1是哈希表记录链表中节点数据内存 2是哈希表记录链表 3是哈希表 4是整个hash结构
           谨慎调用，调用之前需释放链表节点中的数据内存，此处释放：2，3，4。
**
**.EH--------------------------------------------------------------
*/
static void sd_hash_delete(sd_hash_t* a_this){	
	/*释放哈希表记录链表内存*/    
	sd_hash_clear(a_this);	
	if (a_this == 0) return;
	/*释放哈希表内存*/    
	free(a_this->tab);	
	/*释放哈希指针内存*/    
	free(a_this);
}

/*.BH--------------------------------------------------------------
**函数名称：Ddbm_Delete_LastDbi
**
**功能：1.如果有需要，删除最旧的文件(只删除一个)，并释放他们的存储空间(BBM空间不够用了) 2.更新lastdbi
**
**输入参数：fs_index :文件系统编号  i_flag:模式(0写文件，1打开文件)
**
**输出参数：0成功 ,-1失败
**
**注释：如果是写盘时候调用，只有在磁盘空间满了，才会需要调用这个函数；如果是打开文件需要调用，则磁盘有可能没有写满也有可能已经写满
**
**.EH--------------------------------------------------------------
*/
static int32_ddbm Ddbm_Delete_LastDbi(uint32_ddbm fs_index, int32_ddbm i_flag)
{
	STRUCT_DBI  st_DbiInfo;

	uint64_ddbm ull_Sector;

	int32_ddbm i_Ret = 0;
	uint32_ddbm index;

	/*读取ui_LastDbiIdx的下一个DBI信息*/
	ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + (st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx + 1) % gst_BootConfig[fs_index].ui_DbiSum;
    rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	/*如果磁盘已经发生DBI的循环读写，最旧的文件就是下一个DBI，对它进行处理即可*/
	if(((st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx + 1) % gst_BootConfig[fs_index].ui_DbiSum < gst_BootConfig[fs_index].ui_DbiSum) && st_DbiInfo.ui_ValidFlag)
	{
		st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx = (st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx + 1) % gst_BootConfig[fs_index].ui_DbiSum;
		i_Ret = Ddbm_Delete_File(fs_index, st_DbiInfo.uc_FileName);
		if(i_Ret >= 0)  /*DBI从0开始，因此首个DBI也算*/
			printf("重要信息，删除文件%s,对应DBI编号%d,所在磁盘名称%s\n",st_DbiInfo.uc_FileName, i_Ret, st_FsCore[fs_index].c_DevsName);
		else
			return -1;
	}
	/*磁盘的第一遍循环读写还没有发生*/
	/*磁盘还没有循环到第一遍的DBI的情况下，当前磁盘DBU空间就已经全部用完，最旧的文件从第0个DBI开始算起，清理空间也是从它开始清起*/
	else		
	{	
		i_Ret = Ddbm_Spare_Bbm(fs_index);
		/*创建新文件需要腾出其他文件的空间或者写数据写不下都有可能跑到这里*/
		if(i_Ret < 0)
		{
			/*从头开始遍历*/
			for(index = 0; index < st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx; index++)
			{
				ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + index;
    			rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
				/*如果当前DBI非空，则释放当前DBI以及当前DBI的所有BBM*/
				if(st_DbiInfo.ui_ValidFlag)
					break;
			}
			i_Ret = Ddbm_Delete_File(fs_index, st_DbiInfo.uc_FileName);
			if(i_Ret >= 0)  /*DBI从0开始，因此首个DBI也算*/
				printf("重要信息，删除文件%s,对应DBI编号%d,所在磁盘名称%s\n",st_DbiInfo.uc_FileName, i_Ret, st_FsCore[fs_index].c_DevsName);
			else
				return -1;

			/*删除文件从0开始删除，但是ui_LastDbiIdx表示当前文件，仍然按照规则+1，否则open函数的逻辑就错乱了*/
			st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx = (st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx + 1) % gst_BootConfig[fs_index].ui_DbiSum;
		}
		/*否则仍然取下一个DBI作为当前的DBI，但是因为剩余空间足够，没必要为了腾出空间而删除了BBM(DBU)了。(写文件写不下不会跑到这里，创建文件会)*/
		else
		{
			st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx = (st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx + 1) % gst_BootConfig[fs_index].ui_DbiSum;
		}
		
	}
	
	st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_FsCore[fs_index].gst_DdiPowerOnInfo, sizeof(st_FsCore[fs_index].gst_DdiPowerOnInfo) - sizeof(st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_Crc));
	/*更新主DDI*/
	ull_Sector = gst_BootConfig[fs_index].ull_DdiStartSector;
	wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_FsCore[fs_index].gst_DdiPowerOnInfo, ull_Sector, 1);
	/*更新备份DDI*/
	ull_Sector = gst_BootConfig[fs_index].ull_Ddi2StartSector;
	wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_FsCore[fs_index].gst_DdiPowerOnInfo, ull_Sector, 1);
		
	
	return 0;
}

/************************************************
**函数名称：Ddbm_Close_FileSystem
**
**功能：关闭文件系统(内部函数。仅用于initFileSystem中调用！)
**
**输入参数：fs_index :文件系统编号
**
**输出参数：无
**
**注释：
**
*/
static void Ddbm_Close_FileSystem(uint32_ddbm fs_index)
{
	int32_ddbm index;
	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	for(index = 0; index < SHARED_SEM_NUM; index++)
		SHARED_SEM_TAKE(st_FsCore[fs_index].semShared);
	for(index = 0; index < SHARED_SEM_NUM; index++)
		SHARED_SEM_GIVE(st_FsCore[fs_index].semShared);
	/*清空BOOT分区信息*/
	memset(&gst_BootConfig[fs_index], 0x00, sizeof(gst_BootConfig[FS_NUM]));
	/*此处没有清空DBI内存空间，不允许不同的DBI总数设备插装*/
	/*删除DBI哈希表*/
	sd_hash_delete(st_FsCore[fs_index].h_DbiTable);
	
	PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);	
}

/************************************************
**函数名称：Ddbm_Init_FileSystem
**
**功能：初始化文件系统(init里面先调用了closefileSystem)
**
**输入参数：pc_devsName：盘符
**
**输出参数：成功 =0，失败-1
**
**注释：文件系统操作
**
*/
extern int32_ddbm Ddbm_Init_FileSystem(const int8_ddbm * pc_devsName)
{
	MEM_BLK_DEV *pDev;
    uint32_ddbm index = 0;
	uint64_ddbm ull_Sector;
	uint32_ddbm ui_FsCount;
	uint32_ddbm ui_Crc1, ui_Crc2;
	STRUCT_DBI st_DbiInfo;
	STRUCT_FILE_HASH_ITER* pst_FileHashIter;
	STRUCT_HASH_DATA st_HashData;
	/*创建全局信号量，保护所有文件系统名称访问*/
	if(!sem_GlobalFsVar){
		PENDING_SEM_CREATE(sem_GlobalFsVar);
	}
	/*参数判断*/
	if((!pc_devsName)||(strlen(pc_devsName) == 0)||(strlen(pc_devsName) > 32))
		return -1;
	/*获取全局信号量*/
	PENDING_SEM_TAKE(sem_GlobalFsVar);
	/*设备列表查询设备*/
	for(index = 0; index < FS_NUM; index++)
	{
		/*如果相等，则跳出循环，表示查询到已有设备，对当前设备进行操作*/
		if(!strcmp(pc_devsName, st_FsCore[index].c_DevsName))
			break;
	}
    //printf("st_FsCore[0].ui_MemBlkIndex=%d\n",st_FsCore[0].ui_MemBlkIndex);
    //printf("st_FsCore[1].ui_MemBlkIndex=%d\n",st_FsCore[1].ui_MemBlkIndex);
    //printf("[init]index = %d\n",index);
	if(FS_NUM == index){
		/*没有查询到*/
		/*是否超出文件系统数量上界*/
		if(gui_DevsCount >= FS_NUM){
			PENDING_SEM_GIVE(sem_GlobalFsVar);
			return -1;
		}
		/*获取设备指针,不存在不进行文件系统初始化，不占用文件系统上限,zsc:参数改成pc_devsName，原来是st_FsCore[ui_FsCount].c_DevsName*/
		pDev = (MEM_BLK_DEV *)getMemBlkDev((const uint8_ddbm*)pc_devsName);
		if(!pDev){
			PENDING_SEM_GIVE(sem_GlobalFsVar);
            printf("[Ddbm_Init_FileSystem]getMemBlkDev failed\n");
			return -1;
		}
		ui_FsCount = gui_DevsCount;
        //memset(&st_FsCore[ui_FsCount], 0x00, sizeof(st_FsCore[ui_FsCount]));    //这句话实在是太多余了！影响了后面的st_FsCore[ui_FsCount].ui_MemBlkIndex的判断！
		/*创建文件系统信号量*/
		PENDING_SEM_CREATE(st_FsCore[ui_FsCount].semPending);
		SHARED_SEM_CREATE(st_FsCore[ui_FsCount].semShared);
		/*记录设备名字*/
		memcpy(st_FsCore[ui_FsCount].c_DevsName, pc_devsName, strlen(pc_devsName));
		/*文件系统计数累加*/
		gui_DevsCount++;
	}else{
		/*查询到了,同样需要获取设备句柄*/
		pDev = (MEM_BLK_DEV*)getMemBlkDev((const uint8_ddbm*)pc_devsName);
		if(!pDev){
			PENDING_SEM_GIVE(sem_GlobalFsVar);
			return -1;
		}
		ui_FsCount = index;
	}
	PENDING_SEM_GIVE(sem_GlobalFsVar);
	
	/*文件系统关闭*/
	Ddbm_Close_FileSystem(ui_FsCount);
	/*文件系统保护*/
	PENDING_SEM_TAKE(st_FsCore[ui_FsCount].semPending);
	for(index = 0; index < SHARED_SEM_NUM; index++)
		SHARED_SEM_TAKE(st_FsCore[fs_index].semShared);
	for(index = 0; index < SHARED_SEM_NUM; index++)
		SHARED_SEM_GIVE(st_FsCore[fs_index].semShared);
	st_FsCore[ui_FsCount].pDev = (int *)pDev;
	/*获取文件系统的BOOT扇区,扇区编号是32*/
	ull_Sector = 32;   
	rd_sector(pDev, (unsigned char*)&gst_BootConfig[ui_FsCount], ull_Sector, 1);
	/*校验BOOT的CRC*/
	ui_Crc1 = Ddbm_Crc_Check((uint8_ddbm *)&gst_BootConfig[ui_FsCount], sizeof(gst_BootConfig[ui_FsCount]) - sizeof(gst_BootConfig[ui_FsCount].ui_Crc));
	if(ui_Crc1 != gst_BootConfig[ui_FsCount].ui_Crc)
	{
		PENDING_SEM_GIVE(st_FsCore[ui_FsCount].semPending);
        printf("[Ddbm_Init_FileSystem]crc err磁盘可能没有格式化，请先进行格式化操作\n");
        return -1;
        //return NEED_FORMAT;
	}
	/*获取文件系统的DDI扇区*/
	/*主DDI*/
	ull_Sector = gst_BootConfig[ui_FsCount].ull_DdiStartSector;
	rd_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, ull_Sector, 1);
	//printf("当前设备的LastDbiIdx = %d", st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_LastDbiIdx);
	/*备份DDI*/
	ull_Sector = gst_BootConfig[ui_FsCount].ull_Ddi2StartSector;
	rd_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiInfo, ull_Sector, 1);
	/*校验DDI和DBI扇区*/
	ui_Crc1 = Ddbm_Crc_Check((uint8_ddbm *)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo) - sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc));
	ui_Crc2 = Ddbm_Crc_Check((uint8_ddbm *)&st_FsCore[ui_FsCount].gst_DdiInfo, sizeof(st_FsCore[ui_FsCount].gst_DdiInfo) - sizeof(st_FsCore[ui_FsCount].gst_DdiInfo.ui_Crc));
	if(ui_Crc1 != st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc){
		if(ui_Crc2 == st_FsCore[ui_FsCount].gst_DdiInfo.ui_Crc){
			/*更新主DDI*/
			ull_Sector = gst_BootConfig[ui_FsCount].ull_DdiStartSector;
			wrt_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiInfo, ull_Sector, 1);
		}else{
			/*主DDI和备份DDI都不行*/
			st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_LastDbiIdx = -1;
			st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_EndFlag = 0x66AA;
			st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo) - sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc));
		    /*更新主DDI*/
			ull_Sector = gst_BootConfig[ui_FsCount].ull_DdiStartSector;
			wrt_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, ull_Sector, 1);
			/*更新备份DDI*/
			ull_Sector = gst_BootConfig[ui_FsCount].ull_Ddi2StartSector;
			wrt_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, ull_Sector, 1);
		}
	}else if((ui_Crc2 != st_FsCore[ui_FsCount].gst_DdiInfo.ui_Crc)||(ui_Crc2 != ui_Crc1)){
		/*更新备份DDI*/
		ull_Sector = gst_BootConfig[ui_FsCount].ull_Ddi2StartSector;
		wrt_sector(pDev, (unsigned char*)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, ull_Sector, 1);
	}
	/*DDI上电次数*/
	st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ull_FsBootNum++;
	st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo) - sizeof(st_FsCore[ui_FsCount].gst_DdiPowerOnInfo.ui_Crc));
	memcpy(&st_FsCore[ui_FsCount].gst_DdiInfo, &st_FsCore[ui_FsCount].gst_DdiPowerOnInfo, sizeof(STRUCT_DDI));


    //printf("ui_FsCount = %d,st_FsCore[ui_FsCount].ui_MemBlkIndex=%d\n",ui_FsCount,st_FsCore[ui_FsCount].ui_MemBlkIndex);
	/*获取文件系统的DBI扇区*/
    /*申请DBI存储空间序号，存储哈希单元*/
	if(st_FsCore[ui_FsCount].ui_MemBlkIndex < 0){
		st_FsCore[ui_FsCount].ui_MemBlkIndex = Ddbm_MemBlk_Create(gst_BootConfig[ui_FsCount].ui_DbiSum, sizeof(STRUCT_FILE_HASH_ITER)/4+1);
		if(st_FsCore[ui_FsCount].ui_MemBlkIndex < 0){
			PENDING_SEM_GIVE(st_FsCore[ui_FsCount].semPending);
			return -1;
		}
	}
    //printf("st_FsCore[ui_FsCount].ui_MemBlkIndex=%d\n",st_FsCore[ui_FsCount].ui_MemBlkIndex);
	/*建立DBI哈希表,大小为 HASH_SIZE(这里只是构建哈希表的基本框架，具体节点需要在后面增加)*/
	st_FsCore[ui_FsCount].h_DbiTable = sd_hash_new(HASH_SIZE, ui_FsCount, NULL);
	if(!st_FsCore[ui_FsCount].h_DbiTable){
		PENDING_SEM_GIVE(st_FsCore[ui_FsCount].semPending);
		return -1;
	}
    /*遍历DBI，为哈希表添加初始数据*/
	for(index = 0; index < gst_BootConfig[ui_FsCount].ui_DbiSum; index++)
    {
	    /*依次读取DBI信息*/
		ull_Sector = gst_BootConfig[ui_FsCount].ull_DbiStartSector + index;
		rd_sector(pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
		/*如果DBI有效*/
		if(st_DbiInfo.ui_ValidFlag){
            /*获取哈希单元存储空间，存储空间序号采用DBI序号*/
			pst_FileHashIter = (STRUCT_FILE_HASH_ITER*)Ddbm_MemBlk_Alloc(st_FsCore[ui_FsCount].ui_MemBlkIndex);
			/*指针非空*/
			if(pst_FileHashIter){
				/*添加哈希单元*/
				st_HashData.index = st_DbiInfo.ui_DbiIdx;
				st_HashData.status = 0;
				st_HashData.pending = 0;
				sd_hash_add(st_FsCore[ui_FsCount].h_DbiTable, st_DbiInfo.uc_FileName, &st_HashData);
                printf("成功向哈希中添加%s\n",st_DbiInfo.uc_FileName);
			}
		}
	}
	/*BBM区初始化*/
	Ddbm_Init_BbmPartiton(pDev, ui_FsCount);
	PENDING_SEM_GIVE(st_FsCore[ui_FsCount].semPending);
	return 0;
}

/************************************************
**函数名称：Ddbm_Seek_File
**
**功能：更改文件读写指针
**
**输入参数：pFd: 文件句柄；offset：指针偏移量(字节Byte),相对于文件起始位置
**
**输出参数：成功 =文件的DBI Index，失败-1
**
**注释：
**
*/
int32_ddbm Ddbm_Seek_File(STRUCT_FILE *pFd, uint32_ddbm offset)
{

	uint32_ddbm index, ui_offsetBbmNum;
	uint32_ddbm ui_curBbmIdx;
	uint32_ddbm ull_Sector;
	STRUCT_DBI st_DbiInfo;
    //STRUCT_BBM pst_BbmInfo;

	if(pFd->ui_FsNum >= FS_NUM||pFd == NULL||offset>pFd->ull_FileSize) 
		return -1;

	PENDING_SEM_TAKE(st_FsCore[pFd->ui_FsNum].semPending);
	SHARED_SEM_TAKE(st_FsCore[pFd->ui_FsNum].semShared);
    PENDING_SEM_GIVE(st_FsCore[pFd->ui_FsNum].semPending);

    ull_Sector = gst_BootConfig[pFd->ui_FsNum].ull_DbiStartSector + pFd->ui_DbiIdx;
	rd_sector(st_FsCore[pFd->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);

	ui_offsetBbmNum = offset/DBU_SIZE;
	ui_curBbmIdx = st_DbiInfo.ui_FirstBbmIdx;
	for(index=0;index<ui_offsetBbmNum;index++)
	{
		ui_curBbmIdx = Ddbm_Get_NextBbm(st_FsCore[pFd->ui_FsNum].pDev, pFd->ui_FsNum, ui_curBbmIdx);
	}
	
	/*更新文件句柄的当前BBM指针和读写指针位置*/
	pFd->ui_BbmIdx = ui_curBbmIdx;
	pFd->ull_Position = offset;
	
	SHARED_SEM_GIVE(st_FsCore[pFd->ui_FsNum].semShared);
	
	return pFd->ull_Position;	
}

/************************************************
**函数名称：Ddbm_Query_File
**
**功能：根据文件名字查询文件的dbi
**
**输入参数：fs_index：文件系统序号；uc_Name：文件名字；
**
**输出参数：成功 =文件的DBI Index，失败-1
**
**注释：
**
*/
int32_ddbm Ddbm_Query_File(uint32_ddbm fs_index, const int8_ddbm *c_Name)
{
	int32_ddbm i_Ret = -1;
    STRUCT_FILE_HASH_ITER* pst_FileHashIter;
	
	if(fs_index >= FS_NUM||c_Name == NULL) 
		return -1;

	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	SHARED_SEM_TAKE(st_FsCore[fs_index].semShared);
    PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
	/*查询文件列表*/
    pst_FileHashIter = sd_hash_lookup(st_FsCore[fs_index].h_DbiTable, c_Name);
	if(pst_FileHashIter){
		i_Ret = pst_FileHashIter->data.index;
	}
	SHARED_SEM_GIVE(st_FsCore[fs_index].semShared);
	return i_Ret;	
}

/************************************************
**函数名称：Ddbm_Delete_File
**
**功能：删除文件
**
**输入参数：fs_index:文件系统编号    c_Name：注意这里是没有磁盘目录的，就是一个简单的完整的文件名
**
**输出参数：成功 >=0(dbi编号)，失败-1
**
**注释：文件操作
**
*/
extern int32_ddbm Ddbm_Delete_File(uint32_ddbm fs_index, const int8_ddbm *c_Name)
{

	STRUCT_DBI  st_DbiInfo;
	uint64_ddbm ull_Sector;
	STRUCT_FILE_HASH_ITER *pst_FileHashIter;
    
	if(c_Name == NULL || strlen(c_Name) == 0 ){
		return -1;	
	}
	
	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	
	/*查询文件名称，获得DBI信息*/
	//c_Name += index + 1;
	pst_FileHashIter = sd_hash_lookup(st_FsCore[fs_index].h_DbiTable, c_Name);
	/*文件不存在，不能删除*/
	if(!pst_FileHashIter && pst_FileHashIter->data.status == FILE_RDONLY){
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
        printf("文件不存在，不能删除\n");
		return -1;
	}
#if 0
	/*如果剩余空间还有，不能删，返回-1, 否则可以删*/
	if(Ddbm_Spare_Bbm(fs_index) == 0)
	{
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
        printf("DDBM文件系统的剩余空间还有，不能删\n");
		return -1;
	}
#endif
	/*读以上模式(可写，追加或者更新)已经打开过文件。不允许删除*/
	if(pst_FileHashIter && pst_FileHashIter->data.status > FILE_RDONLY)
	{
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
		printf("文件正在写，不可删除!!!\n");
		return -1;
	}
	
	
	ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + pst_FileHashIter->data.index;
	rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	/*清除这个dbi所有的bbm的有效性flag*/
	Ddbm_Del_BbmList(st_FsCore[fs_index].pDev, fs_index, st_DbiInfo.ui_FirstBbmIdx);
	/*删除这个磁盘中的这个文件DBI，更新磁盘*/
	st_DbiInfo.ui_ValidFlag = 0;
	st_DbiInfo.ui_BbmChainSize = 0;
	st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(&st_DbiInfo), sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));	
	wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	

	
	/*删除哈希表中的这个文件所表示的哈希单元*/
	sd_hash_del_unit(st_FsCore[fs_index].h_DbiTable, c_Name, st_FsCore[fs_index].ui_MemBlkIndex);

	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	return st_DbiInfo.ui_DbiIdx;


}


/************************************************
**函数名称：Ddbm_Close_File
**
**功能：关闭文件
**
**输入参数：pFd文件句柄
**
**输出参数：int32_ddbm
**
**注释：文件操作
**
*/
extern int32_ddbm Ddbm_Close_File(STRUCT_FILE *pFd)
{
	uint32_ddbm ull_Sector;
	STRUCT_DBI  st_DbiInfo;
	STRUCT_FILE_HASH_ITER *pst_FileHashIter;

	/*查询文件名称，获得DBI信息*/
	pst_FileHashIter = sd_hash_lookup(st_FsCore[pFd->ui_FsNum].h_DbiTable, pFd->c_FileName);
	/*文件不存在，关闭失败*/
	if(!pst_FileHashIter || NULL == pFd)
	{
		return -1;
	}

	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	
	/*在哈希表中设置文件状态为0，表示未打开状态*/
	pst_FileHashIter->data.status = 0;
	
	/*更新文件大小*/
    ull_Sector = gst_BootConfig[pFd->ui_FsNum].ull_DbiStartSector + pFd->ui_DbiIdx;
	rd_sector(st_FsCore[pFd->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	st_DbiInfo.ull_FileSize = pFd->ull_FileSize;
	st_DbiInfo.ui_CloseTime = Ddbm_Get_Time();
	st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_DbiInfo, sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));
	/*写盘更新DBI(文件大小)*/	
	wrt_sector(st_FsCore[pFd->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	/*写盘更新备份DBI(文件大小)*/
	ull_Sector = gst_BootConfig[pFd->ui_FsNum].ull_Dbi2StartSector + pFd->ui_DbiIdx;
	wrt_sector(st_FsCore[pFd->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	/*释放文件句柄*/
	free(pFd);

	PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
	
	return 0;
}

/************************************************
**函数名称：Ddbm_Open_File
**
**功能：打开文件
**
**输入参数：c_Name：文件名字(带完整路径，也可以不带完整路径)；i_Flag：打开模式；i_Mode:未使用
**
**输出参数：文件指针
**
**注释：文件操作
**
*/
extern void *Ddbm_Open_File(const int8_ddbm *c_DevsName, const int8_ddbm *c_Name, int32_ddbm i_Flag)
{
	STRUCT_FILE *pFd =NULL;
	STRUCT_DBI  st_DbiInfo;
	int32_ddbm i_Ret = 0;
	int32_ddbm index, fs_index;
    //int8_ddbm c_DevsName[FULL_NAME_LEN];
	uint64_ddbm ull_Sector;
	STRUCT_HASH_DATA st_HashData;
	STRUCT_FILE_HASH_ITER *pst_FileHashIter;
    /*test
    printf("[Ddbm_Open_File]规范之前的strlen(c_Name)=%d\n",strlen(c_Name));
    for(int i=0;i<strlen(c_Name)+10;i++)
    {
        printf("%02x\t",c_Name[i]);
    }
    */

	if(c_Name == NULL || strlen(c_Name) == 0 ){
		return NULL;	
	}

	if((i_Flag != FILE_RDONLY) && (i_Flag != FILE_RDWRT) && (i_Flag != FILE_APPEND) && (i_Flag != FILE_UPDATE)){
		return NULL;
	}

	/*zsc del win模拟测试环境下文件名不能包含,如果第一个是/也要过滤掉*/
	if(c_Name[0] == '/')
		c_Name += 1;

    //printf("规范之前的c_Name是%s,index=%d\n",c_Name,strlen(c_Name),index);
    for(index = strlen(c_Name)-1; index >= 0; index--){
        if(c_Name[index] == '/')
            break;
    }
    c_Name += index + 1;
    //printf("规范之后的c_Name是%s,index=%d\n",c_Name,index);
    //if(index >= (strlen(c_Name) -1) || index >= FULL_NAME_LEN || index == 1)    return NULL;

    //memcpy(c_DevsName, c_Name, index);
    //c_DevsName[index] = '\0';

	/*获取设备*/
	PENDING_SEM_TAKE(sem_GlobalFsVar);
	/*查询设备列表*/
	for(fs_index = 0; fs_index < FS_NUM; fs_index++){
		if(!strcmp(st_FsCore[fs_index].c_DevsName, c_DevsName))
			break;
	}
	/*释放全局信号量*/
	PENDING_SEM_GIVE(sem_GlobalFsVar);
	if(fs_index == FS_NUM)
    {
        printf("fs_index=%d\n",fs_index);
        return NULL;
    }


	/*创建文件句柄*/
	pFd = (STRUCT_FILE *)malloc(sizeof(STRUCT_FILE));
	if(!pFd)
	{
		printf("文件句柄分配失败！\n");
		return NULL;
	}
	
	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	
	/*查询文件名称，获得DBI信息*/
	pst_FileHashIter = sd_hash_lookup(st_FsCore[fs_index].h_DbiTable, c_Name);
	/*如果文件已经打开，不能用读以上的方式再次打开。(只读方式打开文件，只能以只读方式再次打开)*/
	if(pst_FileHashIter && pst_FileHashIter->data.status >= FILE_RDONLY && i_Flag > FILE_RDONLY)
	{
        printf("文件已经打开，不能用读以上的方式再次打开！\n");
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
		return NULL;
	}
	/*文件不存在，不能以只读方式打开*/
	if(!pst_FileHashIter && i_Flag == FILE_RDONLY){
        printf("文件不存在，不能以只读方式打开！\n");
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
		return NULL;
	}
	/*文件不存在或者文件可写打开（包括文件不存在且读写、改写、追加方式打开，或者文件存在，以读写方式打开4种情况）
	文件不存在且读写、改写、追加方式打开表示创建文件，文件存在，以读写方式打开表示擦除文件重头开始写的情况*/
	if(!pst_FileHashIter || i_Flag == FILE_RDWRT)
	{
		
		/*一个上电过程内DBI就发生了循环，不允许这样的情况发生，直接返回，否则删除最旧文件，为创建新文件做准备*/
		if(st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx == \
			(st_FsCore[fs_index].gst_DdiInfo.ui_LastDbiIdx -1 + gst_BootConfig[fs_index].ui_DbiSum) % gst_BootConfig[fs_index].ui_DbiSum){
			PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
            printf("一个上电过程内DBI就发生了循环！\n");
			return NULL;
		}

		/*删除最旧文件*/	
		/*如果是创建文件，ui_LastDbiIdx表示找到最旧的那个DBI的idx，并处理它，以及给相应pfd赋值*/
		if(!pst_FileHashIter)
		{
			i_Ret = Ddbm_Delete_LastDbi(fs_index, 0);
		}
		/*如果是文件可写打开并且文件之前还是存在的，Ddbm_Delete_LastDbi函数对ui_LastDbiIdx应该不作处理。*/
		else if(pst_FileHashIter && i_Flag == FILE_RDWRT)
		{
			PENDING_SEM_TAKE(pst_FileHashIter->data.pending);
		    /*如果BBM不在这里释放，这些BBM会永远占着磁盘空间，除非格式化*/
			ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx;
		    rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
			/*清除这个dbi所有的bbm的有效性flag*/
			Ddbm_Del_BbmList(st_FsCore[fs_index].pDev, fs_index, st_DbiInfo.ui_FirstBbmIdx);
		}
		
		
		if(i_Ret < 0){
			PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
            printf("Ddbm_Delete_LastDbi出问题！\n");
			return NULL;
		}
		
	    ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx;
		rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
		strcpy(st_DbiInfo.uc_FileName, c_Name);
		st_DbiInfo.ui_ValidFlag = 1;
		st_DbiInfo.ull_FileSize = 0;
		st_DbiInfo.ui_FirstBbmIdx = 0xFFFFFFFF;
		st_DbiInfo.ui_CreateTime = Ddbm_Get_Time();
		st_DbiInfo.ull_FsBootNum = st_FsCore[fs_index].gst_DdiPowerOnInfo.ull_FsBootNum;
		st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_DbiInfo, sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));
		wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
		ull_Sector = gst_BootConfig[fs_index].ull_Dbi2StartSector + st_FsCore[fs_index].gst_DdiPowerOnInfo.ui_LastDbiIdx;
		wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	}else{
		/*包括了所有文件存在并且文件不是用RDWRT方式打开的情况*/
		/*保护同一个文件被单任务操作*/
		PENDING_SEM_TAKE(pst_FileHashIter->data.pending);
		/*获取该文件的DBI INFO信息并存入局部变量st_DbiInfo*/
		ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + pst_FileHashIter->data.index;
		rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	}

	strcpy(pFd->c_FileName,(const char *)c_Name); 
	pFd->ui_DbiIdx = st_DbiInfo.ui_DbiIdx;
    pFd->ui_FsNum = fs_index;
	pFd->ui_Mode = i_Flag;
	/*四种打开模式分别讨论*/
	if(pFd->ui_Mode == FILE_RDONLY){
		pFd->ui_BbmIdx = st_DbiInfo.ui_FirstBbmIdx;
		pFd->ull_Position = 0;
		pFd->ull_FileSize = st_DbiInfo.ull_FileSize;
	}else if(pFd->ui_Mode == FILE_RDWRT){
		pFd->ui_BbmIdx = st_DbiInfo.ui_FirstBbmIdx;
		pFd->ull_Position = 0;
		pFd->ull_FileSize = 0;
	}else if(pFd->ui_Mode == FILE_APPEND){
	    /*获得尾节点*/
		pFd->ui_BbmIdx = Ddbm_Get_BbmList_Tail(st_FsCore[fs_index].pDev, fs_index, st_DbiInfo.ui_FirstBbmIdx);
		if(!Ddbm_Bbm_Used(fs_index, pFd->ui_BbmIdx)){
			pFd->ull_Position = Ddbm_Read_BbmInfo(st_FsCore[fs_index].pDev, fs_index, pFd->ui_BbmIdx)->ull_WrtPosition;
		}else{
		    pFd->ull_Position = 0;
		}
		pFd->ull_FileSize = st_DbiInfo.ull_FileSize;
	}else if(pFd->ui_Mode == FILE_UPDATE){
		pFd->ui_BbmIdx = st_DbiInfo.ui_FirstBbmIdx;
		pFd->ull_Position = 0;
		pFd->ull_FileSize = st_DbiInfo.ull_FileSize;
	}
	/*将DBI添加到哈希表中*/
	st_HashData.index = st_DbiInfo.ui_DbiIdx;
	/*文件状态采用或的形式*/
	st_HashData.status |= pFd->ui_Mode;
	if((!pst_FileHashIter)||(!pst_FileHashIter->data.pending)){
		PENDING_SEM_CREATE(st_HashData.pending);
	}		
	sd_hash_add(st_FsCore[fs_index].h_DbiTable, st_DbiInfo.uc_FileName, &st_HashData);
	/*创建文件信号量*/
	pFd->sem_Pending = st_HashData.pending;
	
	PENDING_SEM_GIVE(pst_FileHashIter->data.pending);
	PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);	
	
	return pFd;	
}

/************************************************
**函数名称：Ddbm_Read_File
**
**功能：根据文件指针读取文件内容
**
**输入参数：buf：void类型指针；size：每个单元大小；count：单元个数；fp：文件指针
**
**输出参数：成功 >0(读取内容的长度)，失败<=0
**
**注释：
**
*/
extern int32_ddbm Ddbm_Read_File(void * buf, const uint32_ddbm size, const uint32_ddbm count, STRUCT_FILE * const fp)
{
	/*计算需要读取的长度*/
	uint64_ddbm ull_SizeCount = 0;
	/*计算写入的长度*/
	uint32_ddbm ull_Length_Byte = 0;
	/*计算写入的扇区数目*/
	uint32_ddbm ull_Length_Sector = 0;
	/*计算已经读取的长度*/
	uint64_ddbm ull_HasLength = 0;
	/*扇区序号*/
	uint64_ddbm ull_Sector = 0;
	/*缓冲数据*/
	uint8_ddbm c_buff[SECTOR_SIZE];
	/*BBM指针*/
	const STRUCT_BBM * pst_BbmInfo;
	if( !fp || !buf || fp->ui_FsNum >= FS_NUM){
		return (-1);
	}
	/*计算需要读取的长度*/
	ull_SizeCount = (uint64_ddbm)size * count;
	if(!ull_SizeCount)
		return 0;
	
	PENDING_SEM_TAKE(st_FsCore[fp->ui_FsNum].semPending);
	SHARED_SEM_TAKE(st_FsCore[fp->ui_FsNum].semShared);
    PENDING_SEM_GIVE(st_FsCore[fp->ui_FsNum].semPending);
	PENDING_SEM_TAKE(fp->sem_Pending);
	
	if(Ddbm_Bbm_Used(fp->ui_FsNum, fp->ui_BbmIdx))
	{
		PENDING_SEM_GIVE(fp->sem_Pending);
	    SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
		return (-2);
	}
	/*获取当前BBM信息*/
	pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
	/*没有可以读取的数据*/
	if(pst_BbmInfo->ull_WrtPosition <= fp->ull_Position)
	{
		/*当前BBM结束，获取下一个BBM*/
		pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, Ddbm_Get_NextBbm(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, pst_BbmInfo->ui_BbmIdx));
		if(!pst_BbmInfo)
		{
		    PENDING_SEM_GIVE(fp->sem_Pending);
			SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
			return 0;
		}
		/*更新fp文件指针*/
		fp->ui_BbmIdx = pst_BbmInfo->ui_BbmIdx;
		fp->ull_Position = 0;
	}
	/*计算开始读取的扇区*/
	ull_Sector =  pst_BbmInfo->ull_DbuStartSector + fp->ull_Position/SECTOR_SIZE;
	/*判断当前读位置是否是512的整数倍*/
    if(fp->ull_Position % SECTOR_SIZE != 0)
	{
		/*读取一个扇区的数据*/
		rd_sector(st_FsCore[fp->ui_FsNum].pDev, c_buff, ull_Sector, 1);
		/*判断读取的文件长度是否小于该扇区剩余的长度*/
		if(ull_SizeCount < SECTOR_SIZE - fp->ull_Position % SECTOR_SIZE)
		{
			ull_Length_Byte = ull_SizeCount;
		}
		else
		{
			ull_Length_Byte = SECTOR_SIZE - fp->ull_Position % SECTOR_SIZE;
		}
		/*读取的长度是否超出写位置*/
		if(ull_Length_Byte > pst_BbmInfo->ull_WrtPosition - fp->ull_Position){
			ull_Length_Byte = pst_BbmInfo->ull_WrtPosition - fp->ull_Position;
		}
		/*数据搬移*/
		memcpy(buf, c_buff + fp->ull_Position % SECTOR_SIZE, ull_Length_Byte);
		/*更新文件读偏移位置*/
		ull_Sector += 1;
		ull_HasLength += ull_Length_Byte;
		fp->ull_Position += ull_Length_Byte;
		/*是否结束*/
		if(ull_HasLength >= ull_SizeCount){
			PENDING_SEM_GIVE(fp->sem_Pending);
			SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
			return ull_HasLength;
		}
		if(fp->ull_Position >= pst_BbmInfo->ull_WrtPosition && pst_BbmInfo->ull_WrtPosition != gst_BootConfig[fp->ui_FsNum].ui_DbuSize){
			PENDING_SEM_GIVE(fp->sem_Pending);
			SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
			return ull_HasLength;
		}
	}
	/*读取剩下的长度*/
	while((ull_SizeCount - ull_HasLength) > 0)
	{
		/*当前BBM的可读数据计算*/
		if((pst_BbmInfo->ull_WrtPosition - fp->ull_Position) >= (ull_SizeCount - ull_HasLength))
		{
			/*计算读取的扇区数目*/
			ull_Length_Sector = (ull_SizeCount - ull_HasLength)/SECTOR_SIZE;
			rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)buf + ull_HasLength, ull_Sector, ull_Length_Sector);
			/*更新文件读偏移位置*/
			ull_HasLength += ull_Length_Sector * SECTOR_SIZE;
			fp->ull_Position += ull_Length_Sector * SECTOR_SIZE;
			ull_Sector += ull_Length_Sector;
			if((ull_SizeCount - ull_HasLength) > 0)
			{
				/*读取一个扇区的数据*/
				rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)c_buff, ull_Sector, 1);
				ull_Length_Byte = (ull_SizeCount - ull_HasLength);
				memcpy((unsigned char*)buf + ull_HasLength, c_buff, ull_Length_Byte);
				/*更新文件读偏移位置*/
				ull_HasLength += ull_Length_Byte;
				fp->ull_Position += ull_Length_Byte;
			}
			break;
		}
		else
		{
			/*计算读取的扇区数目*/
			ull_Length_Sector = (pst_BbmInfo->ull_WrtPosition - fp->ull_Position)/SECTOR_SIZE;
			rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)buf + ull_HasLength, ull_Sector, ull_Length_Sector);
			/*更新文件读偏移位置*/
			ull_HasLength += ull_Length_Sector * SECTOR_SIZE;
			fp->ull_Position += ull_Length_Sector * SECTOR_SIZE;
			ull_Sector += ull_Length_Sector;
			/*判断是否还有剩余的可读长度，存在的话暂时不应该链下一个BBM*/
			if(pst_BbmInfo->ull_WrtPosition - fp->ull_Position > 0)
			{
				rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)c_buff, ull_Sector, 1);
				ull_Length_Byte = pst_BbmInfo->ull_WrtPosition - fp->ull_Position;
				memcpy((unsigned char*)buf + ull_HasLength, c_buff, ull_Length_Byte);
			    /*更新文件读偏移位置*/
			    ull_HasLength += ull_Length_Byte;
			    fp->ull_Position += ull_Length_Byte;
			}
			/*当前BBM结束，获取下一个BBM*/
			pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, Ddbm_Get_NextBbm(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum,pst_BbmInfo->ui_BbmIdx));
			if(!pst_BbmInfo)
			{
				PENDING_SEM_GIVE(fp->sem_Pending);
				SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
				return ull_HasLength;
			}
			/*更新fp文件指针*/
			fp->ui_BbmIdx = pst_BbmInfo->ui_BbmIdx;
			fp->ull_Position = 0;
			ull_Sector = pst_BbmInfo->ull_DbuStartSector;
		}
	}
	PENDING_SEM_GIVE(fp->sem_Pending);
	SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
	return ull_HasLength;
}

/************************************************
**函数名称：Ddbm_Write_File
**
**功能：将数据写到文件中
**
**输入参数：buf：void类型指针；size：每个单元大小；count：单元个数；fp：文件指针
**
**输出参数：成功 >=0，失败0
**
**注释：
**
*/
extern int32_ddbm Ddbm_Write_File(const void *buf, const uint32_ddbm size, const uint32_ddbm count, STRUCT_FILE * const fp)
{
	/*计算需要写入的总长度*/
	uint64_ddbm ull_SizeCount = 0;
	/*计算写入的长度*/
	uint32_ddbm ull_Length_Byte = 0;
	/*计算写入的扇区数目*/
	uint32_ddbm ull_Length_Sector = 0;
	/*计算已经写入的长度*/
	uint64_ddbm ull_HasLength = 0;
	/*计算起始写入扇区*/
	uint64_ddbm ull_Sector = 0;
	/*缓冲数据*/
	uint8_ddbm c_buff[SECTOR_SIZE];
	/*BBM指针*/
	STRUCT_BBM * pst_BbmInfo;
	STRUCT_DBI st_DbiInfo;
	int32_ddbm i_Ret = 0;
	
	if( !fp || !buf || fp->ui_Mode < FILE_RDWRT || fp->ui_FsNum >= FS_NUM)
		return (-1);

	/*计算需要写入的总长度*/
	ull_SizeCount = (uint64_ddbm)size*count;
	if(!ull_SizeCount)
		return 0;
	
    PENDING_SEM_TAKE(st_FsCore[fp->ui_FsNum].semPending);
	SHARED_SEM_TAKE(st_FsCore[fp->ui_FsNum].semShared);
    PENDING_SEM_GIVE(st_FsCore[fp->ui_FsNum].semPending);
    PENDING_SEM_TAKE(fp->sem_Pending);
    /*校验文件句柄BBM编号有效性*/
	if(Ddbm_Bbm_Used(fp->ui_FsNum, fp->ui_BbmIdx))
	{
	    /*无效，读DBI信息*/
	    ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_DbiStartSector + fp->ui_DbiIdx;
		rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
		/*校验DBI中BBM编号有效性*/
		if(Ddbm_Bbm_Used(fp->ui_FsNum, st_DbiInfo.ui_FirstBbmIdx))
		{
			/*无效，创建簇链*/
			st_DbiInfo.ui_FirstBbmIdx = Ddbm_New_BbmList(fp->ui_FsNum);
			pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, st_DbiInfo.ui_FirstBbmIdx);
			if(!pst_BbmInfo){			
				PENDING_SEM_GIVE(fp->sem_Pending);
				SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
				return (-1);
			}
			pst_BbmInfo->ui_DbiIdx = fp->ui_DbiIdx;
			pst_BbmInfo->ui_NextBbmIdx = 0xFFFFFFFF;
			pst_BbmInfo->ui_PreBbmIdx = 0xFFFFFFF5;
			pst_BbmInfo->ui_UsedFlag = 1;
			pst_BbmInfo->ull_WrtPosition = 0;
			pst_BbmInfo->ull_SequenceNum = 0;
			pst_BbmInfo->ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)pst_BbmInfo, sizeof(STRUCT_BBM) - sizeof(pst_BbmInfo->ui_Crc));
			/*更新DBI*/
			st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_DbiInfo, sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
			ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_Dbi2StartSector + fp->ui_DbiIdx;
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
			/*更新BBM*/
			ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_BbmStartSector + pst_BbmInfo->ui_BbmIdx;
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)pst_BbmInfo, ull_Sector, 1);
			ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_Bbm2StartSector + pst_BbmInfo->ui_BbmIdx;
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)pst_BbmInfo, ull_Sector, 1);
			fp->ui_BbmIdx = pst_BbmInfo->ui_BbmIdx;
		}
		else
		{
			fp->ui_BbmIdx = st_DbiInfo.ui_FirstBbmIdx;
			fp->ull_Position = 0;
		}
		
	}
	/*获取当前BBM信息*/
	pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);

	/*计算开始写入的扇区*/
	ull_Sector =  pst_BbmInfo->ull_DbuStartSector + fp->ull_Position/SECTOR_SIZE;
	/*判断当前读位置是否是512的整数倍*/
	if(fp->ull_Position % SECTOR_SIZE != 0)
	{
		/*读取一个扇区的数据*/
		rd_sector(st_FsCore[fp->ui_FsNum].pDev,c_buff, ull_Sector, 1);
		/*判断读取的文件长度是否小于该扇区剩余的长度*/
		if(ull_SizeCount < SECTOR_SIZE - fp->ull_Position % SECTOR_SIZE)
		{
			ull_Length_Byte = ull_SizeCount;
		}
		else
		{
			ull_Length_Byte = SECTOR_SIZE - fp->ull_Position % SECTOR_SIZE;
		}
		/*数据搬移*/
		memcpy(c_buff + fp->ull_Position % SECTOR_SIZE, buf, ull_Length_Byte);
		wrt_sector(st_FsCore[fp->ui_FsNum].pDev, c_buff, ull_Sector, 1);

		/*更新文件写偏移位置*/
		ull_HasLength += ull_Length_Byte;
		fp->ull_Position += ull_Length_Byte;
		/*更新BBM*//*BBM的写指针大于文件的写指针即为改写，否则为续写*/
		if(pst_BbmInfo->ull_WrtPosition < fp->ull_Position)
		{
			pst_BbmInfo->ull_WrtPosition = fp->ull_Position;
			pst_BbmInfo->ui_Crc = Ddbm_Crc_Check((uint8_ddbm*)pst_BbmInfo, sizeof(STRUCT_BBM) - sizeof(pst_BbmInfo->ui_Crc));
			/*BBM的起始扇区+BBM编号 = 当前BBM扇区*/
			/*BBM写盘*/
			Ddbm_Write_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, pst_BbmInfo->ui_BbmIdx);
		}
		/*是否结束*/
		if(ull_HasLength >= ull_SizeCount){
			PENDING_SEM_GIVE(fp->sem_Pending);
			SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
			fp->ull_FileSize += ull_HasLength;
			return ull_HasLength;
		}
	}

	/*写入剩下的长度*/
	while((ull_SizeCount - ull_HasLength) > 0)
	{
		/*当前BBM的可写数据计算*/
		/*当前可用DBU足够写的下需要写入的数据*/
		if((gst_BootConfig[fp->ui_FsNum].ui_DbuSize - fp->ull_Position) >= (ull_SizeCount - ull_HasLength))
		{
			/*计算写入的扇区数目*/
			ull_Length_Sector = (ull_SizeCount - ull_HasLength)/SECTOR_SIZE;
			ull_Sector = pst_BbmInfo->ull_DbuStartSector + fp->ull_Position/SECTOR_SIZE;
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)buf + ull_HasLength, ull_Sector, ull_Length_Sector);
			/*更新文件写偏移位置*/
			ull_HasLength += ull_Length_Sector * SECTOR_SIZE;
			fp->ull_Position += ull_Length_Sector * SECTOR_SIZE;
            /*计数剩余写入长度*/
			if((ull_SizeCount - ull_HasLength) > 0)
			{
				/*写一个扇区的数据*/
				ull_Sector = pst_BbmInfo->ull_DbuStartSector + fp->ull_Position/SECTOR_SIZE;
				ull_Length_Byte = (ull_SizeCount - ull_HasLength) % SECTOR_SIZE;
				memcpy(c_buff, (const char*)buf + ull_HasLength, ull_Length_Byte);
				wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)c_buff, ull_Sector, 1);
				/*更新文件读偏移位置*/
				ull_HasLength += ull_Length_Byte;
				fp->ull_Position += ull_Length_Byte;

			}
			/*更新BBM*/
			if(pst_BbmInfo->ull_WrtPosition < fp->ull_Position)
			{
				pst_BbmInfo->ull_WrtPosition = fp->ull_Position;
				pst_BbmInfo->ui_Crc = Ddbm_Crc_Check((uint8_ddbm*)pst_BbmInfo, sizeof(STRUCT_BBM)-sizeof(pst_BbmInfo->ui_Crc));
				/*BBM写盘*/
			    Ddbm_Write_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, pst_BbmInfo->ui_BbmIdx);
			}
			break;
		}
		else/*当前剩余DBU空间暂时不够用*/
		{
			/*计算写入的扇区数目*/
			ull_Length_Sector = (gst_BootConfig[fp->ui_FsNum].ui_DbuSize - fp->ull_Position)/SECTOR_SIZE;
			ull_Sector = pst_BbmInfo->ull_DbuStartSector + fp->ull_Position/SECTOR_SIZE;
			wrt_sector(st_FsCore[fp->ui_FsNum].pDev,(unsigned char*)buf + ull_HasLength, ull_Sector, ull_Length_Sector);
			/*更新文件写偏移位置*/
			ull_HasLength += ull_Length_Sector * SECTOR_SIZE;
			fp->ull_Position += ull_Length_Sector * SECTOR_SIZE;

			/*更新BBM(主要是更新读写指针和CRC)*//*BBM的写指针小于文件的写指针即为改写，否则为续写*/
			if(pst_BbmInfo->ull_WrtPosition < fp->ull_Position)
			{
				pst_BbmInfo->ull_WrtPosition = fp->ull_Position;
				pst_BbmInfo->ui_Crc = Ddbm_Crc_Check((uint8_ddbm*)pst_BbmInfo, sizeof(STRUCT_BBM)-sizeof(pst_BbmInfo->ui_Crc));
				/*BBM写盘*/
				Ddbm_Write_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, pst_BbmInfo->ui_BbmIdx);
			}

			/*当前BBM结束，读取下一个BBM(Ddbm_Get_NextBbm)的信息没成功，获取到是因为改写*/
			pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, Ddbm_Get_NextBbm(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx));
			if(!pst_BbmInfo)
			{
				/*申请下一个可用BBM*/
				i_Ret = Ddbm_Append_BbmList(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
				if(i_Ret == 0)
				{
					fp->ui_BbmIdx = Ddbm_Get_NextBbm(st_FsCore[fp->ui_FsNum].pDev,fp->ui_FsNum, fp->ui_BbmIdx);
					fp->ull_Position = 0;
					pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
				}
				else
				{
					/*申请不到，删除文件，释放BBM*/
					i_Ret = Ddbm_Delete_LastDbi(fp->ui_FsNum, 1);
					if(i_Ret < 0){
						PENDING_SEM_GIVE(fp->sem_Pending);
			            SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
						fp->ull_FileSize = ull_HasLength;
						return ull_HasLength;
					}
					/*跑到这里，说明Ddbm_Delete_LastDbi成功，再次通过Ddbm_Append_BbmList获取刚刚释放出来的BBM的idx，更新文件句柄fp，并更新当前写函数的BBM句柄pst_BbmInfo*/
					i_Ret = Ddbm_Append_BbmList(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
					fp->ui_BbmIdx = Ddbm_Get_NextBbm(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
					fp->ull_Position = 0;
					pst_BbmInfo = Ddbm_Read_BbmInfo(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, fp->ui_BbmIdx);
					
					
				}

			}
			else /*当前BBM中，下一个BBM读的到，直接更新文件指针*/
			{
				/*更新fp文件指针*/
				fp->ui_BbmIdx = pst_BbmInfo->ui_BbmIdx;
				fp->ull_Position = 0;
			}
		}
	}
	fp->ull_FileSize += ull_HasLength;

	/*更新DBI*/
    ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_DbiStartSector + fp->ui_DbiIdx;
	rd_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	//if(Ddbm_Get_BbmList_Size(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, st_DbiInfo.ui_FirstBbmIdx) == (ull_HasLength/(((MEM_BLK_DEV *)(st_FsCore[fp->ui_FsNum].pDev))->dev_info.dbu_size)  ))
	st_DbiInfo.ui_BbmChainSize = Ddbm_Get_BbmChain_Size(st_FsCore[fp->ui_FsNum].pDev, fp->ui_FsNum, st_DbiInfo.ui_FirstBbmIdx);
	st_DbiInfo.ui_ModifyTime = Ddbm_Get_Time();
	st_DbiInfo.ull_FileSize = fp->ull_FileSize;
	st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_DbiInfo, sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));
	wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	ull_Sector = gst_BootConfig[fp->ui_FsNum].ull_Dbi2StartSector + fp->ui_DbiIdx;
	wrt_sector(st_FsCore[fp->ui_FsNum].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);

	PENDING_SEM_GIVE(fp->sem_Pending);
	SHARED_SEM_GIVE(st_FsCore[fp->ui_FsNum].semShared);
	return ull_HasLength;
}

/************************************************
**函数名称：Ddbm_File_Info
**
**功能：获取该文件所有BBM信息
**
**输入参数：文件句柄
**
**输出参数：uint32_ddbm **bbmIndexChain(这个参数申请的空间需要用户在外界释放),当前文件的BBM编号信息 uint32_ddbm *bbmNum,当前文件的BBM数量  
**
**返回值：成功bbmChainSize，失败-1
**
**注释：修改，输入参数不再使用pFd  Ddbm_GetFileSystem_Index(const int8_ddbm * pc_devsName)
**
*/
extern int32_ddbm Ddbm_File_BbmInfo(const int8_ddbm * pc_devsName, const int8_ddbm *c_Name, uint32_ddbm **bbmChain)
{

	uint32_ddbm ui_nextBbmIdx,ui_lastBbmIdx;
	uint32_ddbm ull_Sector;
	uint32_ddbm ui_bbmIdx;
	uint32_ddbm bbmChainSize;
	STRUCT_DBI st_DbiInfo;
    int32_ddbm ui_FsCount = -1;
    STRUCT_FILE_HASH_ITER *pst_FileHashIter;
    uint32_ddbm ui_DbiIdx;

    if(pc_devsName == NULL||c_Name == NULL)
        return -1;

    ui_FsCount = Ddbm_GetFileSystem_Index(pc_devsName);

    if(ui_FsCount >= FS_NUM || ui_FsCount < 0)
		return -1;

    PENDING_SEM_TAKE(st_FsCore[ui_FsCount].semPending);
    SHARED_SEM_TAKE(st_FsCore[ui_FsCount].semShared);
    PENDING_SEM_GIVE(st_FsCore[ui_FsCount].semPending);

    /*查询文件名称，获得DBI信息*/
    pst_FileHashIter = sd_hash_lookup(st_FsCore[ui_FsCount].h_DbiTable, c_Name);

    ui_DbiIdx = pst_FileHashIter->data.index;
    ull_Sector = gst_BootConfig[ui_FsCount].ull_DbiStartSector + ui_DbiIdx;
    rd_sector(st_FsCore[ui_FsCount].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
    bbmChainSize = Ddbm_Get_BbmChain_Size(st_FsCore[ui_FsCount].pDev, ui_FsCount, st_DbiInfo.ui_FirstBbmIdx);

	*bbmChain = (uint32_ddbm *)calloc(bbmChainSize,sizeof(uint32_ddbm));
	
    ui_lastBbmIdx = Ddbm_Get_BbmList_Tail(st_FsCore[ui_FsCount].pDev, ui_FsCount, st_DbiInfo.ui_FirstBbmIdx);

	ui_bbmIdx = 0;
    (*bbmChain)[ui_bbmIdx] = st_DbiInfo.ui_FirstBbmIdx;

    ui_nextBbmIdx = st_DbiInfo.ui_FirstBbmIdx;   //给首个bbm赋值
	while(ui_lastBbmIdx != ui_nextBbmIdx)
	{
        ui_nextBbmIdx = Ddbm_Get_NextBbm(st_FsCore[ui_FsCount].pDev, ui_FsCount, ui_nextBbmIdx);
		ui_bbmIdx++;
		(*bbmChain)[ui_bbmIdx] = ui_nextBbmIdx;
		
	}

	if(bbmChainSize != ui_bbmIdx+1)
	{
		
		return -1;
        SHARED_SEM_GIVE(st_FsCore[ui_FsCount].semShared);
	}
	
	
    return bbmChainSize;
    SHARED_SEM_GIVE(st_FsCore[ui_FsCount].semShared);
}


/************************************************
**函数名称：Ddbm_Rename_File
**
**功能：重命名文件
**
**输入参数：const char * oldname, const char * newname,两个参数都必须是完整的路径  
**
**输出参数：成功0，失败-1
**
**注释：
**
*/
extern int32_ddbm Ddbm_Rename_File(const int8_ddbm *oldname, const int8_ddbm *newname)
{
	
	STRUCT_DBI  st_DbiInfo;
    uint32_ddbm index, fs_index;
	int8_ddbm c_OldDevsName[FULL_NAME_LEN];
	int8_ddbm c_NewDevsName[FULL_NAME_LEN];
	uint64_ddbm ull_Sector;
	
	STRUCT_FILE_HASH_ITER *pst_FileHashIter;
    
	
	if(oldname == NULL || strlen(oldname) == 0 || newname == NULL || strlen(newname) == 0 ){
		return -1;	
	}


	/*获取设备名*/
	if(oldname[0] == '/')
		oldname += 1;
	if(newname[0] == '/')
		newname += 1;

	for(index = 0; index < strlen(oldname); index++){
		if(oldname[index] == '/')
			break;
	}
	if(index >= (strlen(oldname) -1) || index >= FULL_NAME_LEN || index == 1)
		return NULL;
	for(index = 0; index < strlen(newname); index++){
		if(newname[index] == '/')
			break;
	}
	if(index >= (strlen(newname) -1) || index >= FULL_NAME_LEN || index == 1)
		return -1;
	
	memcpy(c_OldDevsName, oldname, index);
	c_OldDevsName[index] = '\0';
	memcpy(c_NewDevsName, newname, index);
	c_NewDevsName[index] = '\0';
	/*两个文件不属于同一设备直接返回*/
	if(strcmp(c_OldDevsName,c_NewDevsName) != 0)  return-1;
	/*获取设备*/
	PENDING_SEM_TAKE(sem_GlobalFsVar);
	/*查询设备列表*/
	for(fs_index = 0; fs_index < FS_NUM; fs_index++){
		if(!strcmp(st_FsCore[fs_index].c_DevsName, c_OldDevsName))
			break;
	}
	if(fs_index == FS_NUM)
		return -1;
	/*释放全局信号量*/
	PENDING_SEM_GIVE(sem_GlobalFsVar);
	/*获取文件名*/
	for(index = strlen(oldname)-1; index >= 0; index--){
		if(oldname[index] == '/')
			break;
	}
	oldname += index + 1;
	for(index = strlen(newname)-1; index >= 0; index--){
		if(newname[index] == '/')
			break;
	}
	newname += index + 1;
	
	PENDING_SEM_TAKE(st_FsCore[fs_index].semPending);
	/*查询旧文件名称，获得DBI信息*/
	pst_FileHashIter = sd_hash_lookup(st_FsCore[fs_index].h_DbiTable, oldname);
	/*如果文件已经写方式打开，不能重命名*/
	if(pst_FileHashIter && pst_FileHashIter->data.status > FILE_RDONLY)
	{
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
		return -1;
	}
	/*文件不存在，不能重命名*/
	if(!pst_FileHashIter){
		PENDING_SEM_GIVE(st_FsCore[fs_index].semPending);
		return -1;
	}

	strcpy(pst_FileHashIter->key, newname);

    ull_Sector = gst_BootConfig[fs_index].ull_DbiStartSector + pst_FileHashIter->data.index;
	rd_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	strcpy(st_DbiInfo.uc_FileName, newname);
	st_DbiInfo.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)&st_DbiInfo, sizeof(st_DbiInfo) - sizeof(st_DbiInfo.ui_Crc));
	wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
	ull_Sector = gst_BootConfig[fs_index].ull_Dbi2StartSector + pst_FileHashIter->data.index;
	wrt_sector(st_FsCore[fs_index].pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);

	return 0;
}


/************************************************
**函数名称：Ddbm_Error_Get
**
**功能：获取文件系统的错误代码
**
**输入参数：无
**
**输出参数：错误代码
**
**注释：
**
*/
extern int32_ddbm Ddbm_Error_Get()
{
	return ddbm_errno;
}

/************************************************
**函数名称：Ddbm_Get_Time
**
**功能：文件系统的获取时间
**
**输入参数：无
**
**输出参数：uint64_ddbm大小的格式的时间数据
**
**注释：返回值前三个字节存储年，后五个字节分别是月日时分秒
**
*/
extern uint64_ddbm Ddbm_Get_Time()
{
	uint64_ddbm ull_DdbmTime = 0;
	
	ull_DdbmTime |= ddbm_time.year << 40;
	ull_DdbmTime |=	ddbm_time.month << 32;
	ull_DdbmTime |=	ddbm_time.day << 24;
	ull_DdbmTime |=	ddbm_time.hour << 16;
	ull_DdbmTime |=	ddbm_time.minute <<8;
	ull_DdbmTime |=	ddbm_time.second;
		
	return ull_DdbmTime;
}

/************************************************
**函数名称：Ddbm_Update_Time
**
**功能：供系统调用，更新DDBM文件系统的时间
**
**输入参数：年月日时分秒
**
**输出参数：错误代码
**
**注释：
**
*/
extern int32_ddbm Ddbm_Update_Time(uint16_ddbm year,uint8_ddbm mounth,uint8_ddbm day,uint8_ddbm hour,uint8_ddbm minute,uint8_ddbm second)
{
	if( (year>1990&&year<2999) && (mounth>=1&&mounth<=12) && (day>=1&&day<=31)
		 && (hour>=0&&hour<=23) && (minute>0&&minute<=59) && (second>0&&second<=59) )
	{
		ddbm_time.year = year;
		ddbm_time.month = mounth;
		ddbm_time.day = day;
		ddbm_time.hour = hour;
		ddbm_time.minute = minute;
		ddbm_time.second = second;
		return 0;
	}
	else
		return -1;
	
}

/************************************************
**函数名称：Ddbm_GetFileSystem_Index
**
**功能：得到文件系统的编号
**
**输入参数：pc_devsName：盘符
**
**输出参数：成功 =文件系统编号(从0开始计算)，失败-1
**
**注释：文件系统操作
**
*/
extern int32_ddbm Ddbm_GetFileSystem_Index(const int8_ddbm * pc_devsName)
{
    MEM_BLK_DEV *pDev;
    int32_ddbm index = 0;
    uint32_ddbm ui_FsCount;

    /*创建全局信号量，保护所有文件系统名称访问*/
    if(!sem_GlobalFsVar){
        PENDING_SEM_CREATE(sem_GlobalFsVar);
    }
    /*参数判断*/
    if((!pc_devsName)||(strlen(pc_devsName) == 0)||(strlen(pc_devsName) > 32))
        return -1;
    /*获取全局信号量*/
    PENDING_SEM_TAKE(sem_GlobalFsVar);
    /*设备列表查询设备*/
    for(index = 0; index < FS_NUM; index++)
    {
        /*如果相等，则跳出循环，表示查询到已有设备，对当前设备进行操作*/
        if(!strcmp(pc_devsName, st_FsCore[index].c_DevsName))
            break;
    }
    if(FS_NUM == index){
        /*没有查询到*/
        /*是否超出文件系统数量上界*/
        if(gui_DevsCount >= FS_NUM){
            PENDING_SEM_GIVE(sem_GlobalFsVar);
            return -1;
        }
        /*获取设备指针,不存在不进行文件系统初始化，不占用文件系统上限,zsc:参数改成pc_devsName，原来是st_FsCore[ui_FsCount].c_DevsName*/
        pDev = (MEM_BLK_DEV *)getMemBlkDev((const uint8_ddbm*)pc_devsName);
        if(!pDev){
            PENDING_SEM_GIVE(sem_GlobalFsVar);
            return -1;
        }
        ui_FsCount = gui_DevsCount;
        memset(&st_FsCore[ui_FsCount], 0x00, sizeof(st_FsCore[ui_FsCount]));
        /*创建文件系统信号量*/
        PENDING_SEM_CREATE(st_FsCore[ui_FsCount].semPending);
        SHARED_SEM_CREATE(st_FsCore[ui_FsCount].semShared);
        /*记录设备名字*/
        memcpy(st_FsCore[ui_FsCount].c_DevsName, pc_devsName, strlen(pc_devsName));
        /*文件系统计数累加*/
        gui_DevsCount++;
    }else{
        /*查询到了,同样需要获取设备句柄*/
        pDev = (MEM_BLK_DEV*)getMemBlkDev((const uint8_ddbm*)pc_devsName);
        if(!pDev){
            PENDING_SEM_GIVE(sem_GlobalFsVar);
            return -1;
        }
        ui_FsCount = index;
    }
    PENDING_SEM_GIVE(sem_GlobalFsVar);

    return ui_FsCount;
}

/************************************************
**函数名称：Ddbm_TraverseFiles
**
**功能：遍历当前块设备的文件系统中的所有文件
**
**输入参数：pc_devsName：盘符 ,uint32_ddbm **dbiChain(这个参数申请的空间需要用户在外界释放)
**
**输出参数：成功 = 有效DBI文件的数量(从0开始计算)，失败-1
**
**注释：文件系统操作
**
*/
uint32_ddbm Ddbm_TraverseFiles(const int8_ddbm *pc_devsName, uint32_ddbm **dbiChain)
{
    MEM_BLK_DEV *pDev;
    uint32_ddbm index = 0;
    uint32_ddbm fileSum = 0;
    int32_ddbm ui_FsCount;
    uint64_ddbm ull_Sector;
    uint32_ddbm ui_DbiIdx = 0;
    STRUCT_DBI st_DbiInfo;

    /*创建全局信号量，保护所有文件系统名称访问*/
    if(!sem_GlobalFsVar){
        PENDING_SEM_CREATE(sem_GlobalFsVar);
    }

    ui_FsCount = Ddbm_GetFileSystem_Index(pc_devsName);
    if(ui_FsCount == -1) return -1;

    /*获取设备指针,不存在不进行文件系统初始化，不占用文件系统上限,zsc:参数改成pc_devsName，原来是st_FsCore[ui_FsCount].c_DevsName*/
    pDev = (MEM_BLK_DEV *)getMemBlkDev((const uint8_ddbm*)pc_devsName);
    if(!pDev){
        PENDING_SEM_GIVE(sem_GlobalFsVar);
        return -1;
    }
    /*实现的策略是每一次都去遍历一遍。可以遍历HASH表，也可以去直接遍历DBI。但是因为不管怎样都是遍历的方式，还不如直接遍历。*/
    /*先求总数*/
    for(index = 0; index < gst_BootConfig[ui_FsCount].ui_DbiSum; index++)
    {
        /*依次读取DBI信息*/
        ull_Sector = gst_BootConfig[ui_FsCount].ull_DbiStartSector + index;
        rd_sector(pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
        /*如果DBI有效*/
        if(st_DbiInfo.ui_ValidFlag){
            fileSum++;
        }
    }
    *dbiChain = (uint32_ddbm *)calloc(fileSum,sizeof(uint32_ddbm));
    /*再分别赋值*/
    for(index = 0; index < gst_BootConfig[ui_FsCount].ui_DbiSum; index++)
    {
        /*依次读取DBI信息*/
        ull_Sector = gst_BootConfig[ui_FsCount].ull_DbiStartSector + index;
        rd_sector(pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
        /*如果DBI有效*/
        if(st_DbiInfo.ui_ValidFlag){

            (*dbiChain)[ui_DbiIdx] = st_DbiInfo.ui_DbiIdx;
            ui_DbiIdx++;
        }
    }

    return fileSum;



}

/************************************************
**函数名称：Ddbm_getFileAttr
**
**功能：根据DBI值求文件基本属性信息
**
**输入参数：pc_devsName：盘符 ,uint32_ddbm dbiIdx
**
**输出参数：成功 = filename，失败null
**
**注释：文件系统操作
**
*/
STRUCT_ATTR *Ddbm_getFileAttr(const int8_ddbm *pc_devsName, int32_ddbm dbiIdx)
{
    MEM_BLK_DEV *pDev;
    int32_ddbm ui_FsCount;
    uint64_ddbm ull_Sector;
    STRUCT_DBI st_DbiInfo;
    STRUCT_ATTR *st_fileAttr;
    //int32_ddbm dbiIdx;

    ui_FsCount = Ddbm_GetFileSystem_Index(pc_devsName);
    //printf("[Ddbm_DBI2Name]当前ui_FsCount = %d\n",ui_FsCount);
    if(ui_FsCount>=FS_NUM || ui_FsCount<0)  return NULL;
/*
    dbiIdx = Ddbm_Query_File(ui_FsCount, c_Name);
    if(dbiIdx == -1)
    {
        printf("Ddbm_Query_File->dbi err\n");
        return NULL;
    }
*/
    /*获取设备指针,不存在不进行文件系统初始化，不占用文件系统上限,zsc:参数改成pc_devsName，原来是st_FsCore[ui_FsCount].c_DevsName*/
    pDev = (MEM_BLK_DEV *)getMemBlkDev((const uint8_ddbm*)pc_devsName);
    if(!pDev){
        //printf("[Ddbm_DBI2Name]找不到设备\n");
        return NULL;
    }

    /*依次读取DBI信息*/
    ull_Sector = gst_BootConfig[ui_FsCount].ull_DbiStartSector + dbiIdx;
    rd_sector(pDev, (unsigned char*)&st_DbiInfo, ull_Sector, 1);
    /*如果DBI有效*/
    if(st_DbiInfo.ui_ValidFlag){
        st_fileAttr = (STRUCT_ATTR *)malloc(sizeof(STRUCT_ATTR));  //返回字符串，一定要返回堆中申请的
        if(dbiIdx != (int32_ddbm)st_DbiInfo.ui_DbiIdx)    printf("get dbi err\n");
        st_fileAttr->ui_DbiIdx = st_DbiInfo.ui_DbiIdx;               /*DBI索引号*/
        //if(strcmp(st_fileAttr->uc_FileName, c_Name))    printf("get filename err,不等!\n");
        strcpy(st_fileAttr->uc_FileName, st_DbiInfo.uc_FileName);			/*文件名称*/
        st_fileAttr->ui_FirstBbmIdx = st_DbiInfo.ui_FirstBbmIdx;			/*分配的起始BBM编号*/
        st_fileAttr->ui_BbmChainSize = st_DbiInfo.ui_BbmChainSize;			/*分配的BBM链大小*/
        //st_fileAttr->ui_CreateTime = ;			/*创建时间*/
        //st_fileAttr->ui_ModifyTime = ;			/*最近一次修改时间*/
        //st_fileAttr->ui_CloseTime = ;				/*最近一次关闭时间*/
        st_fileAttr->ull_FileSize = st_DbiInfo.ull_FileSize;				/*文件大小*/
        return st_fileAttr;
    }
    else
    {
        printf("[Ddbm_DBI2Name]当前文件名是无效的\n");
        return NULL;
    }


}


/************************************************
**函数名称：Ddbm_Format
**
**功能：文件系统格式化
**
**输入参数：device：磁盘名字或者指向磁盘的指针
**
**输出参数：成功 0，失败-1
**
**注释：字符串指向为常值
**
*/
int32_ddbm Ddbm_Format(const uint8_ddbm* disk_name, BLK_DEV_INFO dev_info)
{
    uint32_ddbm ret = 0;
    uint32_ddbm i = 0;
    uint32_ddbm bbm_cnt = 0;
    uint32_ddbm dbu_cnt = 0;
    uint32_ddbm bbm_size = 0; /*bbm_size总数*/
    uint32_ddbm disk_sector_num = 0;/*当前可用于格式化的磁盘的所有的扇区数量（不能大于磁盘的总容量）*/

    MEM_BLK_DEV *pDev = NULL;
    STRUCT_BOOTSECTOR st_BootSector;
    STRUCT_DDI        st_Ddi;
    STRUCT_DBI        st_Dbi;
    STRUCT_BBM        st_Bbm;
    /*直接在内存中申请一个BBM空间,未测试*/
    STRUCT_BBM *bbm_data_head;
    STRUCT_DBI *dbi_data_head;


    printf("start disk format,disk name = %s!\n",disk_name);
    /*前32个扇区保留，其中第一个扇区是MBR。BOOTSECTOR扇区从第33个扇区开始算（编号是32）*/

    pDev = getMemBlkDev(disk_name);
    memcpy(&(pDev->dev_info), &dev_info, sizeof(dev_info));   //需要在这里复制一下

    if(pDev == NULL)   return -1;
    disk_sector_num = (pDev->dev_info.disk_size)/SECTOR_SIZE;
#if 0
    /*向整个磁盘的第一个物理扇区写MBR信息*/
    len = wrt_sector(mbr_sector, 0, 1);
    if(!len)  printf("写MBR扇区失败! len = %d!\n", len);
#endif


    /*开始处理磁盘的BOOT,DDI,DBI和BBM区域*/
    memset((uint8_ddbm *)(&st_BootSector), 0, sizeof(STRUCT_BOOTSECTOR));
    memset((uint8_ddbm *)(&st_Ddi), 0, sizeof(STRUCT_DDI));
    memset((uint8_ddbm *)(&st_Dbi), 0, sizeof(STRUCT_DBI));
    memset((uint8_ddbm *)(&st_Bbm), 0, sizeof(STRUCT_BBM));
    /*文件系统名称设置*/
    memcpy(st_BootSector.uc_FsName,"DDBM FS VER1.0\0",strlen("DDBM FS VER1.0\0"));
    //memcpy(st_BootSector.uc_DiskName, disk_name, sizeof(disk_name));    //计算sizeof(diskname)的结果应该是不对的，只是计算的指针占用空间的大小
    memcpy(st_BootSector.uc_DiskName, disk_name, strlen((int8_ddbm*)disk_name)+1);

    /*根据磁盘的信息计算理论上的最大BBM的数量*/
    bbm_cnt = (disk_sector_num - 2*(pDev->dev_info.dbi_start_sector+pDev->dev_info.dbi_max_num))/(pDev->dev_info.one_dbu_sector_num+2*pDev->dev_info.one_bbm_sector_num);
    /*如果bbm总数不是2的倍数，则减少一个*/
    if(bbm_cnt%2) bbm_cnt--;
    printf("bbm_cnt total num is %u\n",bbm_cnt);
    if((0 == bbm_cnt) || (bbm_cnt > (disk_sector_num)/(pDev->dev_info.one_dbu_sector_num)))
    {
        //printf("计算出了非法的BBM个数，个数bbm_cnt = %lu\n", bbm_cnt);
        printf("cal illegal bbm_cnt = %u\n", bbm_cnt);
        return -1;
    }
    else
    {
        dbu_cnt = bbm_cnt;
        bbm_size = bbm_cnt*SECTOR_SIZE;

        //printf("计算出的BBM个数,bbm_cnt = %lu, BBM数据区大小，bbm_size =%lu\n", bbm_cnt, bbm_size);
        //printf("计算出的DBU个数,dbu_cnt = %lu, \n", dbu_cnt);
        printf("bbm_cnt = %u, bbm_size =%u\n", bbm_cnt, bbm_size);
        printf("dbu_cnt = %u\n", dbu_cnt);
    }
    /*BootSector的处理（数据区起始和结束扇区号用ull64位表示，是因为磁盘容量太大了之后，32位表示不下太大的扇区号）*/
    st_BootSector.ui_BbmSum            = bbm_cnt;
    st_BootSector.ui_DbiSum            = pDev->dev_info.dbi_max_num;
    st_BootSector.ui_DbuSize           = pDev->dev_info.dbu_size;
    st_BootSector.ull_DdiStartSector   = pDev->dev_info.boot_start_sector + 1;                   /*扇区号从1开始编号*/
    st_BootSector.ull_DdiEndSector     = st_BootSector.ull_DdiStartSector + 1;
    st_BootSector.ull_DbiStartSector   = pDev->dev_info.dbi_start_sector;
    st_BootSector.ull_DbiEndSector     = st_BootSector.ull_DbiStartSector + st_BootSector.ui_DbiSum;
    st_BootSector.ull_BbmStartSector   = st_BootSector.ull_DbiEndSector;
    st_BootSector.ull_BbmEndSector     = st_BootSector.ull_BbmStartSector + st_BootSector.ui_BbmSum;
    st_BootSector.ull_DbuStartSector   = st_BootSector.ull_BbmEndSector;
    st_BootSector.ull_DbuEndSector     = st_BootSector.ull_DbuStartSector + dbu_cnt*(pDev->dev_info.one_dbu_sector_num);
    st_BootSector.ull_Bbm2StartSector  = st_BootSector.ull_DbuEndSector;
    st_BootSector.ull_Bbm2EndSector    = st_BootSector.ull_Bbm2StartSector + st_BootSector.ui_BbmSum;
    st_BootSector.ull_Dbi2StartSector  = st_BootSector.ull_Bbm2EndSector;
    st_BootSector.ull_Dbi2EndSector    = st_BootSector.ull_Dbi2StartSector + st_BootSector.ui_DbiSum;
    st_BootSector.ull_Ddi2StartSector  = st_BootSector.ull_Dbi2EndSector;
    st_BootSector.ull_Ddi2EndSector    = st_BootSector.ull_Ddi2StartSector + 1;

    st_BootSector.ui_BbmMethod = 0;
    st_BootSector.ui_FsWrtPermission = 1;
    memset(st_BootSector.uc_Reserved, 0, sizeof(st_BootSector.uc_Reserved));
    st_BootSector.ui_EndFlag = 0x55aa;
    st_BootSector.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(&st_BootSector), sizeof(st_BootSector) - sizeof(st_BootSector.ui_Crc));
    /*向磁盘中写BootSector信息和备份BootSector信息*/
    ret = wrt_sector(pDev,(uint8_ddbm *)(&st_BootSector), pDev->dev_info.boot_start_sector, 1);
    if(DISKIO_OK == ret)
        printf("write BootSector succeed！\n");
    else
        printf("write BootSector failed！\n");

    printf("-----------------partition info-------------------\n");
    /*
    printf("#文件系统名称 ：%s\n", st_BootSector.uc_FsName);
    printf("#磁盘序列号：%s; 磁盘名称：%s;\n", st_BootSector.uc_DiskSerial, st_BootSector.uc_DiskName);
    printf("#BBM总数：%d; DBI总数：%d; DBU大小：%d \n", st_BootSector.ui_BbmSum, st_BootSector.ui_DbiSum,st_BootSector.ui_DbuSize);
    printf("以下扇区号为磁盘的逻辑扇区号\n");
    printf("#DDI起始扇区号：%llu, DDI结束扇区号：%llu \n", st_BootSector.ull_DdiStartSector,st_BootSector.ull_DdiEndSector);
    printf("#DBI起始扇区号：%llu, DBI结束扇区号：%llu \n", st_BootSector.ull_DbiStartSector,st_BootSector.ull_DbiEndSector);
    printf("#BBM起始扇区号：%llu, BBM结束扇区号：%llu \n", st_BootSector.ull_BbmStartSector,st_BootSector.ull_BbmEndSector);
    printf("#DBU起始扇区号：%llu, DBU结束扇区号：%llu \n", st_BootSector.ull_DbuStartSector,st_BootSector.ull_DbuEndSector);
    printf("#备份BBM2起始扇区号：%llu, 备份BBM2结束扇区号：%llu \n", st_BootSector.ull_Bbm2StartSector,st_BootSector.ull_Bbm2EndSector);
    printf("#备份DBI2起始扇区号：%llu, 备份DBI2结束扇区号：%llu \n", st_BootSector.ull_Dbi2StartSector,st_BootSector.ull_Dbi2EndSector);
    printf("#备份DDI2起始扇区号：%llu, 备份DDI2结束扇区号：%llu \n", st_BootSector.ull_Ddi2StartSector,st_BootSector.ull_Ddi2EndSector);
    printf("----------------------------------------------\n");
    printf("\n#开始写入初始信息..................\n");
    */
    printf("#file system name ：%s\n", st_BootSector.uc_FsName);
    printf("#disk serial：%s; disk name：%s;\n", st_BootSector.uc_DiskSerial, st_BootSector.uc_DiskName);
    printf("#BBM num：%d; DBI num：%d; DBU size：%d \n", st_BootSector.ui_BbmSum, st_BootSector.ui_DbiSum,st_BootSector.ui_DbuSize);

    printf("#DDI start sector：%llu, DDI end sector：%llu \n", st_BootSector.ull_DdiStartSector,st_BootSector.ull_DdiEndSector);
    printf("#DBI start sector：%llu, DBI end sector：%llu \n", st_BootSector.ull_DbiStartSector,st_BootSector.ull_DbiEndSector);
    printf("#BBM start sector：%llu, BBM end sector：%llu \n", st_BootSector.ull_BbmStartSector,st_BootSector.ull_BbmEndSector);
    printf("#DBU start sector：%llu, DBU end sector：%llu \n", st_BootSector.ull_DbuStartSector,st_BootSector.ull_DbuEndSector);
    printf("#backup BBM2 start sector：%llu, backup BBM2 end sector：%llu \n", st_BootSector.ull_Bbm2StartSector,st_BootSector.ull_Bbm2EndSector);
    printf("#backup DBI2 start sector：%llu, backup DBI2 end sector：%llu \n", st_BootSector.ull_Dbi2StartSector,st_BootSector.ull_Dbi2EndSector);
    printf("#backup DDI2 start sector：%llu, backup DDI2 end sector：%llu \n", st_BootSector.ull_Ddi2StartSector,st_BootSector.ull_Ddi2EndSector);
    printf("----------------------------------------------\n");

    printf("\n#start write DDI..................\n");
    st_Ddi.ui_LastDbiIdx = -1;
    memset(st_Ddi.uc_Reserved, 0, sizeof(st_Ddi.uc_Reserved));
    st_Ddi.ui_EndFlag = 0x66aa;
    st_Ddi.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(&st_Ddi), sizeof(st_Ddi) - sizeof(st_Ddi.ui_Crc));
    ret = wrt_sector(pDev,(uint8_ddbm *)(&st_Ddi), st_BootSector.ull_DdiStartSector, 1);
    if(DISKIO_OK == ret)	printf("#write DDI info succeed..................\n");
    else					printf("#write DDI info failed...................\n");
    ret = wrt_sector(pDev,(uint8_ddbm *)(&st_Ddi), st_BootSector.ull_Ddi2StartSector, 1);
    if(DISKIO_OK == ret)	printf("#write backup DDI info succeed..................\n");
    else					printf("#write backup DDI info failed...................\n");

    printf("start write DBI：\n");
    /*在内存中申请DBI空间*/
    dbi_data_head = (STRUCT_DBI*)malloc(st_BootSector.ui_DbiSum*sizeof(STRUCT_DBI));
    if(dbi_data_head == NULL)
    {
        printf("malloc DBI space failed!\n");
        return DISKIO_ERROR;
    }
    for(i = 0; i < st_BootSector.ui_DbiSum; i++)
    {
        st_Dbi.ui_DbiIdx = i;
        st_Dbi.ui_ValidFlag = 0;
        st_Dbi.ui_EndFlag = 0x77aa;
        st_Dbi.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(&st_Dbi), sizeof(st_Dbi) - sizeof(st_Dbi.ui_Crc));
        memcpy((void*)&dbi_data_head[i], (void *)(&st_Dbi), sizeof(STRUCT_DBI));

    }
    ret = wrt_sector(pDev,(uint8_ddbm *)dbi_data_head, st_BootSector.ull_DbiStartSector, st_BootSector.ui_DbiSum);
    if(DISKIO_OK == ret)
        printf("#write DBI info succeed..................\n");
    else
        printf("#write DBI info failed..................\n");
    ret = wrt_sector(pDev,(uint8_ddbm *)dbi_data_head, st_BootSector.ull_Dbi2StartSector, st_BootSector.ui_DbiSum);
    if(DISKIO_OK == ret)
        printf("#write backup DBI info succeed.................\n");
    else
        printf("#write backup DBI info failed..................\n");

    /*在内存中申请BBM空间*/
    bbm_data_head = (STRUCT_BBM*)malloc(st_BootSector.ui_BbmSum*sizeof(STRUCT_BBM));
    if(bbm_data_head == NULL)
    {
        printf("maoolc BBM space failed!\n");
        return DISKIO_ERROR;
    }
    for(i = 0; i < st_BootSector.ui_BbmSum; i++)
    {
        st_Bbm.ui_BbmIdx = i;
        st_Bbm.ull_DbuStartSector = st_BootSector.ull_DbuStartSector + i*(st_BootSector.ui_DbuSize/SECTOR_SIZE);
        st_Bbm.ull_DbuEndSector   = st_Bbm.ull_DbuStartSector + (st_BootSector.ui_DbuSize/SECTOR_SIZE);
        st_Bbm.ui_UsedFlag = 0;
        st_Bbm.ui_EndFlag = 0x88aa;
        st_Bbm.ui_Crc = Ddbm_Crc_Check((uint8_ddbm *)(&st_Bbm), sizeof(st_Bbm) - sizeof(st_Bbm.ui_Crc));
        memcpy((void*)&bbm_data_head[i], (void *)(&st_Bbm), sizeof(STRUCT_BBM));

    }
    ret = wrt_sector(pDev,(uint8_ddbm *)bbm_data_head, st_BootSector.ull_BbmStartSector, st_BootSector.ui_BbmSum);
    if(DISKIO_OK == ret)
        printf("#write BBM info succeed..................\n");
    else
        printf("#write BBM info failed..................\n");

    ret = wrt_sector(pDev,(uint8_ddbm *)bbm_data_head, st_BootSector.ull_Bbm2StartSector, st_BootSector.ui_BbmSum);
    if(DISKIO_OK == ret)
        printf("#write backup BBM info succeed..................\n");
    else
        printf("#write backup BBM info failed..................\n");

    printf("\n format finished\n");

    /*增加一点，每次格式化成功之后，都初始化一次文件系统*/
    Ddbm_Init_FileSystem((int8_ddbm*)disk_name);

    return DISKIO_OK;
}
