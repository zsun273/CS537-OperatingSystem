#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

//struct for the checkpoint region
typedef struct checkpoint_t {
    int imap[256];
    int endLog; //end of the log
} checkpoint_t;

//struct for inode
typedef struct inode_t {
    MFS_Stat_t stat;
    int blockArr[14];
} inode_t;

// struct for inode map pieces
// one inode map piece can point to 16 inodes
typedef struct imap_t {
    int inodeArr[16];
} imap_t;

//struct for directories
// one directory block can have 128 entries
typedef struct dir_t {
    MFS_DirEnt_t dirArr[128];
} dir_t;

// struct to store all inode locations
typedef struct inodeArr_t {
    int inodeArr[4096];
} inodeArr_t;

//for different library calls
typedef enum lib_t {
    INIT,
    LOOKUP,
    STAT,
    WRITE,
    READ,
    CREAT,
    UNLINK,
    SHUTDOWN
} lib_t;

//message struct, send between ports.
typedef struct msg_t {
    char name[28];
    char buffer[4096];
    int inum;
    int block;
    int type;
    int returnNum;
    int pinum;
    MFS_Stat_t stat;
    lib_t lib;
} msg_t;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__

