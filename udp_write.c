#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define MOD_NAME "udp_write"
#define DEV_NAME ("udp")
#define UDP_WRITE_MINOR (0)
 
#define LOG(level, fmt, ...) \
    printk(KERN_##level MOD_NAME ": " fmt, ##__VA_ARGS__)

ssize_t udp_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static void __exit test_exit(void);

static dev_t device_number;
struct cdev* my_cdev = NULL;
struct file_operations udp_fops = {
	.owner =    THIS_MODULE,
	.write =    udp_write,
};

ssize_t udp_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    LOG(INFO, "write called\n");
    return count;
}

static int __init test_init(void)
{
    int result = 0;
    LOG(DEBUG, "allocating device numbers\n");
    result = alloc_chrdev_region(&device_number, UDP_WRITE_MINOR, 1, DEV_NAME);
    if (result < 0) {
        LOG(WARNING, "can't allocate major\n");
        return result;
    }
    int major = MAJOR(device_number);
    int minor = MINOR(device_number);
    LOG(INFO, "allocated %d, %d\n", major, minor);

    LOG(DEBUG, "registering device\n");
    my_cdev = cdev_alloc();
    if (my_cdev == NULL) {
        LOG(WARNING, "Failed to allocate cdev\n");
        result = -ENOMEM;
        goto failure;
    }
    my_cdev->ops = &udp_fops;
    my_cdev->owner = THIS_MODULE;
    int err = cdev_add(my_cdev, device_number, 1);
    if (err) {
        LOG(NOTICE, "Error %d adding device\n", err);
        return -1;
    }
    LOG(INFO, "registered device\n");


    LOG(INFO, "module loaded\n");
    return 0;

failure:
    test_exit();
    return result;
}

static void __exit test_exit(void)
{
    LOG(DEBUG, "deleting device registration\n");
    if (my_cdev != NULL) {
        cdev_del(my_cdev);
    }

    LOG(DEBUG, "unregistering device numbers\n");
    unregister_chrdev_region(device_number, 1);

    LOG(INFO, "module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Lior Katz");
MODULE_DESCRIPTION("Minimal test driver");