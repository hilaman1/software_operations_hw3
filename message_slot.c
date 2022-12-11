#include "message_slot.h"
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

// this code is based on "chardev" files from recitation 6

#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/

MODULE_LICENSE("GPL");

static int major = 235;
#define DEVICE_RANGE_NAME "char_dev"

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

    // major num is always defined as 235
    static int major = 235;


    return 0;
}

static void __exit message_slot_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    unregister_chrdev(major, DEVICE_RANGE_NAME);
}

if (module_init(simple_init) != 0) {
    printk(KERN_ERR "message_slot module initialization failed");
}
module_exit(simple_cleanup);