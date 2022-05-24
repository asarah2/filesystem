////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_network.c
//  Description    : This is the network implementation for the FS3 system.

//
//  Author         : Patrick McDaniel (professor) and Sarah Babu (student)
//  Last Modified  : Thu 16 Sep 2021 03:04:04 PM EDT
//

// header files
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmpsc311_log.h>
#include <fs3_driver.h>
#include <fs3_network.h>
#include <netinet/in.h>

#define SA struct sockaddr
// defining types to convert signals
#define htonll64(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll64(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

//
//  Global data
unsigned char     *fs3_network_address = NULL; // Address of FS3 server
unsigned short     fs3_network_port = 0;       // Port of FS3 serve
int socket_fd = -1;                             // socket FD


//
// Network functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : network_fs3_syscall
// Description  : Perform a system call over the network
//
// Inputs       : cmd - the command block to send
//                ret - the returned command block
//                buf - the buffer to place received data in
// Outputs      : 0 if successful, -1 if failure

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf)
{
    uint8_t op = 0, retval = 0;
    uint16_t sec = 0;
    uint32_t trk = 0;
    
    // deconstruct the command block
    deconstruct_fs3_cmdblock(cmd, &op, &sec, &trk, &retval);

    // if this is mount operation, open a socket and connect to server.
    if (FS3_OP_MOUNT == op) {
        struct sockaddr_in server_address;

        // create socket
        socket_fd = socket(AF_INET, SOCK_STREAM, 0);

        // check if socket is already created
        if (-1 == socket_fd) {
            return(-1);
        }

        // Set the IP and port numnber
        if (NULL == fs3_network_address) {
            fs3_network_address = (unsigned char *) FS3_DEFAULT_IP;
        }
        if (0 == fs3_network_port) {
            fs3_network_port = FS3_DEFAULT_PORT;
        }
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = inet_addr((char *) fs3_network_address);
        server_address.sin_port = htons(fs3_network_port);

        // connect to server
        if (connect(socket_fd, (SA*)&server_address, sizeof(server_address)) != 0) {
            return(-1);
        }
    }

    // Fail if the socket is not created
    if (socket_fd == -1) {
        return (-1);
    }

    uint64_t temp_write = htonll64(cmd);
    // write command block to the socket
    write(socket_fd, &temp_write, sizeof(FS3CmdBlk));

    // check if write buffer or read buffer is valid
    uint8_t write_buf = 0;
    uint8_t read_buf = 0;
    switch (op) {
        case FS3_OP_MOUNT:
        case FS3_OP_TSEEK:
        case FS3_OP_UMOUNT: {
            write_buf = 0;
            read_buf = 0;
            break;
        }
        case FS3_OP_RDSECT: {
            write_buf = 0;
            read_buf = 1;
            break;
        }
        case FS3_OP_WRSECT: {
            write_buf = 1;
            read_buf = 0;
            break;
        }
        default: {
            return(-1);
        }
    }

    // send write_buf to server if required
    if (write_buf) {
        write(socket_fd, buf, FS3_SECTOR_SIZE);
    }

    FS3CmdBlk temp_read = 0;
    // Read the return command block from server
    read(socket_fd, &temp_read, sizeof(FS3CmdBlk));
    *ret = (FS3CmdBlk) ntohll64((uint64_t) temp_read);

    
    // deconstruct the return command block from server
    deconstruct_fs3_cmdblock(*ret, &op, &sec, &trk, &retval);

    // returns -1 if fs3syscall fails
    if (retval==FAIL) {
    	return (-1);
    }

    // read the return buffer if required
    if (read_buf) {
        read(socket_fd, buf, FS3_SECTOR_SIZE);
    }

    // close the socket if the operation is unmount
    if (FS3_OP_UMOUNT == op) {
        if (-1 != socket_fd) {
            return(-1);
        }
        close(socket_fd);
        socket_fd = -1;
    }

    // Return successfully
    return (0);
}
