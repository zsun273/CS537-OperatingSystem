#include <stdio.h>
#include "udp.h"
#include "mfs.h"


#define BUFFER_SIZE (4096)

int init_image(char *name); // function to initialize the file image
int load_mem();             // load check point, inode map into memory


char name[256]; // TODO: not sure about this size
MFS_checkpoint_t CR;

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
            // TODO: determine request type
            char reply[sizeof(MFS_Msg_t)];
            rc = UDP_Write(sd, &addr, reply, sizeof(MFS_Msg_t));
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
            sprintf(root_dir.dirArr[i].name, "\0");
        }

        // initialize two directory entries
        root_dir.dirArr[0].inum = 0;
        sprintf(root_dir.dirArr[0].name, ".\0");
        root_dir.dirArr[1].inum = 0;
        sprintf(root_dir.dirArr[1].name, "..\0")

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