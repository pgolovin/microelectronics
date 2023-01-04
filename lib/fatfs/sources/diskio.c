/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#ifdef _WIN32
#include "sdcard.h"
#else
#include "include/sdcard.h"
#endif

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return (SDCARD_OK == SDCARD_FAT_IsInitialized(pdrv)) ? RES_OK : STA_NOINIT;
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	return disk_status(pdrv);
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	return SDCARD_OK == SDCARD_FAT_Read(pdrv, buff, sector, count) ? RES_OK : RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	return SDCARD_OK == SDCARD_FAT_Write(pdrv, buff, sector, count) ? RES_OK : RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,			/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	if(cmd == CTRL_SYNC) 
	{
		return RES_OK;
	}
#ifdef _WIN32
	else if (GET_SECTOR_COUNT == cmd)
	{
		uint32_t* buff_i = buff;
		*buff_i = SDCARD_FAT_GetSectorsCount(pdrv);
		
		return RES_OK;
	}
	else if (GET_SECTOR_SIZE == cmd || GET_BLOCK_SIZE == cmd)
	{
		uint32_t* buff_i = buff;
		*buff_i = SDCARD_FAT_GetSectorSize(pdrv);
		return RES_OK;
	}
#endif
	// other commands are not supported
	return RES_PARERR;
}

