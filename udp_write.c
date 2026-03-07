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

#define RESULT_OK (0)
#define OK() ((result_t){RESULT_OK})
#define ERR(code) ((result_t){code})
#define FAILED(r) ((r) != RESULT_OK)
#define IS_OK(r) ((r) == RESULT_OK)

#define CHECK(value) \
    if (FAILED(value)) { \
        goto fail; \
    }

#define CHECK_MSG(value, fmt, ...) \
    if (FAILED(value)) { \
        LOG(ERR, fmt, ##__VA_ARGS__); \
        goto fail; \
    }

#define CHECK_RES(value, res, fmt, ...) \
    if (FAILED(value)) { \
        result = (res); \
        LOG(ERR, fmt, ##__VA_ARGS__); \
        goto fail; \
    }

typedef int result_t;

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

static result_t allocate_device_number(void) {
    result_t result = OK();
    LOG(DEBUG, "allocating device numbers\n");
    result = alloc_chrdev_region(&device_number, UDP_WRITE_MINOR, 1, DEV_NAME);
    CHECK_MSG(result < 0, "can't allocate major\n");
    LOG(INFO, "allocated %d, %d\n", MAJOR(device_number), MINOR(device_number));
fail:
    return result;
}

static result_t register_device(void) {
    result_t result = OK();
    LOG(DEBUG, "registering device\n");
    my_cdev = cdev_alloc();
    CHECK_RES(my_cdev == NULL, -ENOMEM, "Failed to allocate cdev\n");

    my_cdev->ops = &udp_fops;
    my_cdev->owner = THIS_MODULE;
    result = cdev_add(my_cdev, device_number, 1);
    CHECK_RES(result, -1, "Error %d adding device\n", result);
    LOG(INFO, "registered device\n");
    return result;

fail:
    if (my_cdev != NULL) {
        cdev_del(my_cdev);
        my_cdev = NULL;
    }
    return result;
}

static int __init test_init(void)
{
    result_t result = allocate_device_number();
    CHECK(result);

    result = register_device();
    CHECK(result);

    LOG(INFO, "module loaded\n");
    return 0;

fail:
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