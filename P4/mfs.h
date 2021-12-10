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

//struct for checkpoint region, pointing to 256 pieces of inode map
typedef struct __MFS_checkpoint_t {
    int imap[256];  // point to inode map pieces
    int endOfLog;
} MFS_checkpoint_t;

//struct for inode map
typedef struct __MFS_imap_t {
    int inodeArr[16]; // an array indexed by inode number
} MFS_imap_t;

//struct for inode
typedef struct __MFS_inode_t {
    int type;
    int size;
    int blockArr[14]; // point to 4KB block
} MFS_inode_t;

//struct for directories blocks, one block can fit 128 directories
typedef struct __MFS_dir_t {
    MFS_DirEnt_t dirArr[128];
} MFS_dir_t;

typedef enum __MFS_lib_t {
    INIT,
    LOOKUP,
    STAT,
    WRITE,
    READ,
    CREAT,
    UNLINK,
    SHUTDOWN
} MFS_lib_t;

// message struct to be sent between server and client
typedef  struct __MFS_Msg_t {
    char name[28]; //name of the directory
    char buffer[4096];
    int inum;
    int block;
    int type;
    int returnNum;
    int pinum;
    MFS_Stat_t stat;
    MFS_lib_t lib;
} MFS_Msg_t;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__
