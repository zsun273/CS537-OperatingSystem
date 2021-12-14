#include "mfs.h"
#include "udp.h"

char buffer[4096];
struct sockaddr_in addr;
int sd;
struct timeval timeCheck;
fd_set rfds;

void communicate(msg_t* msg){
    int valReturn = 0;
    int rc;
    //wait until one gets a returned value
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char *) msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        //wait for 5 seconds
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                //grab back return val from server
                rc = UDP_Read(sd, &retaddr, (char*) msg, sizeof(msg_t));
            }
        }
    }
}


int MFS_Init(char *hostname, int port) {
    sd = UDP_Open(0);
    assert(sd > -1);
    int rc = UDP_FillSockAddr(&addr, hostname, port);
    assert(rc == 0);
    return rc;
}

int MFS_Lookup(int pinum, char *name) {
    //initialize message
    msg_t msg;
    msg.block = -1;
    msg.inum = -1;
    msg.pinum = pinum;
    msg.type = -1;
    msg.lib = LOOKUP;
    msg.returnNum = -1;
    memcpy(msg.name, name, sizeof(msg.name));
    sprintf(msg.buffer, "Default");

    int valReturn = 0;
    int rc;
    //wait until one gets a returned value
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char *) &msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        //wait for 5 seconds
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                //grab back return val from server
                rc = UDP_Read(sd, &retaddr, (char*) &msg, sizeof(msg_t));
            }
        }
    }
    //communicate(&msg);
    return msg.returnNum;
}


int MFS_Stat(int inum, MFS_Stat_t *m) {
    msg_t msg;
    msg.inum = inum;
    msg.block = -1;
    msg.lib = STAT;
    msg.pinum = -1;
    msg.type = -1;
    msg.returnNum = -1;

    int valReturn = 0;
    int rc;
    //wait for 5 sec and write values in
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char *) &msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(sd, &retaddr, (char*)&msg, sizeof(msg_t));
            }
        }
    }
    
    m->type = msg.stat.type;
    m->size = msg.stat.size;
    
    return msg.returnNum;
}

int MFS_Write(int inum, char *buffer, int block) {
    msg_t msg;
    msg.lib = WRITE;
    msg.inum = inum;
    msg.block = block;
    memcpy(msg.buffer, buffer, 4096);

    int valReturn = 0;
    int rc;
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char*)&msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(sd, &retaddr, (char *)&msg, sizeof(msg_t));
            }
        }
        
    }
    
    return msg.returnNum;
}

int MFS_Read(int inum, char *buffer, int block) {
    msg_t msg;
    msg.inum = inum;
    memcpy(msg.buffer, buffer, 4096);
    msg.block = block;
    msg.lib = READ;
    msg.type = -1;
    msg.pinum = -1;
    msg.returnNum = -1;

    int valReturn = 0;
    int rc;
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char*)&msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);

        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(sd, &retaddr, (char *)&msg, sizeof(msg_t));
            }
        }

    }
    //communicate(&msg);

    memcpy(buffer, msg.buffer, 4096);
    return msg.returnNum;
}

int MFS_Creat(int pinum, int type, char *name) {
    msg_t msg;
    memcpy(msg.name, name, sizeof(msg.name));
    msg.type = type;
    msg.pinum = pinum;
    msg.lib = CREAT;
    msg.inum = -1;
    msg.block = -1;
    msg.returnNum = -1;

    int valReturn = 0;
    int rc;
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char*)&msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(sd, &retaddr, (char *)&msg, sizeof(msg_t));
            }
        }
    }
    return msg.returnNum;
}

int MFS_Unlink(int pinum, char *name) {
    msg_t msg;
    memcpy(msg.name, name, sizeof(msg.name));
    msg.pinum = pinum;
    msg.lib = UNLINK;
    msg.inum = -1;
    msg.type = -1;
    msg.block = -1;
    msg.returnNum = -1;

    int valReturn = 0;
    int rc;
    while(valReturn == 0) {
        rc = UDP_Write(sd, &addr, (char*)&msg, sizeof(msg_t));
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        timeCheck.tv_sec = 5;
        timeCheck.tv_usec = 0;
        valReturn = select(sd + 1, &rfds, NULL, NULL, &timeCheck);
        
        if(valReturn) {
            if(rc > 0) {
                struct sockaddr_in retaddr;
                rc = UDP_Read(sd, &retaddr, (char *)&msg, sizeof(msg_t));
            }
        }
    }
    return msg.returnNum;
}

int MFS_Shutdown() {
    msg_t msg;
    msg.lib = SHUTDOWN;
    msg.returnNum = 0;
    int rc;
    rc = UDP_Write(sd, &addr, (char *)&msg, sizeof(msg_t));
    if(rc > 0) {
        struct sockaddr_in retaddr;
        rc = UDP_Read(fd, &retaddr, (char *)&msg, sizeof(msg_t));
    }
    return 0;
}