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
int fd;

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
            set_return_number(message);
            rc = UDP_Write(sd, &addr, message, sizeof(MFS_Msg_t));
            printf("server:: reply\n");
        }
    }
    return 0; 
}
    
int init_image(char* name){
    int fd = open(name, O_RDWR); // open an existing file with r/w access

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
    return 0;
}

int set_return_number(char* message){
    msg_t *msg = (msg_t*) message;
    msg->returnNum = -1;

    int returnNum;
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
            msg->returnNum = server_Create(msg->pinum, msg->type, msg->name);
            break;
        case UNLINK:
            msg->returnNum = server_Unlink(msg->pinum, msg->name);
            break;
        case SHUTDOWN:
            msg->returnNum = 0;
            memcpy(message, msg, sizeof(*msg));
            UDP_Write(sd, &s, message, sizeof(msg_t));
            fsync(fd);
            close(fd);
            exit(0);
            break;
    }
    memcpy(message, msg, sizeof(*msg));
}

int server_Lookup(int pinum, char *name){
    return 0;
}

int server_Stat(int inum, MFS_Stat_t *m){
    return 0;
}

int server_Write(int inum, char *buffer, int block){
    return 0;
}

int server_Read(int inum, char *buffer, int block){
    return 0;
}

int server_Creat(int pinum, int type, char *name){
    return 0;
}

int server_Unlink(int pinum, char *name){
    return 0;
}

int server_Shutdown(){
    return 0;
}