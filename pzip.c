// CS537 P3a: Group Memeber: Zhuocheng Sun & Gefei Shen
// handin version
// 11/04 2021

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sysinfo.h>

int n_thread = 0;

typedef struct zip_struct {
    char *ptr;
    int offset;
    int len;
} zip_struct;

typedef struct zip_output {
    char   val[10000000];
    int    num[10000000];
    int    n;
} zip_output;

void check_error(int return_value){
    if (return_value < 0){
        printf("Failed\n");
        exit(1);
    }
}

void *thread_zip(void *args) {
    zip_output* ret = malloc(sizeof(zip_output));
    ret->n = 0;
    zip_struct *arg = (zip_struct *) args;
    char *ptr = arg->ptr;
    int offset = arg->offset;
    int len = arg->len;
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
        }
    }
    // for the last char
    ret->val[ret->n] = ch;
    ret->num[ret->n] = nch;

    return (void*)ret;
}

void merge(char* ch, int* nch, zip_output* rets[], int n_thread, int first) {
    if (first == 1) {
        (*ch) = rets[0]->val[0];
        (*nch) = 0;
    }

    for (int i=0; i < n_thread; i++) {
        for (int j = 0; j <= rets[i]->n; j++) {
            if ((*ch) == rets[i]->val[j]) {
                (*nch) += rets[i]->num[j];
            } else {
                //printf("write[num: %d, val: %c]\n", (*nch), (*ch));
                fwrite(nch, sizeof(int), 1 ,stdout);
                fwrite(ch, sizeof(char), 1 ,stdout);
                (*ch) = rets[i]->val[j];
                (*nch) = rets[i]->num[j];
            }
        }
    }
}

void process(char* ch, int* nch, char *path, int first) {
    int fd = open(path, O_RDONLY);
    check_error(fd);

    struct stat st;
    check_error(fstat(fd, &st));

    int filesize = st.st_size;
    int offset = 0;
    char *ptr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, offset);
    if (ptr == MAP_FAILED) {
        printf("mmap fails\n");
        exit(1);
    }
    int len;
    pthread_t threads[n_thread];
    zip_struct args[n_thread];

    // divide the job to n_threads.
    for (int i = 0; i < n_thread; i++) {
        len = ((i+1) * filesize / n_thread) - offset;
        args[i] = (zip_struct){ptr, offset, len};
        pthread_create(&threads[i], NULL, thread_zip, &args[i]);
        offset += len;
    }

    zip_output* rets[n_thread];
    for (int i = 0; i < n_thread; i++) {
        pthread_join(threads[i], (void*)(&rets[i]));
    }

    merge(ch, nch, rets, n_thread, first);

    for (int i = 0; i < n_thread; i++) {
        free(rets[i]);
    }
    check_error(munmap(ptr, filesize));
    close(fd);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("zip: file1 [file2 ...]\n");
        exit(1);
    }
    n_thread = get_nprocs_conf();
    // Process the first file
    char ch;
    int nch = 0;
    process(&ch, &nch, argv[1], 1);

    for (int i = 2; i < argc; i++) {
        process(&ch, &nch, argv[i], 0);
    }
    //printf("write[num: %d, val: %c]\n", nch, ch);
    fwrite(&nch, sizeof(int), 1 ,stdout);
    fwrite(&ch, sizeof(char), 1 ,stdout);
    return 0;
}