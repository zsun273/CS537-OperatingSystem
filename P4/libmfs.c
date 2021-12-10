#include "mfs.h"
#include "udp.h"

//get buffer
char buffer[4096];

//sock addr struct
struct sockaddr_in addr;
int fd;
int portNum;
struct timeval timeCheck;
fd_set rfds;

int MFS_Init(char *hostname, int port) {
//    portNum = port;
//    fd = UDP_Open(0);
//    assert(fd > -1);
//
//    int rc = UDP_FillSockAddr(&addr, hostname, portNum);
//    assert(rc == 0);
    printf("init %s %d\n", hostname, port);
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    return 0;
}


//returns infomation about data given
int MFS_Stat(int inum, MFS_Stat_t *m) {
    return 0;
}

//write new
int MFS_Write(int inum, char *buffer, int block) {
    return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    return 0;
}

int MFS_Unlink(int pinum, char *name) {
    return 0;
}

int MFS_Shutdown() {
    return 0;
}

