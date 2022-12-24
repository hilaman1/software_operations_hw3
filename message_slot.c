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
    int message_size;
    struct channel *next;
    struct channel *prev;
} channel;

typedef struct channel_list{
    channel *head;
    channel *tail;
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
// ** this list will be sorted on-line by id-num

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
//static ssize_t device_read( struct file* file,
//                            char __user* buffer,
//                            size_t       length,
//                            loff_t*      offset )
//{
//
//    int i;
//    message_slot *current_slot = (message_slot*)(file->private_data);
//    int minor = current_slot->minor_number;
//    channel *temp_head = message_slots[minor].head;
//    int channel_id;
//    char the_message[BUF_LEN];
//    int i,j;
//    printk( "Invocing device_read(%p,%ld)\n",file, length);
//    channel_id = current_slot->slot_invoked_channel_id;
//    if (channel_id == 0){
//        return -EINVAL;
//    }
//    while (temp_head != NULL && temp_head->channel_id != channel_id) {
//        temp_head = temp_head->next;
//    }
//    if (temp_head == NULL){
//        return -EINVAL;
//    }
//    if (current_slot->slot_invoked_channel_id != 0 && buffer != NULL){
//        if (last_message_size != 0){
//            if (length >= last_message_size){
//                for(i = 0; i < last_message_size; ++i){
//                    // get_user returns -EFAULT on error:
//                    // https://www.cs.bham.ac.uk/~exr/lectures/opsys/12_13/docs/kernelAPI/r3776.html
//                    if(put_user(the_message[i], &buffer[i]) != 0){
//                        return -EFAULT;
//                    }
//                    // we use put_user to check if this address is legal
//                }
//                // Returns the number of bytes read
//                return i;
//            }else{
//                return -ENOSPC;
//            }
//        }else{
//            return -EWOULDBLOCK;
//        }
//    }else{
//        return -EINVAL;
//    }
//}
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    message_slot *current_slot = (message_slot*) (file->private_data);
    int minor = current_slot->minor_number;
    channel *temp_head = message_slots[minor].head;
    char *current_message;
    int channel_id;
    int i;
    printk("Invoking device_read(%p,%ld)\n", file, length);
    channel_id = current_slot->slot_invoked_channel_id;
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
    for(i = 0; i < length; ++i ) {
        // we use get_user to check if this address is legal
        if (put_user(current_message[i], buffer + i) != 0){
        // get_user returns -EFAULT on error:
        // https://www.cs.bham.ac.uk/~exr/lectures/opsys/12_13/docs/kernelAPI/r3776.html
            return -EFAULT;
        }
    }
    printk("finish reading");

// return the number of input characters used
    return temp_head->message_size;
}

//static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset)
//{
//    message_slot *fData;
//    channel *ch;
//    fData = (message_slot *) file->private_data;
//    if (buffer == NULL || fData->slot_invoked_channel == 0){
//        return -EINVAL;
//    }
//    ch = fData->slot_invoked_channel;
//    if (ch->message_size == 0){
//        return -EWOULDBLOCK;
//    }
//    if (length < ch->message_size){
//        return -ENOSPC;
//    }
//    if (copy_to_user(buffer, ch->current_message, ch->message_size) != 0){
//        return -EIO;
//    }
//
//    printk("success in reading message, message len is %d\n", ch->message_size);
//    return ch->message_size;
//}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
    message_slot *current_slot = (message_slot*) (file->private_data);
    int minor = current_slot->minor_number;
    channel *temp_head = message_slots[minor].head;
    char given_message[BUF_LEN];
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
            if (get_user(given_message[i], &buffer[i]) != 0){
            // get_user returns -EFAULT on error:
            // https://www.cs.bham.ac.uk/~exr/lectures/opsys/12_13/docs/kernelAPI/r3776.html
                return -EFAULT;
            }
        }
        temp_head->message_size = length;
        for(i = 0; i < length; ++i){
            temp_head->current_message[i] = given_message[i];
        }
        // return the number of input characters used
        return length;
    }else{
        return -EMSGSIZE;
    }
}
//static ssize_t device_write( struct file* file, const char __user* buffer, size_t length, loff_t* offset)
//{
//    int i;
//    message_slot *data;
//    channel *ch;
//    char msg[BUF_LEN];
//    data = file->private_data;
//    ch = (channel*) data->slot_invoked_channel;
//
//    if (buffer == NULL){
//        return -EINVAL;
//    }
//    if (data->slot_invoked_channel == 0){
//        return -EINVAL;
//    }
//    if (length == 0 || length > BUF_LEN){
//        return -EMSGSIZE;
//    }
//    if (copy_from_user(msg, buffer, length) != 0){
//        return -EIO;
//    }
//
//    for (i = 0; i < BUF_LEN && i < length; i++){
//        ch->current_message[i] = msg[i];
//    }
//    ch->message_size = (int) length;
//    printk("success in writing message, message len is %d\n", i);
//    return i;
//
//}


//----------------------------------------------------------------
//static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param ) {
//    // we need to envoke the channel if it hasn't been envoked
//    // if we add a new one, we need to add it sorted by the id_num
//    message_slot *chosen_slot = (message_slot *) file->private_data;
//    channel *slot_lst_head = message_slots[chosen_slot->minor_number].head;
//    channel *slot_lst_tail = message_slots[chosen_slot->minor_number].tail;
//    channel *temp_head;
//    channel *new_channel;
//    printk("Invoking ioctl: setting channel to %ld\n", ioctl_param);
//    new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
//    if (new_channel == NULL) {
//        printk("device_ioctl kmalloc failed(%p)\n", file);
//        return -ENOMEM;
//    }
//    // Switch according to the ioctl called
//    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
//        return -EINVAL;
//    }
//
//    if (slot_lst_head == NULL) {
//        // list is empty, we don't need to search and we can add a
//        // new channel at the start
//        slot_lst_head = kmalloc(sizeof(channel), GFP_KERNEL);
//        if (slot_lst_head == NULL) {
//            printk("device_ioctl kmalloc failed(%p)\n", file);
//            // the error of malloc and calloc on failure as mentioned here:
//            // https://man7.org/linux/man-pages/man3/malloc.3.html
//            return -ENOMEM;
//        }
//        // update the chosen slot that this is the invoked channel
//        slot_lst_head->channel_id = ioctl_param;
//        slot_lst_head->next = NULL;
//        slot_lst_head->prev = NULL;
//        slot_lst_head->message_size = 0;
//        // slot_lst_head->current_message is created while creating the object
//        chosen_slot->slot_invoked_channel = slot_lst_head;
//        message_slots[chosen_slot->minor_number].head = slot_lst_head;
//        message_slots[chosen_slot->minor_number].tail = slot_lst_head;
//        return SUCCESS;
//    } else {
//        // list is not empty, we need to find if the channel exists
//        // if not, we need to add it to the list in the sorted place
//        if (ioctl_param < (slot_lst_head->channel_id)) {
//            new_channel->channel_id = ioctl_param;
//            new_channel->message_size = 0;
//            new_channel->next = slot_lst_head;
//            slot_lst_head->prev = new_channel;
//            new_channel->prev = NULL;
//            chosen_slot->slot_invoked_channel = new_channel;
//            chosen_slot->slot_invoked_channel_id = ioctl_param;
//            message_slots[chosen_slot->minor_number].head = new_channel;
//            return SUCCESS;
//        }
//        if (ioctl_param > slot_lst_tail->channel_id) {
//            new_channel->channel_id = ioctl_param;
//            new_channel->message_size = 0;
//            new_channel->prev = slot_lst_tail;
//            slot_lst_tail->next = new_channel;
//            new_channel->next = NULL;
//            chosen_slot->slot_invoked_channel = new_channel;
//            chosen_slot->slot_invoked_channel_id = ioctl_param;
//            message_slots[chosen_slot->minor_number].tail = new_channel;
//            return SUCCESS;
//        }
//        temp_head = slot_lst_head;
//        while (temp_head->next != NULL && temp_head->next->channel_id < ioctl_param) {
//            if (ioctl_param == (temp_head->channel_id)) {
//                // channel exists, update the slot that this is
//                // the invoked channel
//                chosen_slot->slot_invoked_channel = temp_head;
//                chosen_slot->slot_invoked_channel_id = ioctl_param;
//                return SUCCESS;
//            }
//            temp_head = temp_head->next;
//        }
//        new_channel->channel_id = ioctl_param;
//        new_channel->message_size = 0;
//        new_channel->next = temp_head->next;
//        new_channel->prev = temp_head;
//        temp_head->next->prev = new_channel;
//        temp_head->next = new_channel;
//        chosen_slot->slot_invoked_channel = new_channel;
//        chosen_slot->slot_invoked_channel_id = ioctl_param;
//        return SUCCESS;
//    }
//    return SUCCESS;
//}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    message_slot *data = (message_slot*)(file->private_data);
    channel *head = message_slots[data->minor_number].head;
    channel *temp;
    printk("Invoking ioctl: setting channel id to %ld\n", ioctl_param);
    if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
        printk("ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0)");
        return -EINVAL;
    }
    // find channel in list and set as active channel or add a new one if it does not exist
    if (head == NULL) {
        head = kmalloc(sizeof(channel), GFP_KERNEL);
        if (head==NULL) {
            printk("new channel malloc failed in ioctl");
            return -ENOMEM;
        }
        head->channel_id = ioctl_param;
        head->message_size = 0;
        head->next = NULL;
        data->slot_invoked_channel_id = ioctl_param;
        data->slot_invoked_channel = head;
        message_slots[data->minor_number].head=head;

        return SUCCESS;
    }
    else {
        temp = head;
        while (temp->next != NULL) {
            if (temp->channel_id== ioctl_param) {
                data->slot_invoked_channel_id = ioctl_param;
                data->slot_invoked_channel = temp;
                return SUCCESS;
            }
            temp = temp->next;
        }
        if (temp->channel_id == ioctl_param) {
            data->slot_invoked_channel_id = ioctl_param;
            data->slot_invoked_channel = temp;
            return SUCCESS;
        }
        temp->next = kmalloc(sizeof(channel), GFP_KERNEL);
        if (head==NULL) {
            printk("new channel malloc failed in ioctl");
            return -ENOMEM;
        }
        temp->next->channel_id = ioctl_param;
        temp->next->message_size = 0;
        temp->next->next = NULL;
        data->slot_invoked_channel_id = ioctl_param;
        data->slot_invoked_channel = temp->next;
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
