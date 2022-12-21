#include "message_slot.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>


int main(int argc, char** argv) {
    char* message_slot_file_path;
    unsigned int target_message_channel_id;
    char* message_to_pass;
    FILE *ifp; /* input_file pointer */
    int returned_val;
    /* checking if the input is valid */
    if (argc == 4){ /* we include the program's name and there are 4 arguments if max_iter is provided */
        message_slot_file_path = argv[1];
        target_message_channel_id = atoi(argv[2]);
        message_to_pass = argv[3];
    } else{
        perror("Invalid Input!");
        exit(1);
    }
    ifp = open(message_slot_file_path,O_RDWR);
    if (ifp < 0){
        perror("open failed");
        exit(1);
    }
    returned_val = ioctl(ifp, MSG_SLOT_CHANNEL, target_message_channel_id);
    if (returned_val < 0){
        perror("ioctl failed");
        exit(1);
    }
    int message_size = strlen(message_to_pass);
    returned_val = write(ifp, message_to_pass, target_message_channel_id);
    if (returned_val != message_size){
        perror("write failed");
        exit(1);
    }
    close(ifp);
    exit(0);
}
