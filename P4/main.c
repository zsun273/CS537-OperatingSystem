#include <stdio.h>

#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("royal-01.cs.wisc.edu", 10000);
    printf("rc %d\n", rc);
    return rc;
}
