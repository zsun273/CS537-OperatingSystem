#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

// For MacOS
#include <sys/sysctl.h>

int nproc = 0;

int getNumberOfCores() {
    int nm[2];
    size_t len = 4;
    uint32_t count;

    nm[0] = CTL_HW;
    nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if (count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
        if (count < 1) {
        count = 1;
        }
    }
    return count;
}

typedef struct zip_arg_t {
    char *ptr;
    int offset;
    int len;
} zip_arg_t;

typedef struct zip_ret_t {
    char   val[10000000];
    int    num[10000000];
    int    n;
} zip_ret_t;

void *thread_zip(void *args) {
    zip_ret_t* ret = malloc(sizeof(zip_ret_t));
    ret->n = 0;

    char *ptr = ((zip_arg_t *)args)->ptr;
    int offset = ((zip_arg_t *)args)->offset;
    int len = ((zip_arg_t *)args)->len;
    char ch = *(ptr + offset);
    int nch = 0;

    for (int i = 0; i < len; i++) {
        if (*(ptr + offset + i) == ch) { // check if is repetition
            nch++;
        } else {
            ret->val[ret->n] = ch;
            ret->num[ret->n] = nch;
            ret->n ++;
            nch = 1;
            ch = *(ptr + offset + i);
            assert(ret->n < 10000000);
        }
    }

    // for the last char
    ret->val[ret->n] = ch;
    ret->num[ret->n] = nch; 

    return (void*)ret;
}

void merge(char* ch, int* nch, zip_ret_t* rets[], int nthread, bool first) {
    if (first) {
        (*ch) = rets[0]->val[0];
        (*nch) = 0; 
    }
    
    for (int i=0; i < nthread; i++) {
        for (int j = 0; j <= rets[i]->n; j++) {
            if ((*ch) == rets[i]->val[j]) {
                (*nch) += rets[i]->num[j]; 
            } else {
                //printf("Process write[num: %d, val: %c]\n", (*nch), (*ch));
                fwrite(nch, sizeof(int), 1 ,stdout);
                //fprintf(stdout, "%d", *nch);
                fwrite(ch, sizeof(char), 1 ,stdout);
                (*ch) = rets[i]->val[j];
                (*nch) = rets[i]->num[j];
            }
        }
    }
}

void process(char* ch, int* nch, char *path, bool first) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("zip: cannot open file\n");
        exit(1);
    }

    struct stat st;
    int err = fstat(fd, &st);
    if (err < 0) {
        printf("fstat fails\n");
        exit(1);
    }

    int filesize = st.st_size;
    int offset = 0;
    char *ptr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, offset);
    if (ptr == MAP_FAILED) {
        printf("mmap fails\n");
        exit(1);
    }

    int nthread = getNumberOfCores();
    int len;
    pthread_t threads[nthread];
    zip_arg_t args[nthread];

    // divide the job to n_threads.
    for (int i = 0; i < nthread; i++) {
        len = ((i+1) * filesize / nthread) - offset;
        args[i] = (struct zip_arg_t){ptr, offset, len};
        pthread_create(&threads[i], NULL, thread_zip, &args[i]);
        offset += len;
    }

    zip_ret_t* rets[nthread];
    for (int i = 0; i < nthread; i++) {
        pthread_join(threads[i], (void*)(&rets[i]));
    }

    merge(ch, nch, rets, nthread, first);

    for (int i = 0; i < nthread; i++) {
        free(rets[i]);
    }

    err = munmap(ptr, filesize);
    if (err != 0) {
        printf("munmap fails\n");
        exit(1);
    }

    close(fd);
}

int main(int argc, char *argv[]) {

    FILE *fp = open("file.z");
    fread(fp)

    if (argc < 2) {
        printf("zip: file1 [file2 ...]\n");
        exit(1);
    }

    nproc = getNumberOfCores(); // can be changed to get_nprocs_conf() on lab machine
    // Process the first file
    char ch;
    int nch = 0;
    process(&ch, &nch, argv[1], true);

    for (int i = 2; i < argc; i++) {
        process(&ch, &nch, argv[i], false);
    }
    
    //printf("write[num: %d, val: %c]\n", nch, ch);
    //fprintf(stdout, "%d", nch);
    fwrite(&nch, sizeof(int), 1 ,stdout);
    fwrite(&ch, sizeof(char), 1 ,stdout);
    return 0;
}