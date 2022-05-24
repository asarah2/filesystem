////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : Sarah Babu
//   Last Modified : 11/19/2021
//

// Includes
//including all the driver files that we have used in the program as well as the header files that is provided for us
#include <stdio.h>
#include <string.h> 
#include <cmpsc311_log.h> 
#include <stdlib.h>
#include <stdbool.h>
#include <fs3_network.h>

// Project Includes 
#include <fs3_driver.h>
#include <fs3_controller.h>

// including the cache.h header file so we can initiliaze pointers
// we also include it to allow main program to add to the cache list
#include <fs3_cache.h>

//
// Defines

// calculating the index number of the sector using the sector size
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE)) 

// intializing file open as 1 and file close as 0
#define FILE_OPEN 1 
#define FILE_CLOSE 0

//max files we can store 
#define MAX_FILES (FS3_MAX_TRACKS*FS3_TRACK_SIZE) 

//defining logical statements so our code is easier to understand
#define FALSE 0   
#define TRUE 1
 
//making file handlers structure
typedef struct file_info { 
    uint32_t sector_id[512];
    uint16_t num_sectors;
    uint64_t len;
    uint64_t pos;
    char *path;
    int file_state;
} file_t;


//define array of file handlers
file_t file_handlers[MAX_FILES] = {0}; 
// indicates sector usage
uint8_t sector_usage[FS3_MAX_TRACKS][FS3_TRACK_SIZE] = {0}; 
uint32_t sectors_used = 0;

//
// Implementation:
int get_free_sector(uint16_t *track, uint16_t *sector) {
	// initliazing found_free_sector and other variables 
    int found_free_sector = FALSE;
    int retval= FALSE;
	uint16_t i = 0, j = 0;
	// checking whether there is a free track/sector using for loop
	while((i<FS3_MAX_TRACKS) && (found_free_sector == FALSE)) {
		while((j<FS3_TRACK_SIZE) && (found_free_sector == FALSE)){
			if (sector_usage[i][j] == FALSE) 
			{
				//breaks when there is a sector available
				found_free_sector = TRUE;
				continue;
				i++;
			}
			if (found_free_sector == TRUE) 
			{
				//if free sector found, continue with program
				continue;
			}
                        j++;

		}
		if (found_free_sector == FALSE) {
		    i++;
                    j = 0;
		}
	}
	// return sector and track if free sector 
    if (found_free_sector == TRUE) {
        retval = TRUE;
        *track = i;
        *sector = j;
        sectors_used++;
    } else {
        retval = FALSE;
    }

    return(retval);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : construct_fs3_cmdblock
// Description  : Construct to cmd_block from the op code, sector, track and return value
//
// Inputs       : op code, sector, track and return value. 
// Outputs      : hexadecimal command block value

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret) { 
	//this function will create the commad block which lets us know what to do in our file system based on the given inputs
    //initlizing the command block digits to 0
	FS3CmdBlk ret_cmd = 0x0; 
    // create command block from the inputs given
    ret_cmd = (((FS3CmdBlk) op) << 60) | (((FS3CmdBlk) sec) << 44) | (((FS3CmdBlk) trk) << 12) | (((FS3CmdBlk) ret) << 11);
	//returns the created command block 
	return(ret_cmd); 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : deconstruct_fs3_cmdblock
// Description  : Deconstruct from cmd_block to the op code, sector, track and return value
//
// Inputs       : cmdblock, pointers to - op code, sector, track and return value. 
// Outputs      : returns 1 on completion

int deconstruct_fs3_cmdblock(FS3CmdBlk cmdblock, uint8_t *op, uint16_t *sec, uint32_t *trk, uint8_t *ret) {
	// extracts the data from the command block back into their individual parameters by bit wise operations
    *op = (uint8_t) ((cmdblock & 0xf000000000000000) >> 60); 
    *sec = (uint16_t) ((cmdblock & 0x0ffff00000000000) >> 44);
    *trk = (uint32_t) ((cmdblock & 0xf0000ffffffff000) >> 12);
    *ret = (uint8_t) ((cmdblock & 0x0000000000000800) >> 11);
	return 1;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) { 
	//initializing variables that we are going to use in the function
	FS3CmdBlk cmd_blk = 0; 
	FS3CmdBlk ret_cmd_blk = 0;
	uint32_t trk = 0;
	uint8_t op = 0, ret = 0;
	uint16_t sec = 0;
	//constructing the cmdblock to mount
	cmd_blk = construct_fs3_cmdblock(FS3_OP_MOUNT, 0, 0, 0); 
	// pass the cmdblock to the mount file system using fs3syscall
	network_fs3_syscall(cmd_blk, &ret_cmd_blk, NULL);
	// extract return value using the command block that outputs from the syscall
	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret); 
	// if the mounting is success, we get ret=0
	return(ret==SUCCESS); 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {
	//initializing variables that we are going to use in the function
	FS3CmdBlk cmd_blk = 0; 
	FS3CmdBlk ret_cmd_blk = 0;
	uint8_t op = 0, ret = 0;
	uint32_t trk = 0;
	uint16_t sec = 0;
	//constructing the cmdblock to unmount
	cmd_blk = construct_fs3_cmdblock(FS3_OP_UMOUNT, 0, 0, 0);
	// pass the cmdblock to the mount file system using fs3syscall
	network_fs3_syscall(cmd_blk,&ret_cmd_blk, NULL);
	// extract return value using the command block that outputs from the syscall
	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret); 
	// if the mounting is success, we get ret=0
	return(ret==SUCCESS);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) {
	// declaring the variables that we are using for this function
	uint64_t i = 0;
	int16_t free_handle = -1;

	// looping through all the file handlers  
	for(i=0; i < MAX_FILES; i++) { 
		// check whether file exists in the file system
		if ((file_handlers[i].path != NULL) && (0==strcmp(path, file_handlers[i].path))) { 
			//if the file is open, we return -1 
			if (file_handlers[i].file_state == FILE_OPEN) { 
				return(-1);
			} else { 
				// otherwise reset the read/write pointer and file state
				file_handlers[i].pos = 0; 
				file_handlers[i].file_state = FILE_OPEN;
				//return filehandler as output
				return(i); 
			}
		} else {
			//remember the first free file handler
			if (file_handlers[i].path == NULL) { 
				if (free_handle == -1) {
					free_handle = i;
				}
			}
		}
	}
	// returns -1 if no free file handler is found 
	if (free_handle == -1) {
		return(-1);
	}

	//saving the details of the file in the file handlers array and reset the position/length of the file
	file_handlers[free_handle].num_sectors = 0;
	file_handlers[free_handle].len = 0;
	file_handlers[free_handle].pos = 0;
	file_handlers[free_handle].path = calloc(strlen(path)+1, sizeof(char));
	strcpy(file_handlers[free_handle].path, path);
	file_handlers[free_handle].file_state = FILE_OPEN;

	//returns the file handle 
	return (free_handle);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {
	// check if file handle is valid
	if ((fd < 0) || (fd >= MAX_FILES)) {
		return(-1);
	}
	//checks if the file is open
	if (file_handlers[fd].file_state != FILE_OPEN) {
		return(-1);
	}
	//resets file read/write pointer and file state
	file_handlers[fd].pos = 0;
	file_handlers[fd].file_state = FILE_CLOSE;

	//return 0 if function is successful
	return (0);
}

uint32_t fs3_net_read( uint16_t track, uint16_t sector, void *buf) {
	uint16_t sec = 0;
	uint8_t op = 0, ret = 0;
	FS3CmdBlk cmd_blk = 0;
	FS3CmdBlk ret_cmd_blk = 0;
	uint32_t trk = 0;

    	//creates command block to seek to the given track/sector
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, track, 0);
    
    	// passes the command block to the fs3syscall
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, NULL);
    
    	//deconstructs the return value from command block after being passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fail
    	if (ret==FAIL) {
    		return (-1);
    	}

    	// constructs command block to read from given sector associated with the file
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_RDSECT, sector, 0, 0);
    
    	// passes the command block to fs3syscall 
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, buf);
    
    	// deconstructs the command block to get the return values after passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fail
    	if (ret==FAIL) {
    		return (-1);
    	}

	return 1;
}

int32_t fs3_read_first_twenty(int16_t fd, void *buf, int32_t count) {

	//initialising the variables of the function
	FS3CmdBlk cmd_blk = 0;
	FS3CmdBlk ret_cmd_blk = 0;
	uint8_t op = 0, ret = 0;
	uint16_t sec = 0;
	uint32_t trk = 0;
	//intialising the temp buffer 
	uint8_t temp_buf[FS3_SECTOR_SIZE*512];

	// check if file handle is valid
	if ((fd < 0) || (fd >= MAX_FILES)) {
		return(-1);
	}
	//checks if the file is open
	if (file_handlers[fd].file_state != FILE_OPEN) {
		return(-1);
	}
	// reset count if the data to be written is more than the free space in the sector 
	// initializing the variables used 
    uint32_t cur_pos = file_handlers[fd].pos;
    uint32_t remaining_count = count;
    uint32_t read_index = 0;
    uint8_t sector_buf[FS3_SECTOR_SIZE];
    int16_t sector_index = 0;
	uint32_t bytes_to_read = 0;
    void* cache_data = NULL;
	uint16_t track = 0, sector = 0;

	// checking if there is any more bytes to read
    while (remaining_count > 0) {
		// checking if there is enough space in the sector to write to cache
        if (((cur_pos % FS3_SECTOR_SIZE) + remaining_count) > FS3_SECTOR_SIZE) {
            bytes_to_read = FS3_SECTOR_SIZE - (cur_pos % FS3_SECTOR_SIZE);
        } else {
            bytes_to_read = remaining_count; // holds the remaining bytes
        }
		// calculates the final index in sector
        sector_index = cur_pos / FS3_SECTOR_SIZE; 
		// calculating the track and sector based on file_handlers
 		track = file_handlers[fd].sector_id[sector_index] / FS3_TRACK_SIZE;
 		sector = file_handlers[fd].sector_id[sector_index] % FS3_TRACK_SIZE;

		//check whether pre-existing cache data
        if (NULL != (cache_data = fs3_get_cache(track, sector))) 
		{	
			// we store the cache in temp buffer
    	    memcpy(&temp_buf[read_index], cache_data, bytes_to_read);\
			// remove the read portion to remaining count
            remaining_count -= bytes_to_read;
			// add to current position/read index
            cur_pos += bytes_to_read;
            read_index += bytes_to_read;
            continue;
        }

    	// constructs command block to seek to the sector/track of the given file
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, track, 0);
    
    	// passing command block to fs3syscall 
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, NULL);
    
    	//deconstruct the return values from the command block after passing through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fs3syscall failed
    	if (ret==FAIL) {
    		return (-1);
    	}

    	//constructs command block to read from the given track/sector
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_RDSECT, sector, 0, 0);
    	
    	//passes the command block to fs3 syscall
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, sector_buf);
    
    	// deconstructs the return values from the command block after it is passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    	
    	// returns -1 if fs3syscall fails
    	if (ret==FAIL) {
    		return (-1);
    	}

		// store the sector index as a buffer
    	memcpy(&temp_buf[read_index], &(sector_buf[cur_pos % FS3_SECTOR_SIZE]),bytes_to_read);

		// inputting current track/sector/sector buffer to cache
        fs3_put_cache(track, sector, sector_buf);
		// modifying our cache counts based on read values
        remaining_count -= bytes_to_read;
        cur_pos += bytes_to_read;
        read_index += bytes_to_read;
    }

    // extract data based on what is returned by fs3syscall 
    memcpy(buf, temp_buf, count);

	// returns the number of bytes that has been read
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {

	//initialising the variables of the function
	FS3CmdBlk cmd_blk = 0;
	FS3CmdBlk ret_cmd_blk = 0;
	uint8_t op = 0, ret = 0;
	uint16_t sec = 0;
	uint32_t trk = 0;
	//intialising the temp buffer 
	uint8_t temp_buf[FS3_SECTOR_SIZE*512];

	// check if file handle is valid
	if ((fd < 0) || (fd >= MAX_FILES)) {
		return(-1);
	}
	//checks if the file is open
	if (file_handlers[fd].file_state != FILE_OPEN) {
		return(-1);
	}
	// reset count if the data to be written is more than the free space in the sector 
	if (count > (file_handlers[fd].len - file_handlers[fd].pos)) {
		count = file_handlers[fd].len - file_handlers[fd].pos + 1;
	}
	// initializing the variables used 
    uint32_t cur_pos = file_handlers[fd].pos;
    uint32_t remaining_count = count;
    uint32_t read_index = 0;
    uint8_t sector_buf[FS3_SECTOR_SIZE];
    int16_t sector_index = 0;
	uint32_t bytes_to_read = 0;
    void* cache_data = NULL;
	uint16_t track = 0, sector = 0;

	// checking if there is any more bytes to read
    while (remaining_count > 0) {
		// checking if there is enough space in the sector to write to cache
        if (((cur_pos % FS3_SECTOR_SIZE) + remaining_count) > FS3_SECTOR_SIZE) {
            bytes_to_read = FS3_SECTOR_SIZE - (cur_pos % FS3_SECTOR_SIZE);
        } else {
            bytes_to_read = remaining_count; // holds the remaining bytes
        }
		// calculates the final index in sector
        sector_index = cur_pos / FS3_SECTOR_SIZE; 
		// calculating the track and sector based on file_handlers
 		track = file_handlers[fd].sector_id[sector_index] / FS3_TRACK_SIZE;
 		sector = file_handlers[fd].sector_id[sector_index] % FS3_TRACK_SIZE;

		//check whether pre-existing cache data
        if (NULL != (cache_data = fs3_get_cache(track, sector))) 
		{	
			// we store the cache in temp buffer
    	    memcpy(&temp_buf[read_index], cache_data, bytes_to_read);\
			// remove the read portion to remaining count
            remaining_count -= bytes_to_read;
			// add to current position/read index
            cur_pos += bytes_to_read;
            read_index += bytes_to_read;
            continue;
        }

    	// constructs command block to seek to the sector/track of the given file
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, track, 0);
    
    	// passing command block to fs3syscall 
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, NULL);
    
    	//deconstruct the return values from the command block after passing through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fs3syscall failed
    	if (ret==FAIL) {
    		return (-1);
    	}

    	//constructs command block to read from the given track/sector
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_RDSECT, sector, 0, 0);
    	
    	//passes the command block to fs3 syscall
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, sector_buf);
    
    	// deconstructs the return values from the command block after it is passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    	
    	// returns -1 if fs3syscall fails
    	if (ret==FAIL) {
    		return (-1);
    	}

		// store the sector index as a buffer
    	memcpy(&temp_buf[read_index], &(sector_buf[cur_pos % FS3_SECTOR_SIZE]),bytes_to_read);

		// inputting current track/sector/sector buffer to cache
        fs3_put_cache(track, sector, sector_buf);
		// modifying our cache counts based on read values
        remaining_count -= bytes_to_read;
        cur_pos += bytes_to_read;
        read_index += bytes_to_read;
    }

    // extract data based on what is returned by fs3syscall 
    memcpy(buf, temp_buf, count);

    file_handlers[fd].pos += count;
	// returns the number of bytes that has been read
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) {
	//initialising the variables of the function
	FS3CmdBlk cmd_blk = 0;
	FS3CmdBlk ret_cmd_blk = 0;
	uint32_t trk = 0;
	uint8_t op = 0, ret = 0;
	uint16_t sec = 0;
	//initialising the temp buffer
	uint8_t temp_buf[FS3_SECTOR_SIZE];
	uint16_t copy_count;

	// check if file handle is valid
	if ((fd < 0) || (fd >= MAX_FILES)) {
		return(-1);
	}
	// checking whether the file is open
	if (file_handlers[fd].file_state != FILE_OPEN) {
		return(-1);
	}

	// checking if there is not enough space in sector
	//if (file_handlers[fd].pos + count > FS3_SECTOR_SIZE*10) {
	//	return(-1);
	//}
	// initializing variables used
    uint32_t cur_count = count;
    int16_t sector_index = 0;
    uint16_t copy_index = 0, read_index = 0;
    uint16_t track = 0, sector_id = 0,sector = 0,vacancy = 0;


	// loop through bytes to write 
    while (cur_count > 0) {
		//checking if space is available to write in sector
        sector_index = (file_handlers[fd].pos)/FS3_SECTOR_SIZE;
		// if there is not enough space, we write in next sector the remaining bytes
        if (sector_index+1 > file_handlers[fd].num_sectors) {
            if (FALSE == get_free_sector(&track, &sector)) {
                return(-1);
            }
			// we copy the sector id/track id to file_handlers so we can write remaining bytes
			sector_usage[track][sector] = TRUE;
            file_handlers[fd].num_sectors++;
	        sector_id = ((track)*FS3_TRACK_SIZE) + sector;
            file_handlers[fd].sector_id[(file_handlers[fd].num_sectors)-1] = sector_id;
        } else {
           sector_id = file_handlers[fd].sector_id[sector_index];
        }

 	track = sector_id / FS3_TRACK_SIZE;
 	sector = sector_id % FS3_TRACK_SIZE;
	//calculating the remaining bytes available in the sector
        vacancy = FS3_SECTOR_SIZE - ((file_handlers[fd].pos) % FS3_SECTOR_SIZE);

	// we check if vacancy is enough to copy the remaining code 
	// decrementing current count to check how much space is left in sector
        if (cur_count > vacancy) {
            copy_count = vacancy;
        } else {
            copy_count = cur_count;
        }
        cur_count -= copy_count;

    	//creates command block to seek to the given track/sector
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, track, 0);
    
    	// passes the command block to the fs3syscall
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, NULL);
    
    	//deconstructs the return value from command block after being passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fail
    	if (ret==FAIL) {
    		return (-1);
    	}

    	// constructs command block to read from given sector associated with the file
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_RDSECT, sector, 0, 0);
    
    	// passes the command block to fs3syscall 
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, temp_buf);
    
    	// deconstructs the command block to get the return values after passed through fs3syscall
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fail
    	if (ret==FAIL) {
    		return (-1);
    	}
		uint8_t *dummy_buffer = (uint8_t *)buf;
    	memcpy(&(temp_buf[(file_handlers[fd].pos) % FS3_SECTOR_SIZE]),
               &dummy_buffer[copy_index],
               copy_count);
        copy_index += copy_count;

    	// constructs command block to write based on the sector/track
    	cmd_blk = construct_fs3_cmdblock(FS3_OP_WRSECT, sector, 0, 0);
    
    	// passes the command block to fs3syscall 
    	network_fs3_syscall(cmd_blk, &ret_cmd_blk, temp_buf);
    
    	// deconstructs the command block after passing it through fs3syscall 
    	deconstruct_fs3_cmdblock(ret_cmd_blk, &op, &sec, &trk, &ret);
    
    	//returns -1 if fail
    	if (ret==FAIL) {
    		return (-1);
    	}

        if (1 == fs3_put_cache(track, sector, &temp_buf[read_index])) {
            return(-1);
        }

    	// adjusts file length if the file length increases based on the write pointer
    	if (((file_handlers[fd].pos) + copy_count) > file_handlers[fd].len)
    	{	
    		file_handlers[fd].len = file_handlers[fd].pos + copy_count;
    	}
	
        file_handlers[fd].pos += copy_count;
    }
	return (count);
}

// return ???

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {
	//checks if the file handler is valid
	if ((fd < 0) || (fd >= MAX_FILES)) {
		return(-1);
	}
	// checks if the given file is open
	if (file_handlers[fd].file_state != FILE_OPEN) {
		return(-1);
	}
	//checks if the file is big enough to seek
	if (loc > file_handlers[fd].len) {
		return(-1);
	}
	// set read/write pointer at the seek position 
	file_handlers[fd].pos = loc;
	//returns 0 if successful
	return (0);
}


