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
    int first_non_nul = 0;

    int nch = 0;
    for (int j = 0; j < len; j++) { // find the first non-nul char
        ch = *(ptr + offset + j);
        if (ch != '\0'){
            first_non_nul = j;
            break;
        }
        if (j == len-1){
            // all null
            return (void*)ret;
        }
    }

    for (int i = first_non_nul; i < len; i++) {
        char curr = *(ptr + offset + i);
        if (curr == '\0'){
            continue;
        }
        if (curr == ch) { // check if is repetition
            nch++;
        } else {
            ret->val[ret->n] = ch;
            ret->num[ret->n] = nch;
            ret->n ++;
            if (curr != '\0'){
                nch = 1;
                ch = curr;
            }

        }
    }
    if (ch != '\0'){
        // for the last char
        ret->val[ret->n] = ch;
        ret->num[ret->n] = nch;
    }else{
        ret->n --;
    }

    return (void*)ret;
}

void merge(char* ch, int* nch, zip_output* rets[], int n_thread, int first) {
    if (first == 1) {
        (*ch) = rets[0]->val[0];
        (*nch) = 0;
    }
    for (int i = 0; i < n_thread; i++) {
        for (int j = 0; j <= rets[i]->n; j++) {
            if (rets[i]->val[j] == '\0') {
                continue;
            }
            if ((*ch) == rets[i]->val[j]) {
                (*nch) += rets[i]->num[j];
            } else {
                //printf("write[num: %d, val: %c]\n", (*nch), (*ch));

                //DEBUG info
//                if (*ch == '\n') printf("thread: %d, number: %d, detect new line here\n", i, j);
//                else printf("thread: %d, number: %d\n", i, j);

//                if (*ch == '\0') {
//                    (*ch) = rets[i]->val[j];
//                    (*nch) = rets[i]->num[j];
//                    continue;
//                }

                fwrite(nch, sizeof(int), 1 ,stdout);
                fwrite(ch, sizeof(char), 1 ,stdout);
                (*ch) = rets[i]->val[j];
                (*nch) = rets[i]->num[j];
            }
        }
    }
}

int process(char* ch, int* nch, char *path, int first) {
    int fd = open(path, O_RDONLY);
    if (fd < 0){
        return 0;
    }

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

    int unit = 1 + filesize / n_thread;
    for (int i = 0; i < n_thread-1; i++) {
        len = ((i+1) * unit) - offset;
        args[i] = (zip_struct){ptr, offset, len};
        pthread_create(&threads[i], NULL, thread_zip, &args[i]);
        offset += len;
    }
    //last thread
    len = filesize - offset;
    args[n_thread-1] = (zip_struct){ptr, offset, len};
    pthread_create(&threads[n_thread-1], NULL, thread_zip, &args[n_thread-1]);

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
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("pzip: file1 [file2 ...]\n");
        exit(1);
    }
    n_thread = get_nprocs_conf() * 5;
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