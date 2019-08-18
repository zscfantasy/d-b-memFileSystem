#ifndef __DDBM_COMMON_H__
#define __DDBM_COMMON_H__

#define int64_ddbm    long long               /*有符号64bit*/
#define uint64_ddbm   unsigned long long      /*无符号64bit*/
#define int32_ddbm    int                     /*有符号32bit*/
#define uint32_ddbm   unsigned int            /*无符号32bit*/
#define int16_ddbm    short                   /*有符号16bit*/
#define uint16_ddbm   unsigned short          /*无符号16bit*/
#define int8_ddbm     char                    /*有符号8bit*/
#define uint8_ddbm    unsigned char           /*无符号8bit*/
#define ubit_ddbm     unsigned                /*位域定义*/
#define SEM_ID_DDBM   int32_ddbm


#define SWAP16(x) ((unsigned short)(((unsigned short)(x) & (unsigned short)0x00ffU)<<8) | ((unsigned short)(x) & (unsigned short)0xff00U)>>8))
#define SWAP32(x) ((unsigned int)(((unsigned int)(x) & (unsigned int)0x000000ffUL) << 24) | ((unsigned int)(x) & (unsigned int)0x0000ff00UL) << 8 | ((unsigned int)(x) & (unsigned int)0x00ff0000UL) >> 8 | ((unsigned int)(x) & (unsigned int)0xff000000UL) >> 24))

#define DDBM_LOW32(x)  (long) (((long long)(x)) & (unsigned long) 0xffffffff)
#define DDBM_HIGH32(x) (long) ((((long long)(x)) >> 32) & (unsigned long)0xffffffff)


#endif
