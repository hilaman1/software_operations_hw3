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

MODULE_LICENSE("GPL");


struct chardev_info {
    spinlock_t lock;
};

// used to prevent concurent access into the same device
static int dev_open_flag = 0;

static struct chardev_info device_info;

// The message the device will give when asked
static char the_message[BUF_LEN];

// major num is always defined as 235

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
            errno = EINVAL;
            return -1;
        }
        return SUCCESS;

    } else{
        errno = EINVAL;
        return -1;
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
    // init dev struct
    memset( &device_info, 0, sizeof(struct chardev_info) );
    spin_lock_init( &device_info.lock );
    return 0;
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