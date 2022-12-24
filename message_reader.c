#include "message_slot.h"
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */

int main(int argc, char** argv) {
    char the_message[BUF_LEN];
    char* message_slot_file_path;
    unsigned int target_message_channel_id;
    int ifp; /* file descriptor of message_slot */
    int returned_val;
    /* checking if the input is valid */
    if (argc == 3){ /* we include the program's name */
        message_slot_file_path = argv[1];
        target_message_channel_id = atoi(argv[2]);
    } else{
        perror("Invalid Input!");
        exit(1);
    }
    ifp = open(message_slot_file_path,O_RDWR);
    if (ifp < 0){
        perror("open() failed");
        exit(1);
    }
    returned_val = ioctl(ifp, MSG_SLOT_CHANNEL, target_message_channel_id);
    if (returned_val < 0){
        perror("ioctl() failed");
        exit(1);
    }
    // using the syscall read()
    returned_val = read(ifp, &the_message, BUF_LEN);
    if (returned_val < 0){
        perror("read() failed");
        exit(1);
    }
    close(ifp);
    // to print a message using write() system call,
    // we can write the message to the standard output file descriptor, which is '1'.
    returned_val = write(1, the_message, BUF_LEN);
    if (returned_val < 0){
        perror("Error writing to standard output");
        exit(1);
    }
    exit(0);
}
