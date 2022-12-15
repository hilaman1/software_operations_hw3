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

typedef struct {
    channel *head;
    channel *tail;
} channel_list;

// a data structure to describe individual message slots
// (device files with different minor numbers)
typedef struct{
    int minor_number;
    unsigned int slot_invoked_channel_id;
    channel *slot_invoked_channel;
} message_slot;


// there are 256 possible minor numbers
// each message_slot has a list of channels
// thus we will make a "global" array which contains
// a list of channels for each minor num
// ** this list will be sorted on-line by id-num

// we put it here and not in __init so it won't
// be deleted after module_init()
channel_list message_slots[256];

struct chardev_info {
    spinlock_t lock;
};

// used to prevent concurent access into the same device
static int dev_open_flag = 0;

static struct chardev_info device_info;

// The message the device will give when asked
static char the_message[BUF_LEN];




//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
    unsigned long flags; // for spinlock
    printk("Invoking device_open(%p)\n", file);

    // We don't want to talk to two processes at the same time
    spin_lock_irqsave(&device_info.lock, flags);
    if( 1 == dev_open_flag ) {
        spin_unlock_irqrestore(&device_info.lock, flags);
        return -EBUSY;
    }

    ++dev_open_flag;
    spin_unlock_irqrestore(&device_info.lock, flags);
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
    // read doesnt really do anything (for now)
    printk( "Invocing device_read(%p,%ld) - "
            "operation not supported yet\n"
            "(last written - %s)\n",
            file, length, the_message );
    //invalid argument error
    return errno;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer
                             size_t             length,
                             loff_t*            offset)
{
    message_slot *data = (message_slot*)(file->private_data);
    ssize_t i;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    for(i = 0; i < length && i < BUF_LEN; ++i ) {
        get_user(the_message[i], &buffer[i]);
    }

    // return the number of input characters used
    return i;
}
//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    // Switch according to the ioctl called
    if( MSG_SLOT_CHANNEL == ioctl_command_id ) {
        if (ioctl_param == 0){
            return -EINVAL;
        }
        // we need to envoke the channel if it hasn't been envoked
        // if we add a new one, we need to add it sorted by the id_num
        message_slot *chosen_slot = (message_slot) file->private_data;
        channel *slot_lst_head = message_slots[chosen_slot->minor_number].head;
        if (slot_lst_head == NULL){
        // list is empty, we don't need to search and we can add a
        // new channel at the start
            slot_lst_head = kmalloc(sizeof(channel), GFP_KERNEL);
            if (slot_lst_head == NULL){
                // the error of malloc and calloc on failure as mentioned here:
                // https://man7.org/linux/man-pages/man3/malloc.3.html
                return -ENOMEM;
            }
            // update the chosen slot that this is the invoked channel
            slot_lst_head->channel_id = ioctl_param;
            slot_lst_head->next = NULL;
            slot_lst_head->prev = NULL;
            slot_lst_head->message_size = 0;
            // slot_lst_head->current_message is created while creating the object
            chosen_slot->slot_invoked_channel = slot_lst_head;
            message_slots[chosen_slot->minor_number].head = slot_lst_head;
            message_slots[chosen_slot->minor_number].tail = slot_lst_head;
            return SUCCESS;
        }else{
            // list is not empty, we need to find if the channel exists
            // if not, we need to add it to the list in the sorted place
            channel *temp_head = slot_lst_head;
            channel *temp_tail = slot_lst_head;

            if (ioctl_param < temp_head->channel_id){
                channel *new_channel;
                new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
                if (new_channel == NULL){
                    return -ENOMEM;
                }
                new_channel->channel_id = ioctl_param;
                new_channel->message_size = 0;
                temp_head->prev = new_channel;
                new_channel->next = temp_head;
                new_channel->prev = NULL;
                slot_lst_head = new_channel;
                chosen_slot->slot_invoked_channel = slot_lst_head;
                chosen_slot->slot_invoked_channel_id = ioctl_param;
                message_slots[chosen_slot->minor_number].head = slot_lst_head;
                return SUCCESS;
            }
            if (ioctl_param > temp_tail->channel_id){
                channel *new_channel;
                new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
                if (new_channel == NULL){
                    return -ENOMEM;
                }
                new_channel->channel_id = ioctl_param;
                new_channel->message_size = 0;
                temp_head->prev = new_channel;
                new_channel->next = NULL;
                new_channel->prev = temp_tail;
                slot_lst_head = new_channel;
                chosen_slot->slot_invoked_channel = slot_lst_head;
                chosen_slot->slot_invoked_channel_id = ioctl_param;
                return SUCCESS;
            }
            while (temp_head != NULL){
                if (ioctl_param == (temp_head->channel_id)){
                    // channel exists, update the slot this is
                    // the invoked channel
                    chosen_slot->slot_invoked_channel = temp_head;
                    chosen_slot->slot_invoked_channel_id = ioctl_param;
                    return SUCCESS;
                }
                if ((temp_head->prev->channel_id) < ioctl_param < (temp_head->channel_id)){
                    channel *temp_prev = temp_head->prev;
                    channel *new_channel;
                    new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
                    if (new_channel == NULL){
                        return -ENOMEM;
                    }
                    new_channel->channel_id = ioctl_param;
                    new_channel->message_size = 0;
                    new_channel->prev = temp_prev;
                    new_channel->next = temp_head;
                    temp_head->prev->next = new_channel;
                    temp_head->prev = new_channel;
                    chosen_slot->slot_invoked_channel = new_channel;
                    return SUCCESS;
                }
            }
            channel *new_channel;
            new_channel = kmalloc(sizeof(channel), GFP_KERNEL);
            if (new_channel == NULL){
                return -ENOMEM;
            }
        }
        return SUCCESS;
    } else{
        return -EINVAL;
    }

}
//==================== DEVICE SETUP =============================
struct file_operations Fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .release        = device_release,
        .unlocked_ioctl = device_ioctl,
};

static int __init message_slot_init(void)
{
    // taken from CHARDEV2\chardev.c file from recitation 6
    int rc = -1;
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
    for (int j = 0; j < 256; ++j) {
        message_slots[j].head = NULL;
    }
    return SUCCESS;
}

static void __exit message_slot_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    unregister_chrdev(major, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------

if (module_init(simple_init) != 0) {
    printk(KERN_ERR "message_slot module initialization failed");
}

module_exit(simple_cleanup);