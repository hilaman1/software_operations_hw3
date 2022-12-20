#include <stdio.h>
#include <stdlib.h>


int main(int argc, char** argv) {
    char* message_slot_file_path;
    int target_message_channel_id;
    char* message_to_pass;
    /* checking if the input is valid */
    if (argc == 4){ /* we include the program's name and there are 4 arguments if max_iter is provided */
        message_slot_file_path = argv[1];
        target_message_channel_id = atoi(argv[2]);
        message_to_pass = argv[3];
    } else{
        perror("Invalid Input!");
    }
}
