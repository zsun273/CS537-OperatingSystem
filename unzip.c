#include<stdio.h>
#include<stdlib.h>

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("my-unzip: file1 [file2 ...]\n");
        exit(1);
    }
    else {
        for (int i  = 1; i < argc; i++) {
            FILE *fp = fopen (argv[i], "r");
            if (fp == NULL) {
                printf ("my-unzip: cannot open file\n");
                exit(1);
            }
            char ch[1000000];
            int buffer[1000000];
            while (fread(buffer, sizeof(int), 1, fp) == 1) {
                int count = *buffer;
                fread(ch, sizeof(char), 1, fp);
                for (int j = 0; j < count; j++) {
                    printf("%c", *ch);
                }
            }
        }
    }
    return 0;
}

