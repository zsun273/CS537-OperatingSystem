#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

int init_image(char *image);
void set_return_number(char * buffer);
int load_inode_loc();
int server_read(int inum, char *buff, int block);
int server_write(int inum, char *buff, int block);
int server_unlink(int pinum, char *name);
int del_inode_imap(int inum);
int server_stat(int inum, MFS_Stat_t *m);
int server_lookup(int pinum, char *name);
int server_create(int pinum, int type, char *name);
int create_inode_imap(int pinum, int type);
int server_shutDown();

int sd;                 // global socket descriptor
struct sockaddr_in s;
char name[256];
int fd;                 // global file descriptor
checkpoint_t CR;        // checkpoint region for log
inodeArr_t inode_locs;  // all inode locations

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: server <port> <file image>\n");
        exit(0);
    }
    int port = atoi(argv[1]);
    sd = UDP_Open(port); // use a port number to open
    assert(sd > -1);

    //init the server image
    init_image(argv[2]);
    load_inode_loc();

    while (1) {
        char buffer[sizeof(MFS_Msg_t)];
        int rc = UDP_Read(sd, &s, buffer, sizeof(MFS_Msg_t));
        if (rc > 0) {
            set_return_number(buffer);
            rc = UDP_Write(sd, &s, buffer, sizeof(MFS_Msg_t));
        }
    }
    return 0;
}

void set_return_number(char * buffer) {
    MFS_Msg_t *msg = (MFS_Msg_t*) buffer;
    msg->returnNum = -1;

    switch(msg->lib) {
        case INIT:
            msg->returnNum = init_image(name);
            break;
        case LOOKUP:
            msg->returnNum = server_lookup(msg->pinum, msg->name);
            break;
        case STAT:
            msg->returnNum = server_stat(msg->inum, &(msg->stat));
            break;
        case WRITE:
            msg->returnNum = server_write(msg->inum, msg->buffer, msg->block);
            break;
        case READ:
            msg->returnNum = server_read(msg->inum, msg->buffer, msg->block);
            break;
        case CREAT:
            msg->returnNum = server_create(msg->pinum, msg->type, msg->name);
            break;
        case UNLINK:
            msg->returnNum = server_unlink(msg->pinum, msg->name);
            break;
        case SHUTDOWN:
            msg->returnNum = 0;
            memcpy(buffer, msg, sizeof(*msg));
            UDP_Write(sd, &s, buffer, sizeof(MFS_Msg_t));
            fsync(fd);
            close(fd);
            exit(0);
    }
    memcpy(buffer, msg, sizeof(*msg));
}

int init_image(char *image) {
    fd = open(image, O_RDWR); // open an existing file with r/w access

    if(fd < 0) {                // file not exist, create and initialize
        fd = open(image, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);

        // initialize the checkpoint region
        for(int i = 0; i < 256; i++) {
            CR.imap[i] = -1;
        }
        CR.imap[0] = sizeof(checkpoint_t); // imap starts right after checkpoint region
        CR.endLog = sizeof(checkpoint_t) + sizeof(imap_t) + sizeof(inode_t) + sizeof(dir_t);

        //initialize the inode map
        imap_t imap;
        for(int i = 0; i < 16; i++) {
            imap.inodeArr[i] = -1;
        }
        imap.inodeArr[0] = sizeof(checkpoint_t) + sizeof(imap_t); // first inode starts right after the imap

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
        for(int i = 0; i < 128; i++) {
            rootDir.dirArr[i].inum = -1;
            sprintf(rootDir.dirArr[i].name, "x");
        }

        // initialize two directory entries
        rootDir.dirArr[0].inum = 0;
        sprintf(rootDir.dirArr[0].name, ".");
        rootDir.dirArr[1].inum = 0;
        sprintf(rootDir.dirArr[1].name, "..");

        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(checkpoint_t));
        write(fd, &imap, sizeof(imap));
        write(fd, &root, sizeof(inode_t));
        write(fd, &rootDir, sizeof(dir_t));
    }
    else { //file exist, read in existing checkpoint region
        read(fd, &CR, sizeof(checkpoint_t));
    }

    // initialize the inode location array
    for(int i = 0; i < 4096; i++) {
        inode_locs.inodeArr[i] = -1;
    }
    load_inode_loc();

    return 0;
}

int load_inode_loc() {
    lseek(fd, 0, SEEK_SET);
    read(fd, &CR, sizeof(checkpoint_t));

    //store all inode info
    int k = 0;
    imap_t imapTemp;
    for(int i = 0; i < 256; i++) {
        if(CR.imap[i] >= 0) {
            lseek(fd, CR.imap[i], SEEK_SET);
            read(fd, &imapTemp, sizeof(imap_t));
            for(int j = 0; j < 16; j++) {
                if(imapTemp.inodeArr[j] >= 0) {
                    inode_locs.inodeArr[k] = imapTemp.inodeArr[j];
                }
                k++;
            }
        }
    }
    return 0;
}

int server_read(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) return -1;
    if(blk > 13 || blk < 0) return -1;

    load_inode_loc();

    if(inode_locs.inodeArr[inum] == -1) return -1;

    // read in the inode
    inode_t inode;
    lseek(fd, inode_locs.inodeArr[inum], SEEK_SET);
    read(fd, &inode, sizeof(inode_t));

    if(inode.blockArr[blk] == -1) return -1;

    lseek(fd, inode.blockArr[blk], SEEK_SET);
    read(fd, buff, BUFFER_SIZE);
    return 0;
}

int server_write(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) return -1;
    if(blk > 13 || blk < 0) return -1;

    load_inode_loc();

    if(inode_locs.inodeArr[inum] == -1) return -1;

    //read in the inode
    inode_t inode;
    lseek(fd, inode_locs.inodeArr[inum], SEEK_SET);
    read(fd, &inode, sizeof(inode_t));

    if(inode.stat.type != 1) return -1;

    if(inode.blockArr[blk] == -1) {
        int endLogTmp = CR.endLog;
        CR.endLog += BUFFER_SIZE;

        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(checkpoint_t));

        inode.blockArr[blk] = endLogTmp;
        inode.stat.size =  (blk + 1) * 4096;
        lseek(fd, inode_locs.inodeArr[inum], SEEK_SET);
        write(fd, &inode, sizeof(inode_t));
        lseek(fd, endLogTmp, SEEK_SET);
        write(fd, buff, BUFFER_SIZE);
    }
    else {
        inode.stat.size = (blk + 1) * 4096;
        lseek(fd, inode_locs.inodeArr[inum], SEEK_SET);
        write(fd, &inode, sizeof(inode_t));
        lseek(fd, inode.blockArr[blk], SEEK_SET);
        write(fd, buff, BUFFER_SIZE);
    }
    load_inode_loc();
    return 0;
}

int server_unlink(int pinum, char *name) {
    if(pinum < 0 || pinum > 4096) return -1;
    if(inode_locs.inodeArr[pinum] == -1) return -1;
    //test to see if name is equivalent to parent or current directory
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -1;
    if(strlen(name) > 28 || strlen(name) < 0) return -1;

    inode_t pInode;
    lseek(fd, inode_locs.inodeArr[pinum], SEEK_SET);
    read(fd, &pInode, sizeof(inode_t));
    if(pInode.stat.type != MFS_DIRECTORY) return -1;

    int found = 0;
    int delDirBlkLoc;
    int delInd;
    int delInodeLoc;
    dir_t dirBlk;
    for(int i = 0; i < 14; i++) {
        if(pInode.blockArr[i] >= 0) {
            lseek(fd, pInode.blockArr[i], SEEK_SET);
            read(fd, &dirBlk, sizeof(dir_t));
            for(int j = 0; j < 128; j++) {
                if(strcmp(dirBlk.dirArr[j].name, name) == 0) {
                    found = 1;
                    delInd = j;
                    delInodeLoc = inode_locs.inodeArr[dirBlk.dirArr[j].inum];
                    delDirBlkLoc = pInode.blockArr[i];
                    break;
                }
            }
            if (found == 1) break;
        }
    }

    if(found == 0) return 0; // name not existing is not a failure

    inode_t inodeDel;
    lseek(fd, delInodeLoc, SEEK_SET);
    read(fd, &inodeDel, sizeof(inodeDel));

    if(inodeDel.stat.type == MFS_DIRECTORY && inodeDel.stat.size > 2 * BUFFER_SIZE) {
        return -1; // this is not an empty directory
    }

    del_inode_imap(delInodeLoc);

    dirBlk.dirArr[delInd].inum = -1;
    sprintf(dirBlk.dirArr[delInd].name, "x");
    lseek(fd, delDirBlkLoc, SEEK_SET);
    write(fd, &dirBlk, sizeof(dir_t));

    int sizeInd = -1;
    for(int i = 0; i < 14; i++) {
        if(pInode.blockArr[i] != -1) {
            sizeInd = i;
        }
    }
    //update parent directory's size
    pInode.stat.size = (sizeInd + 1) * BUFFER_SIZE;
    lseek(fd, inode_locs.inodeArr[pinum], SEEK_SET);
    write(fd, &pInode, sizeof(inode_t));

    load_inode_loc();
    return 0;
}

int del_inode_imap(int inum) {
    int imapInd = inum / 16;
    int imapInIndex = inum % 16;

    imap_t imapTmp;
    lseek(fd, CR.imap[imapInd], SEEK_SET);
    read(fd, &imapTmp, sizeof(imap_t));

    imapTmp.inodeArr[imapInIndex] = -1;
    int empty = 0;
    for (int j = 0; j < 16; ++j) {
        if (imapTmp.inodeArr[j] > 0){
            empty ++;
        }
    }
    if(empty == 0) { // this imap is empty, all -1, delete the imap
        CR.imap[imapInd] = -1;
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(checkpoint_t));
    }
    else {
        lseek(fd, CR.imap[imapInd], SEEK_SET);
        write(fd, &imapTmp, sizeof(imapTmp));
    }
    return 0;
}

int server_stat(int inum, MFS_Stat_t *m) {
    if(inum < 0 || inum >= 4096 ) return -1;
    if (fd < 0) return -1;
    load_inode_loc();

    if(inode_locs.inodeArr[inum] == -1) return -1;

    inode_t inode;
    lseek(fd, inode_locs.inodeArr[inum], 0);
    read(fd, &inode, sizeof(inode_t));

    m->type = inode.stat.type;
    m->size = inode.stat.size;

    return 0;
}

int server_lookup(int pinum, char *name) {
    if(pinum < 0 || pinum >= 4096) return -1;
    if(strlen(name) < 1 || strlen(name) > 28) return -1;
    load_inode_loc();
    //check if parent inode number  is valid
    if(inode_locs.inodeArr[pinum] == -1) return -1;

    //read the parent inode
    inode_t pInode;
    lseek(fd, inode_locs.inodeArr[pinum], SEEK_SET);
    read(fd, &pInode, sizeof(pInode));
    if(pInode.stat.type != MFS_DIRECTORY) return -1;

    int dirBlk;
    for(int i = 0; i < 14; i++) {
        dirBlk = pInode.blockArr[i];
        lseek(fd, dirBlk, SEEK_SET);
        dir_t dirBlkTmp;
        read(fd, &dirBlkTmp, sizeof(dir_t));
        for(int j = 0; j < 128; j++) {
            // find the matched name and return
            if(strcmp(dirBlkTmp.dirArr[j].name, name) == 0) {
                return dirBlkTmp.dirArr[j].inum;
            }
        }
    }
    return -1;
}

int server_create(int pinum, int type, char *name) {
    if(pinum < 0 || pinum > 4096) return -1;
    if(type != MFS_DIRECTORY && type != MFS_REGULAR_FILE) return -1;
    if(strlen(name) < 1 || strlen(name) >= 28) return -1;
    if(inode_locs.inodeArr[pinum] == -1) return -1;
    if(server_lookup(pinum, name) >= 0) return 0; // name exist, return success

    //get pinum location
    inode_t pInode;
    lseek(fd, inode_locs.inodeArr[pinum], SEEK_SET);
    read(fd, &pInode, sizeof(inode_t));

    if(pInode.stat.type != MFS_DIRECTORY) return -1;
    if(pInode.stat.size >= (BUFFER_SIZE * 14 * 128)) return -1;

    int newInum = create_inode_imap(pinum, type); // create a new inode and new imap and return inode number
    int iDirBlkInd = pInode.stat.size / (BUFFER_SIZE * 128);
    if(iDirBlkInd > 14) return -1;

    int dirBlkInd = (pInode.stat.size/(BUFFER_SIZE) % 128);

    pInode.stat.size += BUFFER_SIZE;

    // if full, add new block for directory
    if(dirBlkInd == 0) {
        pInode.blockArr[iDirBlkInd] = CR.endLog;
        dir_t newDirBlk;
        for(int i = 0; i < 128; i++) {
            sprintf(newDirBlk.dirArr[i].name, "x");
            newDirBlk.dirArr[i].inum = -1;
        }

        lseek(fd, CR.endLog, SEEK_SET);
        write(fd, &newDirBlk, sizeof(dir_t));
        // update checkpoint region
        CR.endLog += BUFFER_SIZE;
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(checkpoint_t));
    }

    lseek(fd, inode_locs.inodeArr[pinum], SEEK_SET);
    write(fd, &pInode, sizeof(pInode));

    lseek(fd, pInode.blockArr[iDirBlkInd], SEEK_SET);
    dir_t dirBlk;
    read(fd, &dirBlk, sizeof(dir_t));

    sprintf(dirBlk.dirArr[dirBlkInd].name, "x");
    sprintf(dirBlk.dirArr[dirBlkInd].name, name, 28);
    dirBlk.dirArr[dirBlkInd].inum = newInum;

    //write to updated directory
    lseek(fd, pInode.blockArr[iDirBlkInd], 0);
    write(fd, &dirBlk, sizeof(dir_t));

    load_inode_loc();
    return 0;
}

int create_inode_imap(int pinum, int type) {
    load_inode_loc();
    // find first empty inode
    int nInodeNum = -1;
    for(int i = 0; i < 4096; i++) {
        if(inode_locs.inodeArr[i] == -1) {
            nInodeNum = i;
            break;
        }
    }
    if (nInodeNum == -1) return -1;

    int imapInd = nInodeNum / 16;
    int nImapInd = nInodeNum % 16;

    if(nImapInd == 0) { // need to create a new imap
        int emptyMapNum = -1;
        for(int i = 0; i < 256; i++) {
            if(CR.imap[i] == -1) {
                emptyMapNum = i;
                break;
            }
        }
        if (emptyMapNum == -1) return -1;

        CR.imap[emptyMapNum] = CR.endLog;
        imap_t nimap;
        for(int i = 0; i < 16; i++) {
            nimap.inodeArr[i] = -1;
        }

        CR.endLog += sizeof(imap_t);
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(checkpoint_t));
        lseek(fd, CR.imap[emptyMapNum], SEEK_SET);
        write(fd, &nimap, sizeof(imap_t));
    }

    imap_t imapTmp;
    lseek(fd, CR.imap[imapInd], SEEK_SET);
    read(fd, &imapTmp, sizeof(imap_t));

    imapTmp.inodeArr[nImapInd] = CR.endLog;
    lseek(fd, CR.imap[imapInd], SEEK_SET);
    write(fd, &imapTmp, sizeof(imap_t));

    inode_t nInode;
    nInode.stat.type = type;
    for(int i = 0; i < 14; i++) {
        nInode.blockArr[i] = -1;
    }

    nInode.blockArr[0] = CR.endLog + sizeof(inode_t);
    if(type == MFS_DIRECTORY) {
        nInode.stat.size = 2 * BUFFER_SIZE;
        lseek(fd, CR.endLog, SEEK_SET);
        write(fd, &nInode, sizeof(inode_t));
        CR.endLog += sizeof(nInode);

        dir_t dirBlk;
        for(int i = 0; i < 128; i++) {
            dirBlk.dirArr[i].inum = -1;
            sprintf(dirBlk.dirArr[i].name, "x");
        }

        dirBlk.dirArr[0].inum = nInodeNum;
        sprintf(dirBlk.dirArr[0].name, ".");
        dirBlk.dirArr[1].inum = pinum;
        sprintf(dirBlk.dirArr[1].name, "..");

        write(fd, &dirBlk, sizeof(dir_t));
        CR.endLog += sizeof(dir_t);
    } else {
        nInode.stat.size = 0;
        lseek(fd, CR.endLog, SEEK_SET);
        write(fd, &nInode, sizeof(inode_t));
        CR.endLog += sizeof(nInode);

        char *nBlk = malloc(BUFFER_SIZE);
        write(fd, nBlk, BUFFER_SIZE);
        free(nBlk);
        CR.endLog += BUFFER_SIZE;
    }

    // updated checkpoint region
    lseek(fd, 0, SEEK_SET);
    write(fd, &CR, sizeof(checkpoint_t));
    load_inode_loc();
    return nInodeNum;
}

int server_shutDown() {
    close(fd);
    exit(0);
    return 0;
}