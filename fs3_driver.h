#ifndef FS3_DRIVER_INCLUDED
#define FS3_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the FS3 storage system.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun 19 Sep 2021 08:12:43 AM PDT
//

// Include files
#include <stdint.h>
#include <fs3_controller.h>

// Defines
#define FS3_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define FS3_MAX_PATH_LENGTH 128 // Maximum length of filename length

// we use 0 to reprsent success so we make code easier to read
#define SUCCESS 0 
// we cannot define fail as (-1) since we cannot store -1 as a bit so we just 1 to represent fail 
#define FAIL 1 

//
// Interface functions

int32_t fs3_mount_disk(void);
	// FS3 interface, mount/initialize filesystem

int32_t fs3_unmount_disk(void);
	// FS3 interface, unmount the disk, close all files

int16_t fs3_open(char *path);
	// This function opens a file and returns a file handle

int16_t fs3_close(int16_t fd);
	// This function closes a file

int32_t fs3_read(int16_t fd, void *buf, int32_t count);
	// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t fs3_write(int16_t fd, void *buf, int32_t count);
	// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t fs3_seek(int16_t fd, uint32_t loc);
	// Seek to specific point in the file

int deconstruct_fs3_cmdblock(FS3CmdBlk cmdblock, uint8_t *op, uint16_t *sec, uint32_t *trk, uint8_t *ret);
	// Deconstruct the command block

#endif
