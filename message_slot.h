// based on "chardev.h" from CHARDEV2 dir from recitation 6
#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>
#define MAJOR_NUM 235

// Set the message of the device driver
#define IOCTL_SET_ENC _IOWR(MAJOR_NUM, 0, unsigned long)

#define DEVICE_RANGE_NAME "message_slot"
// in write() we write a non-empty message of up to 128 bytes from the user’s buffer to the channel
#define BUF_LEN 128
#define DEVICE_FILE_NAME "simple_char_dev"
#define SUCCESS 0
#define FAILURE -1


#endif
