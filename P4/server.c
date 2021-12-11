#include <stdio.h>
#include <string.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (4096)

int init_image(char *name); // function to initialize the file image
int load_mem();             // load check point, inode map into memory
int set_return_number();    // set the return number for message
int server_Lookup(int pinum, char *name);
int server_Stat(int inum, MFS_Stat_t *m);
int server_Write(int inum, char *buffer, int block);
int server_Read(int inum, char *buffer, int block);
int server_Creat(int pinum, int type, char *name);
int server_Unlink(int pinum, char *name);
int server_Shutdown();


char name[256]; // TODO: not sure about this size
MFS_checkpoint_t CR;
int fd;         // file descriptor
int sd;
struct sockaddr_in s;
MFS_inodeArr_t all_inodes;

// server code
int main(int argc, char *argv[]) {

    if (argc != 3){
        fprintf(stderr, "Usage: server <port> <file image>\n");
        exit(0);
    }
    int port_num = atoi(argv[1]);
    //sprintf(name, argv[2]);       // assign file image to name

    int sd = UDP_Open(port_num);  // use a port-number to open
    assert(sd > -1);

    // initialize the image file
    init_image(argv[2]);
    load_mem();

    while (1) {
	struct sockaddr_in addr;
        char message[sizeof(MFS_Msg_t)];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, sizeof(MFS_Msg_t));
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
            set_return_number(message); // TODO: Not sure about here
            rc = UDP_Write(sd, &addr, message, sizeof(MFS_Msg_t));
            printf("server:: reply\n");
        }
    }
    return 0; 
}
    
int init_image(char* name){
    fd = open(name, O_RDWR); // open an existing file with r/w access

    if (fd < 0){                 // file not exist, create and initialize
        fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        // create checkpoint region, initial inode map, create a single root directory with . and ..

        // initialize the checkpoint region
        for (int i = 0; i < 256; ++i) {
            CR.imap[i] = -1;
        }
        CR.imap[0] = sizeof(CR);   // this imap starts right after checkpoint region
        CR.endOfLog = sizeof(MFS_checkpoint_t) + sizeof(MFS_imap_t) + sizeof(MFS_inode_t) + sizeof(MFS_dir_t);

        // then initialize the inode map
        MFS_imap_t imap;
        for (int i = 0; i < 16; ++i) {
            imap.inodeArr[i] = -1;
        }
        imap.inodeArr[0] = sizeof(CR) + sizeof(imap); // first inode starts right after the imap

        // initialize inode for root directory
        MFS_inode_t root;
        root.type = MFS_DIRECTORY;
        root.size = 2*4096;
        for (int i = 0; i < 14; ++i) {
            root.blockArr[i] = -1;
        }
        root.blockArr[0] = sizeof(CR) + sizeof(imap) + sizeof(root);

        MFS_dir_t root_dir;
        for (int i = 0; i < 128; ++i) {
            root_dir.dirArr[i].inum = -1;
            sprintf(root_dir.dirArr[i].name, "x");
        }

        // initialize two directory entries
        root_dir.dirArr[0].inum = 0;
        sprintf(root_dir.dirArr[0].name, ".");
        root_dir.dirArr[1].inum = 0;
        sprintf(root_dir.dirArr[1].name, "..");

        lseek(fd, 0, SEEK_SET); // set the file offset to 0
        write(fd, &CR, sizeof(MFS_checkpoint_t));
        write(fd, &imap, sizeof(MFS_imap_t));
        write(fd, &root, sizeof(MFS_inode_t));
        write(fd, &root_dir, sizeof(MFS_dir_t));
    }
    else{ // file exist, read in existing checkpoint region
        read(fd, &CR, sizeof(MFS_checkpoint_t));
    }

    load_mem();

    return 0;
}

int load_mem(){
    lseek(fd, 0, SEEK_SET);
    read(fd, &CR, sizeof(MFS_checkpoint_t));

    for(int i = 0; i < 4096; i++) {
        all_inodes.inodeArr[i] = -1;
    }

    // store all inode info into all_inode array
    int k = 0;
    MFS_imap_t imap_copy;
    for(int i = 0; i < 256; i++) {
        if(CR.imap[i] >= 0) {
            lseek(fd, CR.imap[i], SEEK_SET);
            read(fd, &imap_copy, sizeof(MFS_imap_t));
            for(int j = 0; j < 16; j++) {
                if(imap_copy.inodeArr[j] >= 0) {
                    all_inodes.inodeArr[k] = imap_copy.inodeArr[j];
                }
                k++; // TODO: difference here
            }
        }
    }
    return 0;
}

int set_return_number(char* message){
    MFS_Msg_t *msg = (MFS_Msg_t*) message;
    msg->returnNum = -1;

    switch(msg->lib) {
        case INIT:
            msg->returnNum = init_image(name);
            break;
        case LOOKUP:
            msg->returnNum = server_Lookup(msg->pinum, msg->name);
            break;
        case STAT:
            msg->returnNum = server_Stat(msg->inum, &(msg->stat));
            break;
        case WRITE:
            msg->returnNum = server_Write(msg->inum, msg->buffer, msg->block);
            break;
        case READ:
            msg->returnNum = server_Read(msg->inum, msg->buffer, msg->block);
            break;
        case CREAT:
            msg->returnNum = server_Creat(msg->pinum, msg->type, msg->name);
            break;
        case UNLINK:
            msg->returnNum = server_Unlink(msg->pinum, msg->name);
            break;
        case SHUTDOWN:
            msg->returnNum = 0;
            memcpy(message, msg, sizeof(*msg));
            UDP_Write(sd, &s, message, sizeof(MFS_Msg_t));
            fsync(fd);
            close(fd);
            exit(0);
    }

    memcpy(message, msg, sizeof(*msg));
    return 0;
}

int server_Lookup(int pinum, char *name){
    if (pinum < 0 || pinum >= 4096) return -1;
    if (strlen(name) < 1 || strlen(name) > 28) return -1; // name can be at most 28 bytes
    load_mem();
    // check if parent inode number is valid
    if (all_inodes.inodeArr[pinum] == -1) return -1;

    // read the parent inode
    MFS_inode_t parent_inode;
    lseek(fd, all_inodes.inodeArr[pinum], SEEK_SET);
    read(fd, &parent_inode, sizeof(MFS_inode_t));
    if (parent_inode.type != MFS_DIRECTORY) return -1;

    int dir_data_addr;
    for (int i = 0; i < 14; ++i) {
        dir_data_addr = parent_inode.blockArr[i];
        lseek(fd, dir_data_addr, SEEK_SET);
        MFS_dir_t dir_block;
        read(fd, &dir_block, sizeof(MFS_dir_t));

        for (int j = 0; j < 128; ++j) {
            // find the matched name and return
            if (strcmp(dir_block.dirArr[j].name, name) == 0){
                return dir_block.dirArr[j].inum;
            }
        }
    }

    return -1;
}

int server_Stat(int inum, MFS_Stat_t *m){
    if (inum < 0 || inum >= 4096) return -1;
    if (fd < 0) return -1;
    load_mem();
    if (all_inodes.inodeArr[inum] == -1) return -1;

    // read in the inode
    MFS_inode_t inode;
    lseek(fd, all_inodes.inodeArr[inum], SEEK_SET);
    read(fd, &inode, sizeof(MFS_inode_t));

    m->size = inode.size;
    m->type = inode.type;

    return 0;
}

int server_Write(int inum, char *buffer, int block){
    if (inum < 0 || inum >= 4096) return -1;
    if (block < 0 || block > 16) return -1;

    load_mem();
    if (all_inodes.inodeArr[inum] == -1) return -1;

    // read in the inode
    MFS_inode_t inode;
    lseek(fd, all_inodes.inodeArr[inum], SEEK_SET);
    read(fd, &inode, sizeof(MFS_inode_t));
    if (inode.type != MFS_REGULAR_FILE) return -1;

    if(inode.blockArr[block] != -1){ // data exist, we need to overwrite the data block
        lseek(fd, all_inodes.inodeArr[inum], SEEK_SET);
        write(fd, &inode, sizeof(MFS_inode_t));
        lseek(fd, inode.blockArr[block], SEEK_SET);
        write(fd, buffer, BUFFER_SIZE);
    }
    else{
        int end = CR.endOfLog;
        CR.endOfLog += BUFFER_SIZE;
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_checkpoint_t));

        inode.blockArr[block] = end;
        inode.size += BUFFER_SIZE;
        lseek(fd, all_inodes.inodeArr[inum], SEEK_SET);
        write(fd, &inode, sizeof(MFS_inode_t));

        lseek(fd, CR.endOfLog, SEEK_SET);
        write(fd, buffer, BUFFER_SIZE);
    }

    load_mem();
    return 0;
}

int server_Read(int inum, char *buffer, int block){
    if (inum < 0 || inum >= 4096) return -1;
    if (block < 0 || block > 16) return -1;

    load_mem();
    if (all_inodes.inodeArr[inum] == -1) return -1;

    // read in the inode
    MFS_inode_t inode;
    lseek(fd, all_inodes.inodeArr[inum], SEEK_SET);
    read(fd, &inode, sizeof(MFS_inode_t));

    // read in the data block to buffer
    lseek(fd, inode.blockArr[block], SEEK_SET);
    read(fd, buffer, BUFFER_SIZE);

    return 0;
}

int server_Creat(int pinum, int type, char *name){
    return 0;
}

int server_Unlink(int pinum, char *name){
    return 0;
}

int server_Shutdown(){
    close(fd);
    exit(0);
    return 0;
}
