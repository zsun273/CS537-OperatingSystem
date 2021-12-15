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

// struct for the checkpoint region
typedef struct __MFS_CR_t {
    int imap[256];
    int end;        //end of the log
} MFS_CR_t;

// struct for inode
typedef struct __MFS_inode_t {
    MFS_Stat_t stat;
    int block_loc[14];
} MFS_inode_t;

// struct for inode map pieces
// one inode map piece can point to 16 inodes
typedef struct __MFS_imap_t {
    int inode_loc[16];
} MFS_imap_t;

// struct for directories
// one directory block can have 128 entries
typedef struct __MFS_Dir_t {
    MFS_DirEnt_t dir_ent_loc[128];
} MFS_Dir_t;

// struct to store all inode locations
typedef struct __MFS_inode_loc_t {
    int inode_loc[4096];
} MFS_inode_loc_t;

//struct for different library calls
typedef enum __MFS_Lib_t {
    INIT,
    LOOKUP,
    STAT,
    WRITE,
    READ,
    CREAT,
    UNLINK,
    SHUTDOWN
} MFS_Lib_t;

//message struct, send between ports.
typedef struct __MFS_Msg_t {
    char name[28];
    char buffer[4096];
    int inum;
    int pinum;
    int block;
    int returnNum;
    MFS_Stat_t stat;
    MFS_Lib_t lib;
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

