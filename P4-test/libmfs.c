#include "mfs.h"
#include "udp.h"

char buffer[4096];
struct sockaddr_in addr;
int fd;
fd_set fdSet;
struct timeval time_eval;

MFS_Msg_t communicate(MFS_Msg_t* msg){
    int return_val = 0;
    int rc;
    while(return_val == 0) {
        rc = UDP_Write(fd, &addr, (char *) msg, sizeof(MFS_Msg_t));
        FD_ZERO(&fdSet);     //initialize the file descriptor set fdSet to have zero bits for all file descriptors
        FD_SET(fd, &fdSet);
        //wait 5 seconds
        time_eval.tv_sec = 5;
        time_eval.tv_usec = 0;
        return_val = select(fd + 1, &fdSet, NULL, NULL, &time_eval);
        if(return_val) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(fd, &retaddr, (char*) msg, sizeof(MFS_Msg_t));
            }
        }
    }
    return *msg;
}

int MFS_Init(char *hostname, int port) {
    fd = UDP_Open(0);
    assert(fd > -1);
    int rc = UDP_FillSockAddr(&addr, hostname, port);
    assert(rc == 0);
    return rc;
}

int MFS_Lookup(int pinum, char *name) {
    MFS_Msg_t msg;
    msg.pinum = pinum;
    msg.lib = LOOKUP;
    msg.returnNum = -1;
    memcpy(msg.name, name, sizeof(msg.name));
    sprintf(msg.buffer, "Default");
    msg = communicate(&msg);
    return msg.returnNum;
}


int MFS_Stat(int inum, MFS_Stat_t *m) {
    MFS_Msg_t msg;
    msg.inum = inum;
    msg.block = -1;
    msg.lib = STAT;
    msg.returnNum = -1;
    msg = communicate(&msg);
    m->type = msg.stat.type;
    m->size = msg.stat.size;
    return msg.returnNum;
}

int MFS_Write(int inum, char *buffer, int block) {
    MFS_Msg_t msg;
    msg.lib = WRITE;
    msg.inum = inum;
    msg.block = block;
    memcpy(msg.buffer, buffer, 4096);
    int rc;
    int return_val = 0;
    while(return_val == 0) {
        rc = UDP_Write(fd, &addr, (char*)&msg, sizeof(MFS_Msg_t));
        FD_ZERO(&fdSet);
        FD_SET(fd, &fdSet);
        time_eval.tv_sec = 5;
        time_eval.tv_usec = 0;
        return_val = select(fd + 1, &fdSet, NULL, NULL, &time_eval);

        if(return_val) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(fd, &retaddr, (char *)&msg, sizeof(MFS_Msg_t));
            }
        }

    }
    return msg.returnNum;
}

int MFS_Read(int inum, char *buffer, int block) {
    MFS_Msg_t msg;
    msg.inum = inum;
    memcpy(msg.buffer, buffer, 4096);
    msg.block = block;
    msg.lib = READ;
    msg.returnNum = -1;
    msg = communicate(&msg);
    memcpy(buffer, msg.buffer, 4096);
    return msg.returnNum;
}

int MFS_Creat(int pinum, int type, char *name) {
    MFS_Msg_t msg;
    memcpy(msg.name, name, sizeof(msg.name));
    msg.type = type;
    msg.pinum = pinum;
    msg.lib = CREAT;
    msg.returnNum = -1;
    int rc;
    int return_val = 0;
    while(return_val == 0) {
        rc = UDP_Write(fd, &addr, (char*)&msg, sizeof(MFS_Msg_t));
        FD_ZERO(&fdSet);
        FD_SET(fd, &fdSet);
        time_eval.tv_sec = 5;
        time_eval.tv_usec = 0;
        return_val = select(fd + 1, &fdSet, NULL, NULL, &time_eval);
        if(return_val) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(fd, &retaddr, (char *)&msg, sizeof(MFS_Msg_t));
            }
        }
    }
    return msg.returnNum;
}

int MFS_Unlink(int pinum, char *name) {
    MFS_Msg_t msg;
    memcpy(msg.name, name, sizeof(msg.name));
    msg.pinum = pinum;
    msg.lib = UNLINK;
    msg.returnNum = -1;
    msg = communicate(&msg);
    return msg.returnNum;
}

int MFS_Shutdown() {
    MFS_Msg_t msg;
    msg.lib = SHUTDOWN;
    msg.returnNum = 0;
    int rc;
    rc = UDP_Write(fd, &addr, (char *)&msg, sizeof(MFS_Msg_t));
    if(rc > 0) {
        struct sockaddr_in retaddr;
        rc = UDP_Read(fd, &retaddr, (char *)&msg, sizeof(MFS_Msg_t));
    }
    return 0;
}