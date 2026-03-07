#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>

#define MOD_NAME "udp_write"
#define DEV_NAME ("udp")
#define UDP_WRITE_MINOR (0)
 
#define LOG(level, fmt, ...) \
    printk(KERN_##level MOD_NAME ": " fmt, ##__VA_ARGS__)

static dev_t dev;

static int __init test_init(void)
{
    LOG(DEBUG, "allocating device numbers\n");
    int result = alloc_chrdev_region(&dev, UDP_WRITE_MINOR, 1, DEV_NAME);
    if (result < 0) {
        LOG(WARNING, "can't allocate major\n");
        return result;
    }
    int major = MAJOR(dev);
    int minor = MINOR(dev);
    LOG(INFO, "allocated %d, %d\n", major, minor);

    LOG(INFO, "module loaded\n");
    return 0;
}

static void __exit test_exit(void)
{
    LOG(DEBUG, "unregistering device numbers\n");
    unregister_chrdev_region(dev, 1);
    LOG(INFO, "module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Lior Katz");
MODULE_DESCRIPTION("Minimal test driver");