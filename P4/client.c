#include <stdio.h>
#include "mfs.h"
#include "udp.h"

#define BUFFER_SIZE (4096)

// client code, fill in an address before sending messages
int main(int argc, char *argv[]) {
    struct sockaddr_in addrSnd, addrRcv;

    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char message[sizeof(MFS_Msg_t)];
    sprintf(message, "defined type struct");

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, message, sizeof(MFS_Msg_t));
    if (rc < 0) {
	    printf("client:: failed to send\n");
	    exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, sizeof(MFS_Msg_t));
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);

    int rc = MFS_Init("royal-01.cs.wisc.edu", 10000);

    return 0;
}

