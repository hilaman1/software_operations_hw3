#include "message_slot.h"
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

// this code is based on CHARDEV1, CHARDEV2 files from recitation 6

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h> /* for GFP_KERNEL flag */

MODULE_LICENSE("GPL");


typedef struct channel{
    unsigned int channel_id;
    char current_message[BUF_LEN];
    size_t message_size;
    struct channel *next;
} channel;

typedef struct channel_list{
    channel *head;
} channel_list;

// a data structure to describe individual message slots
// (device files with different minor numbers)
typedef struct message_slot{
    int minor_number;
    unsigned int slot_invoked_channel_id;
    channel *slot_invoked_channel;
} message_slot;

struct chardev_info {
    spinlock_t lock;
};
static struct chardev_info device_info;


// there are 256 possible minor numbers
// each message_slot has a list of channels
// thus we will make a "global" array which contains
// a list of channels for each minor num

// we put it here and not in __init so it won't
// be deleted after module_init()
channel_list message_slots[257];

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
    int minor = iminor(inode);
    message_slot *new_slot = kmalloc(sizeof(message_slot), GFP_KERNEL);

    printk("Invoking device_open(%p)\n", file);
    if (new_slot == NULL){
        printk("device_open kmalloc failed(%p)\n", file);
        // the error of malloc and calloc on failure as mentioned here:
        // https://man7.org/linux/man-pages/man3/malloc.3.html
        return -ENOMEM;
    }
    new_slot->minor_number = minor;
    new_slot->slot_invoked_channel_id = 0;
    file->private_data = (void*) new_slot;
    return SUCCESS;
}


//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    message_slot *current_slot = (message_slot*) (file->private_data);
    int minor = current_slot->minor_number;
    channel *temp_head = message_slots[minor].head;
    char *current_message;
    unsigned int channel_id;
    int i;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    channel_id = (unsigned int) current_slot->slot_invoked_channel_id;
    if (channel_id == 0 || buffer == NULL){
        printk("line 142");
        return -EINVAL;
    }
    while (temp_head != NULL && temp_head->channel_id != channel_id) {
        temp_head = temp_head->next;
        printk("line 147");
    }
    if (temp_head == NULL){
        printk("line 149");
        return -EINVAL;
    }
    if (temp_head->message_size == 0){
        printk("line 153");
        return -EWOULDBLOCK;
    }
    if (length < temp_head->message_size){
        printk("line 157");
        return -ENOSPC;
    }
    current_message = temp_head->current_message;
    for(i = 0; i < temp_head->message_size; ++i ) {
        // we use get_user to check if this address is legal
        if (put_user(current_message[i], &buffer[i]) != 0){
        // get_user returns -EFAULT on error:
        // https://www.cs.bham.ac.uk/~exr/lectures/opsys/12_13/docs/kernelAPI/r3776.html
            return -EFAULT;
        }
    }

// return the number of input characters used
    return temp_head->message_size;
}


//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
    message_slot *current_slot = (message_slot*) (file->private_data);
    int minor = current_slot->minor_number;
    channel *temp_head = message_slots[minor].head;
//    char given_message[BUF_LEN];
    int channel_id;
    int i;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    channel_id = current_slot->slot_invoked_channel_id;
    if (channel_id == 0 || buffer == NULL){
        return -EINVAL;
    }
    while (temp_head != NULL && temp_head->channel_id != channel_id) {
        temp_head = temp_head->next;
    }
    if (temp_head == NULL || temp_head->channel_id != channel_id){
        return -EINVAL;
    }
    if (length != 0 && length <= BUF_LEN){
        for(i = 0; i < length; ++i ) {
        // we use get_user to check if this address is legal
            if (get_user(temp_head->current_message[i], &buffer[i]) != 0){
            // get_user returns -EFAULT on error:
            // https://www.cs.bham.ac.uk/~exr/lectures/opsys/12_13/docs/kernelAPI/r3776.html
                return -EFAULT;
            }
        }
        temp_head->message_size = length;
        return length;
    }else{
        return -EMSGSIZE;
    }
}

//----------------------------------------------------------------
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param ) {
    // we need to envoke the channel if it hasn't been envoked
    // if we add a new one, we need to add it sorted by the id_num
    message_slot *chosen_slot = (message_slot *) file->private_data;
    channel *slot_lst_head = message_slots[chosen_slot->minor_number].head;
    channel *temp_head;
    channel *new_channel;
    printk("Invoking ioctl: setting channel to %ld\n", ioctl_param);
    // Switch according to the ioctl called
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
        return -EINVAL;
    }

    if (slot_lst_head == NULL) {
        new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
        if (new_channel == NULL) {
            printk("device_ioctl kmalloc failed(%p)\n", file);
            // the error of malloc and calloc on failure as mentioned here:
            // https://man7.org/linux/man-pages/man3/malloc.3.html
            return -ENOMEM;
        }
        // list is empty, we don't need to search and we can add a
        // new channel at the start
        // update the chosen slot that this is the invoked channel
        new_channel->channel_id = ioctl_param;
        new_channel->next = NULL;
        new_channel->message_size = 0;
        // slot_lst_head->current_message is created while creating the object
        chosen_slot->slot_invoked_channel = new_channel;
        chosen_slot->slot_invoked_channel_id = ioctl_param;
        message_slots[chosen_slot->minor_number].head = new_channel;
        return SUCCESS;
    } else {
        temp_head = slot_lst_head;
        while (temp_head->next != NULL) {
            if (temp_head->channel_id == ioctl_param ) {
                // channel exists, update the slot that this is
                // the invoked channel
                chosen_slot->slot_invoked_channel = temp_head;
                chosen_slot->slot_invoked_channel_id = ioctl_param;
                return SUCCESS;
            }
            temp_head = temp_head->next;
        }
//       we got to the end of the list and didn't find the channel
//        we need to add it at the end
        new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
        if (new_channel == NULL) {
            printk("device_ioctl kmalloc failed(%p)\n", file);
           // the error of malloc and calloc on failure as mentioned here:
            // https://man7.org/linux/man-pages/man3/malloc.3.html
            return -ENOMEM;
        }
        new_channel->channel_id = ioctl_param;
        new_channel->message_size = 0;
        new_channel->next = NULL;
        chosen_slot->slot_invoked_channel = new_channel;
        chosen_slot->slot_invoked_channel_id = ioctl_param;
        temp_head->next = new_channel;
        return SUCCESS;
    }
}

//==================== DEVICE SETUP =============================
struct file_operations Fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
};

static int __init message_slot_init(void)
{
    // taken from CHARDEV2\chardev.c file from recitation 6
    int rc = -1;
    int j;
    // init dev struct
    memset( &device_info, 0, sizeof(struct chardev_info) );
    spin_lock_init( &device_info.lock );

    // Register driver capabilities. Obtain major num
    rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );
    // Negative values signify an error
    if( rc < 0 ) {
        printk( KERN_ERR "%s registraion failed for  %d\n",
                DEVICE_FILE_NAME, MAJOR_NUM );
        return rc;
    }
//  initiate an empty list for each possible message_slot
//  with minor number 0<=i<=256
    for (j = 0; j < 257; ++j) {
        message_slots[j].head = NULL;
    }
    return SUCCESS;
}

static void __exit message_slot_cleanup(void)
{
    // free all the allocated memory (list for each message_slot device)
    channel *temp_head;
    channel *head;
    int i;
    for (i = 0; i < 257; ++i) {
        head = message_slots[i].head;
        while(head != NULL){
            temp_head = head;
            head = head->next;
            kfree(temp_head);
        }
    }
    // Unregister the device
    // Should always succeed
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------

module_init(message_slot_init);
module_exit(message_slot_cleanup);
//========================= END OF FILE =========================
