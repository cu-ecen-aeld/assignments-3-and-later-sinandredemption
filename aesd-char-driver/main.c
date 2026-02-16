/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/errno.h>
#include "aesdchar.h"

#define PRINTK_FAULT(CODE) PDEBUG("fault code: %d", CODE)

struct aesd_buffer_entry buff_entry;

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev; /* device information */
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    // I guess we only need to point filp->private_data to aesd_dev
    

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    /**
     * TODO: handle release
     */

    PDEBUG("RELEASE");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    // Same as aesd_device static global but I like it verbose for some reason
    struct aesd_dev* fdev = (struct aesd_dev*)(filp->private_data);
    struct aesd_circular_buffer *fbuff = &(fdev->buffer);
    size_t start;
    struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(fbuff, *f_pos, &start);
    
    if (entry == NULL) {
        PRINTK_FAULT(__LINE__);
        goto out;
    }

    // We will copy to userspoace only bytes stored in the entry found
    // if requested count is more, the kernel will call again

    if (copy_to_user(buf, entry->buffptr + start, entry->size - start)) {
        PRINTK_FAULT(__LINE__);
        retval = -EFAULT;
        goto out;
    }

    *f_pos += entry->size - start;
    retval = entry->size - start;

    out:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev* fdev = (struct aesd_dev*)(filp->private_data);
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

     // okayu, we gotta write buf of size `count`
     // I think we can ignore f_pos
     
     // for now, just write directly.
     // TBD lock

     
    // extend by allocating
    char *tmp_buff = kmalloc(count + buff_entry.size, GFP_KERNEL);

    if (tmp_buff == NULL)
    {
        PRINTK_FAULT(__LINE__);
        retval = -ENOMEM;
        goto out;
    }
    
    // copy from previous buffer
    if (buff_entry.size > 0)
        memcpy(tmp_buff, buff_entry.buffptr, buff_entry.size);

    kfree(buff_entry.buffptr);
    buff_entry.buffptr = tmp_buff;


    if (copy_from_user(buff_entry.buffptr + buff_entry.size, buf, count)) {
       PRINTK_FAULT(__LINE__);
       retval = -EFAULT;
       goto out;
    }

    buff_entry.size += count;

    aesd_circular_buffer_add_entry(&(fdev->buffer), &buff_entry);
    buff_entry.buffptr = NULL;
    buff_entry.size = 0;

    retval = count;
    *f_pos += count;
    // TBD unlock
     
    out:
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    PDEBUG("INIT");
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    
    aesd_circular_buffer_init(&(aesd_device.buffer));

    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    // Release memory allocated via kmalloc
    int index;
    struct aesd_buffer_entry* entry;
    struct aesd_dev *fdev = &aesd_device;
    struct aesd_circular_buffer *fbuff = &(fdev->buffer);
    
    PDEBUG("CLEANUP");

    AESD_CIRCULAR_BUFFER_FOREACH(entry,fbuff,index) {
        PDEBUG("Freeing memory at 0x%llx", (long long)(entry->buffptr));
        kfree(entry->buffptr); // kfree can be called with null
    }

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    // TODO Probably should cleanup all memory allocated by aesd_buffer here

    unregister_chrdev_region(devno, 1);

    buff_entry.buffptr = NULL;
    buff_entry.size = 0;

}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
