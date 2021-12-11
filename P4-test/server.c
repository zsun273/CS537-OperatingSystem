#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

//functions
int initImage(char *imgName);
int loadMem();
int sRead(int inum, char *buff, int blk);
int sWrite(int inum, char *buff, int blk);
int sUnlink(int pinum, char *name);
int sStat(int inum, MFS_Stat_t *m);
int sLookup(int pinum, char *name);
int sCreate(int pinum, int type, char *name);
int cImapPiece();
int delInode(int inum);
int delImap(int imapInd);
int cInode(int pinum, int type);
void handle(char * msgBuff);
int shutDown();

int sd;
struct sockaddr_in s;
char name[256];
int fdDisk;
checkpoint_t chkpt;
inodeArr_t iArr;

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: server <port> <file image>\n");
        exit(0);
    }
    int portNum = atoi(argv[1]);
    //sprintf(name, argv[2]);

    sd = UDP_Open(portNum); // use a port number to open
    assert(sd > -1);
    
    //init the server image
    initImage(argv[2]);
    loadMem();
    
    while (1) {
        char buffer[sizeof(msg_t)];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &s, buffer, sizeof(msg_t));
        if (rc > 0) {
            handle(buffer);
            rc = UDP_Write(sd, &s, buffer, sizeof(msg_t));
            printf("server:: reply\n");
        }
    }
    return 0;
}

int initImage(char *imgName) {
    fdDisk = open(imgName, O_RDWR); // open an existing file with r/w access

    if(fdDisk < 0) {                // file not exist, create and initialize
        fdDisk = open(imgName, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);

        // initialize the checkpoint region
        for(i = 0; i < 256; i++) {
            chkpt.imap[i] = -1;
        }
        chkpt.imap[0] = sizeof(checkpoint_t); // imap starts right after checkpoint region
        chkpt.endLog = sizeof(checkpoint_t) + sizeof(imap_t) + sizeof(inode_t) + sizeof(dir_t);

        //initialize the inode map
        imap_t imap;
        for(int i = 0; i < 16; i++) {
            imap.inodeArr[i] = -1;
        }
        imap.inodeArr[0] = sizeof(checkpoint_t)+sizeof(imap_t); // first inode starts right after the imap
        
        //initialize inode for root directory
        inode_t root;
        root.stat.type = MFS_DIRECTORY;
        root.stat.size = 2 * 4096;
        for(int i = 0; i < 14; i++) {
            root.blockArr[i] = -1;
        }
        root.blockArr[0] = sizeof(checkpoint_t) + sizeof(imap_t) + sizeof(inode_t);
        
        //write first directory block
        dir_t rootDir;
        for(int i = 0; i < 64; i++) { // TODO: upper bound need to change here
            rootDir.dirArr[i].inum = -1;
            sprintf(rootDir.dirArr[i].name, "x");
        }
        
        // initialize two directory entries
        rootDir.dirArr[0].inum = 0;
        sprintf(rootDir.dirArr[0].name, ".");
        rootDir.dirArr[1].inum = 0;
        sprintf(rootDir.dirArr[1].name, "..");

        lseek(fdDisk, 0, SEEK_SET);
        write(fdDisk, &chkpt, sizeof(checkpoint_t));
        write(fdDisk, &imap, sizeof(imap));
        write(fdDisk, &root, sizeof(inode_t));
        write(fdDisk, &rootDir, sizeof(dir_t));
    }
    else { //file exist, read in existing checkpoint region
        read(fdDisk, &chkpt, sizeof(checkpoint_t));
    }

    loadMem();
    
    return 0;
}

int loadMem() {
    lseek(fdDisk, 0, SEEK_SET);
    read(fdDisk, &chkpt, sizeof(checkpoint_t));

    for(int i = 0; i < 4096; i++) {
        iArr.inodeArr[i] = -1;
    }

    //store all inode info
    int k = 0;
    imap_t imapTemp;
    for(int i = 0; i < 256; i++) {
        if(chkpt.imap[i] >= 0) {
            lseek(fdDisk, chkpt.imap[i], 0);
            read(fdDisk, &imapTemp, sizeof(imap_t));
            for(int j = 0; j < 16; j++) {
                if(imapTemp.inodeArr[j] >= 0) {
                    iArr.inodeArr[k] = imapTemp.inodeArr[j];
                }
                k++;
            }
        }
    }
    return 0;
}

int sRead(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) return -1;
    if(blk > 13 || blk < 0) return -1;
    
    loadMem();
    
    if(iArr.inodeArr[inum] == -1) return -1;

    // read in the inode
    inode_t inode;
    lseek(fdDisk, iArr.inodeArr[inum], SEEK_SET);
    read(fdDisk, &inode, sizeof(inode_t));
    
    if(inode.blockArr[blk] == -1) return -1;
    
    lseek(fdDisk, inode.blockArr[blk], SEEK_SET);
    read(fdDisk, buff, BUFFER_SIZE);
    return 0;
}

int sWrite(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) {
        return -1;
    }
    
    if(blk > 13 || blk < 0) {
        return -1;
    }
    
    loadMem();
    
    if(iArr.inodeArr[inum] == -1) {
        return -1;
    }
    
    //read inode
    inode_t inode;
    lseek(fdDisk, iArr.inodeArr[inum], 0);
    read(fdDisk, &inode, sizeof(inode_t));
    
    if(inode.stat.type != 1) {
        return -1;
    }
    
    if(inode.blockArr[blk] == -1) {
        int endLogTmp = chkpt.endLog;
        dir_t nDirBlk;
        int i;
        for(i = 0; i < 64; i++) {
            sprintf(nDirBlk.dirArr[i].name, "\0");
            nDirBlk.dirArr[i].inum = -1;
        }
        lseek(fdDisk, chkpt.endLog, 0);
        write(fdDisk, &nDirBlk, sizeof(dir_t));
        
        chkpt.endLog += 4096;
        lseek(fdDisk, 0, 0);
        write(fdDisk, &chkpt, sizeof(checkpoint_t));
        
        inode.blockArr[blk] = endLogTmp;
        inode.stat.size = (blk + 1) * 4096;
        lseek(fdDisk, iArr.inodeArr[inum], 0);
        write(fdDisk, &inode, sizeof(inode_t));
        lseek(fdDisk, endLogTmp, 0);
    }
    else {
        inode.stat.size = (blk + 1) * 4096;
        lseek(fdDisk, iArr.inodeArr[inum], 0);
        write(fdDisk, &inode, sizeof(inode_t));
        lseek(fdDisk, inode.blockArr[blk], 0);
    }
    write(fdDisk, buff, 4096);
    printf("buffer: %s\n", buff);
    loadMem();
    return 0;
}

int sUnlink(int pinum, char *name) {
    loadMem();
    
    if(pinum < 0 || pinum > 4096) {
        return -1;
    }
    
    if(iArr.inodeArr[pinum] == -1) {
        return -1;
    }
    
    //test to see if name is equivalent to parent or current directory
    if(strcmp(name, "..\0") == 0 || strcmp(name, ".\0") == 0) {
        return -1;
    }
    
    if(strlen(name) > 60 || strlen(name) < 0) {
        return -1;
    }
    
    int pinumLoc = iArr.inodeArr[pinum];
    
    inode_t pInode;
    lseek(fdDisk, pinumLoc, 0);
    read(fdDisk, &pInode, sizeof(inode_t));

    if(pInode.stat.type != 0) {
        return -1;
    }
    
    int found = -1;
    int delDirBlkLoc;
    int delInd;
    int delInodeLoc;
    dir_t dirBlk;
    int i;
    int j;
    for(i = 0; i < 14; i++) {
        if(pInode.blockArr[i] >= 0) {
            lseek(fdDisk, pInode.blockArr[i], 0);
            read(fdDisk, &dirBlk, sizeof(dir_t));
            for(j = 0; j < 64; j++) {
                if(strcmp(dirBlk.dirArr[j].name, name) == 0) {
                    found = 1;
                    delInodeLoc = iArr.inodeArr[dirBlk.dirArr[j].inum];
                    delInd = j;
                    delDirBlkLoc = pInode.blockArr[i];
                    i = 15;
                    break;
                }
            }
        }
    }
    
    if(found < 0) {
        return -1;
    }
    
    inode_t inodeDel;
    lseek(fdDisk, delInodeLoc, 0);
    read(fdDisk, &inodeDel, sizeof(inodeDel));
    
    if(inodeDel.stat.type == 0) {
        if(inodeDel.stat.size > 2 * 4096) {
            return -1;
        }
    }

    delInode(delInodeLoc);
    
    dirBlk.dirArr[delInd].inum = -1;
    sprintf(dirBlk.dirArr[delInd].name, "\0");
    lseek(fdDisk, delDirBlkLoc, 0);
    write(fdDisk, &dirBlk, sizeof(dir_t));
    
    int sizeInd = 0;
    for(i = 0; i < 14; i++) {
        if(pInode.blockArr[i] != -1) {
            sizeInd = i;
        }
    }
    
    //increase size that can be put into directory
    pInode.stat.size = (sizeInd + 1) * 4096;
    lseek(fdDisk, pinumLoc, 0);
    write(fdDisk, &pInode, sizeof(inode_t));
    loadMem();
    return 0;
}

int sStat(int inum, MFS_Stat_t *m) {
    if(inum < 0 || inum >= 4096 ) return -1;
    if (fdDisk < 0) return -1;
    loadMem();

    if(iArr.inodeArr[inum] == -1) return -1;
    
    inode_t inode;
    lseek(fdDisk, iArr.inodeArr[inum], 0);
    read(fdDisk, &inode, sizeof(inode_t));
    
    m->type = inode.stat.type;
    m->size = inode.stat.size;
    
    return 0;
}

int sLookup(int pinum, char *name) {
    if(pinum < 0 || pinum >= 4096) return -1;
    if(strlen(name) < 1 || strlen(name) > 60) return -1; // TODO: name should be at most 28 bytes
    loadMem();
    //check if parent inode number  is valid
    if(iArr.inodeArr[pinum] == -1) return -1;

    //read the parent inode
    inode_t pInode;
    lseek(fdDisk, iArr.inodeArr[pinum], 0);
    read(fdDisk, &pInode, sizeof(pInode));
    if(pInode.stat.type != MFS_DIRECTORY) return -1;

    int dirBlk;
    for(int i = 0; i < 14; i++) {
        dirBlk = pInode.blockArr[i];
        lseek(fdDisk, dirBlk, 0);
        dir_t dirBlkTmp;
        read(fdDisk, &dirBlkTmp, sizeof(dir_t));
        for(int j = 0; j < 64; j++) { // TODO: upper bound should be 128
            // find the matched name and return
            if(strcmp(dirBlkTmp.dirArr[j].name, name) == 0) {
                return dirBlkTmp.dirArr[j].inum;
            }
        }
    }
    return -1;
}

int sCreate(int pinum, int type, char *name) {
    //makesure it is ok
    if((type != 0 && type != 1) || pinum < 0 || pinum > 4096) {
        return -1;
    }
    
    //check name size too see if too long or too short
    if(strlen(name) < 1 || strlen(name) >= 60) {
        return -1;
    }
    
    if(iArr.inodeArr[pinum] == -1) {
        return -1;
    }
    
    if(sLookup(pinum, name) >= 0) {
        return 0;
    }
    
    //get pinum location
    int pinumLoc = iArr.inodeArr[pinum];
    lseek(fdDisk, pinumLoc, 0);
    inode_t pInode;
    read(fdDisk, &pInode, sizeof(inode_t));
    
    if(pInode.stat.type != 0) {
        return -1;
    }
    
    if(pInode.stat.size >= (4096 * 14 * 64)) {
        return -1;
    }
    
    int newInum = cInode(pinum, type);
    int iDirBlkInd = pInode.stat.size / (4096 * 64);
    
    if(iDirBlkInd > 14) {
        return -1;
    }

    int dirBlkInd = (pInode.stat.size/(4096) % 64);
    
    
    pInode.stat.size += 4096;
    
    if(dirBlkInd == 0) {
        pInode.blockArr[iDirBlkInd] = chkpt.endLog;
        dir_t newDirBlk;
        int i;
        for(i = 0; i < 64; i++) {
            sprintf(newDirBlk.dirArr[i].name, "\0");
            newDirBlk.dirArr[i].inum = -1;
        }
        
        lseek(fdDisk, chkpt.endLog, 0);
        write(fdDisk, &newDirBlk, sizeof(dir_t));
        
        chkpt.endLog += 4096;
        lseek(fdDisk, 0, 0);
        write(fdDisk, &chkpt, sizeof(checkpoint_t));
    }
    
    lseek(fdDisk, pinumLoc, 0);
    write(fdDisk, &pInode, sizeof(pInode));
    loadMem();
    lseek(fdDisk, pInode.blockArr[iDirBlkInd], 0);
    dir_t dirBlk;
    read(fdDisk, &dirBlk, sizeof(dir_t));
    
    int nInd = dirBlkInd;
    sprintf(dirBlk.dirArr[nInd].name, "\0");
    sprintf(dirBlk.dirArr[nInd].name, name, 60);
    dirBlk.dirArr[nInd].inum = newInum;
    
    //write to updated directory
    lseek(fdDisk, pInode.blockArr[iDirBlkInd], 0);
    write(fdDisk, &dirBlk, sizeof(dir_t));
    
    loadMem();
    return 0;
}

int cImapPiece() {
    loadMem();
    int nImapInd;
    
    int i;
    for(i = 0; i < 256; i++) {
        if(chkpt.imap[i] == -1) {
            nImapInd = i;
            break;
        }
    }
    
    chkpt.imap[nImapInd] = chkpt.endLog;
    imap_t nimap;
    for(i = 0; i < 16; i++) {
        nimap.inodeArr[i] = -1;
    }
    
    chkpt.endLog += sizeof(imap_t);
    lseek(fdDisk, 0, 0);
    write(fdDisk, &chkpt, sizeof(checkpoint_t));
    lseek(fdDisk, chkpt.imap[nImapInd], 0);
    write(fdDisk, &nimap, sizeof(imap_t));
    loadMem();
    return nImapInd;
}

int delInode(int inum) {
    int imapInd = inum / 16;
    int imapInIndex = inum % 16;
    if(imapInIndex < 0) {
        return -1;
    }
    
    imap_t imapTmp;
    lseek(fdDisk, chkpt.imap[imapInd], 0);
    read(fdDisk, &imapTmp, sizeof(imap_t));
    
    imapTmp.inodeArr[imapInIndex] = -1;
    int i = 0;
    while(imapTmp.inodeArr[i] > 0 && i < 16) {
        i++;
    }
    
    if(i == 0) {
        int test = delImap(imapInd);
    }
    else {
        lseek(fdDisk, chkpt.imap[imapInd], 0);
        write(fdDisk, &imapTmp, sizeof(imapTmp));
    }
    loadMem();
    return 0;
    
}

int delImap(int imapInd) {
    loadMem();
    chkpt.imap[imapInd] = -1;
    
    //write to disk
    lseek(fdDisk, 0, 0);
    write(fdDisk, &chkpt, sizeof(checkpoint_t));
    loadMem();
    return 0;
}

int cInode(int pinum, int type) {
    loadMem();
    int nInodeNum;

    int i;
    for(i = 0; i < 4096; i++) {
        if(iArr.inodeArr[i] == -1) {
            nInodeNum = i;
            break;
        }
    }
    
    int imapInd = nInodeNum / 16;
    int nImapInd = nInodeNum % 16;
    
    int imapPart;
    if(nImapInd == 0) {
        imapPart = cImapPiece();
    }
    
    imap_t imapTmp;
    lseek(fdDisk, chkpt.imap[imapInd], 0);
    read(fdDisk, &imapTmp, sizeof(imap_t));
    
    imapTmp.inodeArr[nImapInd] = chkpt.endLog;
    lseek(fdDisk, chkpt.imap[imapInd], 0);
    write(fdDisk, &imapTmp, sizeof(imap_t));
    
    inode_t nInode;
    nInode.stat.type = type;
    for(i = 0; i < 14; i++) {
        nInode.blockArr[i] = -1;
    }
    
    nInode.blockArr[0] = chkpt.endLog + sizeof(inode_t);
    if(type == 0) {
        nInode.stat.size = 2 * 4096;
    } else {
        nInode.stat.size = 0;
    }
    
    lseek(fdDisk, chkpt.endLog, 0);
    write(fdDisk, &nInode, sizeof(inode_t));
    
    chkpt.endLog += sizeof(nInode);
    
    if(type == 0) {
        dir_t dirBlk;
        int k = sizeof(dirBlk)/sizeof(dirBlk.dirArr[0]);
        for(i = 0; i < k; i++) {
            dirBlk.dirArr[i].inum = -1;
            sprintf(dirBlk.dirArr[i].name, "\0");
        }
        
        sprintf(dirBlk.dirArr[0].name, ".\0");
        dirBlk.dirArr[0].inum = nInodeNum;
        
        sprintf(dirBlk.dirArr[1].name, "..\0");
        dirBlk.dirArr[1].inum = pinum;
        
        write(fdDisk, &dirBlk, sizeof(dir_t));
        chkpt.endLog += sizeof(dir_t);
    }
    else {
        char *nBlk = malloc(4096);
        write(fdDisk, nBlk, 4096);
        free(nBlk);
        chkpt.endLog += 4096;
    }
    
    lseek(fdDisk, 0, 0);
    write(fdDisk, &chkpt, sizeof(checkpoint_t));
    loadMem();
    return nInodeNum;
}

void handle(char * msgBuff) {
    msg_t *msg = (msg_t*) msgBuff;
    msg->returnNum = -1;

    switch(msg->lib) {
        case INIT:
            msg->returnNum = initImage(name);
            break;
        case LOOKUP:
            msg->returnNum = sLookup(msg->pinum, msg->name);
            break;
        case STAT:
            msg->returnNum = sStat(msg->inum, &(msg->stat));
            break;
        case WRITE:
            msg->returnNum = sWrite(msg->inum, msg->buffer, msg->block);
            break;
        case READ:
            msg->returnNum = sRead(msg->inum, msg->buffer, msg->block);
            break;
        case CREAT:
            msg->returnNum = sCreate(msg->pinum, msg->type, msg->name);
            break;
        case UNLINK:
            msg->returnNum = sUnlink(msg->pinum, msg->name);
            break;
        case SHUTDOWN:
            msg->returnNum = 0;
            memcpy(msgBuff, msg, sizeof(*msg));
            UDP_Write(sd, &s, msgBuff, sizeof(msg_t));
            fsync(fdDisk);
            close(fdDisk);
            exit(0);
    }
    memcpy(msgBuff, msg, sizeof(*msg));
}

int shutDown() {
    close(fdDisk);
    exit(0);
    return 0;
}

