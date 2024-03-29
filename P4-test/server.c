#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE 4096

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

struct sockaddr_in s;
int sd;                 // global socket descriptor
int fd;                 // global file descriptor
char name[256];
MFS_CR_t CR;        // checkpoint region for log
MFS_inode_loc_t inode_locs;  // all inode locations

int main(int argc, char *argv[]) {
    if(argc != 3) {
        fprintf(stderr, "Usage: server <port> <file image>\n");
        exit(0);
    }
    int port = atoi(argv[1]);
    sd = UDP_Open(port); // use a port number to open
    assert(sd > -1);

    //initialize the file system image
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
    // set the return number for each different library call
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
            msg->returnNum = server_create(msg->pinum, msg->stat.type, msg->name);
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
        CR.imap[0] = sizeof(MFS_CR_t); // imap starts right after checkpoint region
        CR.end = sizeof(MFS_CR_t) + sizeof(MFS_imap_t) + sizeof(MFS_inode_t) + sizeof(MFS_Dir_t);

        //initialize the inode map
        MFS_imap_t imap;
        for(int i = 0; i < 16; i++) {
            imap.inode_loc[i] = -1;
        }
        imap.inode_loc[0] = sizeof(MFS_CR_t) + sizeof(MFS_imap_t); // first inode starts right after the imap

        //initialize inode for root directory
        MFS_inode_t root;
        root.stat.type = MFS_DIRECTORY;
        root.stat.size = 2 * 4096;
        for(int i = 0; i < 14; i++) {
            root.block_loc[i] = -1;
        }
        root.block_loc[0] = sizeof(MFS_CR_t) + sizeof(MFS_imap_t) + sizeof(MFS_inode_t);

        //write first directory block
        MFS_Dir_t root_dir;
        for(int i = 0; i < 128; i++) {
            root_dir.dir_ent_loc[i].inum = -1;
            sprintf(root_dir.dir_ent_loc[i].name, "x");
        }

        // initialize two directory entries
        root_dir.dir_ent_loc[0].inum = 0;
        sprintf(root_dir.dir_ent_loc[0].name, ".");
        root_dir.dir_ent_loc[1].inum = 0;
        sprintf(root_dir.dir_ent_loc[1].name, "..");

        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_CR_t));
        write(fd, &imap, sizeof(imap));
        write(fd, &root, sizeof(MFS_inode_t));
        write(fd, &root_dir, sizeof(MFS_Dir_t));
    }
    else { //file exist, read in existing checkpoint region
        read(fd, &CR, sizeof(MFS_CR_t));
    }

    // initialize the inode location array
    for(int i = 0; i < 4096; i++) {
        inode_locs.inode_loc[i] = -1;
    }
    load_inode_loc();
    return 0;
}

int load_inode_loc() {
    lseek(fd, 0, SEEK_SET);
    read(fd, &CR, sizeof(MFS_CR_t));

    //store all inode start locations
    int idx = 0;
    MFS_imap_t imap_copy;
    for(int i = 0; i < 256; i++) {
        if(CR.imap[i] >= 0) {
            lseek(fd, CR.imap[i], SEEK_SET);
            read(fd, &imap_copy, sizeof(MFS_imap_t));
            for(int j = 0; j < 16; j++) {
                if(imap_copy.inode_loc[j] >= 0) {
                    inode_locs.inode_loc[idx] = imap_copy.inode_loc[j];
                }
                idx++;
            }
        }
    }
    return 0;
}

int server_read(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) return -1;
    if(blk > 13 || blk < 0) return -1;

    load_inode_loc();
    if(inode_locs.inode_loc[inum] == -1) return -1;

    // read in the inode
    MFS_inode_t inode;
    lseek(fd, inode_locs.inode_loc[inum], SEEK_SET);
    read(fd, &inode, sizeof(MFS_inode_t));

    if(inode.block_loc[blk] == -1) return -1;

    lseek(fd, inode.block_loc[blk], SEEK_SET);
    read(fd, buff, BUFFER_SIZE);
    return 0;
}

int server_write(int inum, char *buff, int blk) {
    if(inum >= 4096 || inum < 0) return -1;
    if(blk >= 14 || blk < 0) return -1;

    load_inode_loc();
    if(inode_locs.inode_loc[inum] == -1) return -1;

    //read in the inode
    MFS_inode_t inode;
    lseek(fd, inode_locs.inode_loc[inum], SEEK_SET);
    read(fd, &inode, sizeof(MFS_inode_t));

    if(inode.stat.type != MFS_REGULAR_FILE) return -1;

    if(inode.block_loc[blk] == -1) { // block is empty, write in
        int end_log = CR.end;
        CR.end += BUFFER_SIZE;

        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_CR_t));

        inode.block_loc[blk] = end_log;
        inode.stat.size =  (blk + 1) * 4096;
        lseek(fd, inode_locs.inode_loc[inum], SEEK_SET);
        write(fd, &inode, sizeof(MFS_inode_t));
        lseek(fd, end_log, SEEK_SET);
        write(fd, buff, BUFFER_SIZE);
    }
    else {                         // block is not empty, overwrite
        inode.stat.size = (blk + 1) * 4096;
        lseek(fd, inode_locs.inode_loc[inum], SEEK_SET);
        write(fd, &inode, sizeof(MFS_inode_t));
        lseek(fd, inode.block_loc[blk], SEEK_SET);
        write(fd, buff, BUFFER_SIZE);
    }
    load_inode_loc();
    return 0;
}

int server_unlink(int pinum, char *name) {
    if(pinum < 0 || pinum > 4096) return -1;
    if(inode_locs.inode_loc[pinum] == -1) return -1;
    // the directory name cannot be parent or current directory
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return -1;
    if(strlen(name) > 28 || strlen(name) < 0) return -1;

    // read in the parent inode
    MFS_inode_t parent_inode;
    lseek(fd, inode_locs.inode_loc[pinum], SEEK_SET);
    read(fd, &parent_inode, sizeof(MFS_inode_t));
    if(parent_inode.stat.type != MFS_DIRECTORY) return -1;

    // find all the locations to do delete
    int found = 0;
    int dir_blk_loc, idx_dir_arr, inode_loc;
    MFS_Dir_t dir_blk;
    for(int i = 0; i < 14; i++) {
        if(parent_inode.block_loc[i] >= 0) {
            lseek(fd, parent_inode.block_loc[i], SEEK_SET);
            read(fd, &dir_blk, sizeof(MFS_Dir_t));
            for(int j = 0; j < 128; j++) {
                if(strcmp(dir_blk.dir_ent_loc[j].name, name) == 0) {
                    found = 1;
                    idx_dir_arr = j;
                    inode_loc = inode_locs.inode_loc[dir_blk.dir_ent_loc[j].inum];
                    dir_blk_loc = parent_inode.block_loc[i];
                    break;
                }
            }
            if (found == 1) break;
        }
    }

    if(found == 0) return 0; // name not existing is not a failure

    MFS_inode_t inode_del;
    lseek(fd, inode_loc, SEEK_SET);
    read(fd, &inode_del, sizeof(inode_del));

    if(inode_del.stat.type == MFS_DIRECTORY && inode_del.stat.size > 2 * BUFFER_SIZE) {
        return -1; // this is not an empty directory, delete fail
    }

    del_inode_imap(inode_loc);

    dir_blk.dir_ent_loc[idx_dir_arr].inum = -1;
    sprintf(dir_blk.dir_ent_loc[idx_dir_arr].name, "x");
    lseek(fd, dir_blk_loc, SEEK_SET);
    write(fd, &dir_blk, sizeof(MFS_Dir_t));

    int last_blk = -1;
    for(int i = 0; i < 14; i++) {
        if(parent_inode.block_loc[i] != -1) {
            last_blk = i;
        }
    }
    //update parent directory
    parent_inode.stat.size = (last_blk + 1) * BUFFER_SIZE;
    lseek(fd, inode_locs.inode_loc[pinum], SEEK_SET);
    write(fd, &parent_inode, sizeof(MFS_inode_t));

    load_inode_loc();
    return 0;
}

int del_inode_imap(int inum) {
    int idx_of_imap = inum / 16;
    int idx_in_imap = inum % 16;

    MFS_imap_t imap_copy;
    lseek(fd, CR.imap[idx_of_imap], SEEK_SET);
    read(fd, &imap_copy, sizeof(MFS_imap_t));

    imap_copy.inode_loc[idx_in_imap] = -1;
    int empty = 0;
    for (int j = 0; j < 16; ++j) {
        if (imap_copy.inode_loc[j] > 0){
            empty ++;
        }
    }
    if(empty == 0) { // this imap is empty, all -1, delete the imap
        CR.imap[idx_of_imap] = -1;
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_CR_t));
    }
    else {
        lseek(fd, CR.imap[idx_of_imap], SEEK_SET);
        write(fd, &imap_copy, sizeof(imap_copy));
    }
    return 0;
}

int server_stat(int inum, MFS_Stat_t *m) {
    if(inum < 0 || inum >= 4096 ) return -1;
    if (fd < 0) return -1;
    load_inode_loc();

    if(inode_locs.inode_loc[inum] == -1) return -1;

    // read in the inode
    MFS_inode_t inode;
    lseek(fd, inode_locs.inode_loc[inum], 0);
    read(fd, &inode, sizeof(MFS_inode_t));

    m->type = inode.stat.type;
    m->size = inode.stat.size;
    return 0;
}

int server_lookup(int pinum, char *name) {
    if(pinum < 0 || pinum >= 4096) return -1;
    if(strlen(name) < 1 || strlen(name) > 28) return -1;
    load_inode_loc();
    if(inode_locs.inode_loc[pinum] == -1) return -1;

    //read in the parent inode
    MFS_inode_t parent_inode;
    lseek(fd, inode_locs.inode_loc[pinum], SEEK_SET);
    read(fd, &parent_inode, sizeof(parent_inode));
    if(parent_inode.stat.type != MFS_DIRECTORY) return -1;

    for(int i = 0; i < 14; i++) {
        lseek(fd, parent_inode.block_loc[i], SEEK_SET);
        MFS_Dir_t dir_blk;
        read(fd, &dir_blk, sizeof(MFS_Dir_t));
        for(int j = 0; j < 128; j++) {
            // find the matched name and return its inode number
            if(strcmp(dir_blk.dir_ent_loc[j].name, name) == 0) {
                return dir_blk.dir_ent_loc[j].inum;
            }
        }
    }
    return -1;
}

int server_create(int pinum, int type, char *name) {
    if(pinum < 0 || pinum > 4096) return -1;
    if(type != MFS_DIRECTORY && type != MFS_REGULAR_FILE) return -1;
    if(strlen(name) < 1 || strlen(name) >= 28) return -1;
    if(inode_locs.inode_loc[pinum] == -1) return -1;
    if(server_lookup(pinum, name) >= 0) return 0; // name exist, return success

    //read in the parent inode
    MFS_inode_t parent_inode;
    lseek(fd, inode_locs.inode_loc[pinum], SEEK_SET);
    read(fd, &parent_inode, sizeof(MFS_inode_t));

    if(parent_inode.stat.type != MFS_DIRECTORY) return -1;
    if(parent_inode.stat.size >= (BUFFER_SIZE * 14 * 128)) return -1;

    int new_inum = create_inode_imap(pinum, type); // create a new inode and new imap and return inode number
    int idx_of_dir_blk = parent_inode.stat.size / (BUFFER_SIZE * 128);
    if(idx_of_dir_blk > 14) return -1;

    int idx_in_dir_blk = (parent_inode.stat.size/(BUFFER_SIZE) % 128);

    parent_inode.stat.size += BUFFER_SIZE;

    // if full, add new block to the directory
    if(idx_in_dir_blk == 0) {
        parent_inode.block_loc[idx_of_dir_blk] = CR.end;
        MFS_Dir_t new_dir_blk;
        for(int i = 0; i < 128; i++) {
            sprintf(new_dir_blk.dir_ent_loc[i].name, "x");
            new_dir_blk.dir_ent_loc[i].inum = -1;
        }
        lseek(fd, CR.end, SEEK_SET);
        write(fd, &new_dir_blk, sizeof(MFS_Dir_t));
        CR.end += BUFFER_SIZE;
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_CR_t));
    }
    // update to disk
    lseek(fd, inode_locs.inode_loc[pinum], SEEK_SET);
    write(fd, &parent_inode, sizeof(parent_inode));

    lseek(fd, parent_inode.block_loc[idx_of_dir_blk], SEEK_SET);
    MFS_Dir_t dir_blk;
    read(fd, &dir_blk, sizeof(MFS_Dir_t));
    sprintf(dir_blk.dir_ent_loc[idx_in_dir_blk].name, "x");
    sprintf(dir_blk.dir_ent_loc[idx_in_dir_blk].name, name, 28);
    dir_blk.dir_ent_loc[idx_in_dir_blk].inum = new_inum;

    lseek(fd, parent_inode.block_loc[idx_of_dir_blk], 0);
    write(fd, &dir_blk, sizeof(MFS_Dir_t));

    load_inode_loc();
    return 0;
}

int create_inode_imap(int pinum, int type) {
    // find the first empty inode
    int first_empty_inum = -1;
    for(int i = 0; i < 4096; i++) {
        if(inode_locs.inode_loc[i] == -1) {
            first_empty_inum = i;
            break;
        }
    }
    if (first_empty_inum == -1) return -1;

    int idx_of_imap = first_empty_inum / 16;
    int idx_in_imap = first_empty_inum % 16;

    if(idx_in_imap == 0) { // need to create a new imap
        // find the first empty imap
        int first_empty_imap = -1;
        for(int i = 0; i < 256; i++) {
            if(CR.imap[i] == -1) {
                first_empty_imap = i;
                break;
            }
        }
        if (first_empty_imap == -1) return -1;

        CR.imap[first_empty_imap] = CR.end;
        MFS_imap_t new_imap;
        for(int i = 0; i < 16; i++) {
            new_imap.inode_loc[i] = -1;
        }
        CR.end += sizeof(MFS_imap_t);
        lseek(fd, 0, SEEK_SET);
        write(fd, &CR, sizeof(MFS_CR_t));
        lseek(fd, CR.imap[first_empty_imap], SEEK_SET);
        write(fd, &new_imap, sizeof(MFS_imap_t));
    }

    MFS_imap_t imap_copy;
    lseek(fd, CR.imap[idx_of_imap], SEEK_SET);
    read(fd, &imap_copy, sizeof(MFS_imap_t));
    imap_copy.inode_loc[idx_in_imap] = CR.end;
    lseek(fd, CR.imap[idx_of_imap], SEEK_SET);
    write(fd, &imap_copy, sizeof(MFS_imap_t));

    MFS_inode_t new_inode;
    new_inode.stat.type = type;
    for(int i = 0; i < 14; i++) {
        new_inode.block_loc[i] = -1;
    }

    new_inode.block_loc[0] = CR.end + sizeof(MFS_inode_t);
    if(type == MFS_DIRECTORY) {
        new_inode.stat.size = 2 * BUFFER_SIZE;
        lseek(fd, CR.end, SEEK_SET);
        write(fd, &new_inode, sizeof(MFS_inode_t));
        CR.end += sizeof(new_inode);

        MFS_Dir_t dir_blk;
        for(int i = 0; i < 128; i++) {
            dir_blk.dir_ent_loc[i].inum = -1;
            sprintf(dir_blk.dir_ent_loc[i].name, "x");
        }

        dir_blk.dir_ent_loc[0].inum = first_empty_inum;
        sprintf(dir_blk.dir_ent_loc[0].name, ".");
        dir_blk.dir_ent_loc[1].inum = pinum;
        sprintf(dir_blk.dir_ent_loc[1].name, "..");

        write(fd, &dir_blk, sizeof(MFS_Dir_t));
        CR.end += sizeof(MFS_Dir_t);
    } else { // type is regular file
        new_inode.stat.size = 0;
        lseek(fd, CR.end, SEEK_SET);
        write(fd, &new_inode, sizeof(MFS_inode_t));
        CR.end += sizeof(new_inode);

        char *new_blk = malloc(BUFFER_SIZE);
        write(fd, new_blk, BUFFER_SIZE);
        free(new_blk);
        CR.end += BUFFER_SIZE;
    }

    lseek(fd, 0, SEEK_SET);
    write(fd, &CR, sizeof(MFS_CR_t));
    load_inode_loc();
    return first_empty_inum;
}