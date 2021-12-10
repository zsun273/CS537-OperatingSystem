#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

#define BUFFER_SIZE (4096)

int init_image(char *name); // function to initialize the file image
int load_mem();             // load check point, inode map into memory


char name[256]; // TODO: not sure about this size
MFS_checkpoint_t CR;
MFS_imap_t imap;

// server code
int main(int argc, char *argv[]) {

    if (argc != 3){
        fprintf(stderr, "Usage: server <port> <file image>\n");
        exit(0);
    }
    int port_num = atoi(argv[1]);
    sprintf(name, argv[2]);       // assign file image to name

    int sd = UDP_Open(port_num);  // use a port-number to open
    assert(sd > -1);

    // initialize the image file
    init_image(name);
    load_mem();

    while (1) {
	    struct sockaddr_in addr;
        char message[sizeof(MFS_Msg_t)];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, sizeof(MFS_Msg_t));
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
            char reply[sizeof(MFS_Msg_t)];
            rc = UDP_Write(sd, &addr, reply, sizeof(MFS_Msg_t));
            printf("server:: reply\n");
        }
    }
    return 0; 
}
    
int init_image(char* name){
    int fd = open(name, O_RDWR); // open an existing file with r/w access

    if (fd < 0){                 // filr not exist, create and initialize
        fd = open(name, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        // create checkpoint region, initial inode map, create a single root directory with . and ..
        // first initialize two directory entries
        MFS_DirEnt_t self_dir;
        MFS_DirEnt_t parent_dir;
        self_dir.inum = 0;
        sprintf(self_dir.name, ".\0");
        parent_dir.inum = 0;
        sprintf(parent_dir.name, "..\0")

        // then initialize inode for root directory
        MFS_inode_t root;
        root.type = MFS_DIRECTORY;
        for (int i = 0; i < 14; ++i) {
            MFS_DirEnt_t dir_entry;
            sprintf(dir_entry.name, "\0");
            dir_entry.inum = -1;
            root.blockArr[i] = dir_entry;
        }
        root.blockArr[0] = self_dir;
        root.blockArr[1] = parent_dir;

        // then initialize the inode map
        for (int i = 0; i < 16; ++i) {
            imap.inodeArr[i] = -1;
        }
        imap.inodeArr[0] = root;

        // then initialize the checkpoint region
        for (int i = 0; i < 256; ++i) {
            CR.imap[i] = -1;
        }
        CR.imap[0] = imap;
        CR.endOfLog = sizeof(CR) + sizeof(imap) + sizeof(root) + sizeof(self_dir) + sizeof(parent_dir);

        lseek(fd, 0, SEEK_SET); // set the file offset to 0
        write(fd, &CR, sizeof(MFS_checkpoint_t));
        write(fd, &imap, sizeof(MFS_imap_t));
        write(fd, &root, sizeof(MFS_inode_t));
        write(fd, &self_dir, sizeof(MFS_DirEnt_t));
        write(fd, &parent_dir, sizeof(MFS_DirEnt_t));
    }
    else{
        read(fd, &CR, sizeof(MFS_checkpoint_t));
    }

    return 0;
}

int load_mem(){
    return 0;
}
 

#pragma clang diagnostic pop