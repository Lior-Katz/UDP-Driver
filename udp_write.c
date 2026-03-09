#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device/class.h>
#include <linux/err.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/uio.h>
#include <net/sock.h>
#include <linux/kstrtox.h>

#define MOD_NAME "udp_write"
#define DEV_NAME ("udp")
#define DEV_CLS DEV_NAME
#define UDP_WRITE_MINOR (0)

#define LOG(level, fmt, ...) \
    printk(KERN_##level MOD_NAME ": " fmt, ##__VA_ARGS__)

#define RESULT_OK (0)
#define OK() ((result_t){RESULT_OK})
#define ERROR(code) ((result_t){code})
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
struct class* cls = NULL;
struct device* dev = NULL;
struct file_operations udp_fops = {
	.owner =    THIS_MODULE,
	.write =    udp_write,
};

result_t _parse_input(struct sockaddr_in* const address, char* data, char * const buf, size_t* const count) {
    result_t err = -EINVAL;
    
    const char* ip_start = buf;
    char* port_delim = strchr(buf, ':');
    if (port_delim == NULL){
        goto fail;
    }
    *port_delim = '\0';
    char* port_start = port_delim + 1;
    
    const char* end;
    if (!in4_pton(ip_start, port_delim - ip_start, (u8*)&address->sin_addr.s_addr, -1, &end)) {
        goto fail;
    }
    if (end != port_delim) {
        goto fail;
    }
    char* space = strchr(port_start, ' ');
    if (space == NULL) {
        goto fail;
    }
    *space = '\0';
    u16 port;
    int res = kstrtou16(port_start, 10, &port);
    if (res != 0) {
        err = res;
        goto fail;
    }
    
    address->sin_port = htons(port);
    *count -= (space - buf + 1);
    memcpy(data, space + 1, *count);

    address->sin_family = AF_INET;
    err = OK();
fail:
    return err;
}

result_t parse_input(struct sockaddr_in* address, char* data, const char __user * const buf, size_t* const count) {
    char* kbuf = kmalloc((*count) + 1, GFP_KERNEL | __GFP_ZERO);
    if (kbuf == NULL) {
        return -ENOMEM;
    }
    if (copy_from_user(kbuf, buf, *count)) {
        kfree(kbuf);
        return -EFAULT;
    }
    result_t result = _parse_input(address, data, kbuf, count);
    kfree(kbuf);
    return result;
}

struct msghdr build_message(struct sockaddr_in* address) {
    struct msghdr msg = {
        .msg_name = address,
        .msg_namelen = sizeof(*address),
        .msg_flags = 0,
        .msg_control = NULL,
        .msg_controllen = 0,
    };
    return msg;
}

ssize_t udp_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    ssize_t retval;
    struct sockaddr_in address = {0};
    char* data = kmalloc(count, GFP_KERNEL | __GFP_ZERO);
    if (data == NULL) {
        retval = -ENOMEM;
        goto fail_no_release;
    }
    size_t data_len = count;
    retval = parse_input(&address, data, buf, &data_len);
    CHECK_MSG(retval, "failed to parse data\n");

    struct msghdr msg = build_message(&address);
    struct kvec kv = {
            .iov_base = data,
            .iov_len = data_len,
        };
        
    struct socket *sock = NULL;
    int err;
    
    err = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
    if (err < 0) {
        retval = err;
        goto fail;
    }
    retval = kernel_sendmsg(sock, &msg, &kv, 1, data_len);
    if (retval < 0) {
        goto fail;
    }
    retval = count;

fail:
    if (sock != NULL) {
        sock_release(sock);
    }
    kfree(data);
fail_no_release:
    return retval;
}

static result_t allocate_device_number(void) {
    result_t result = OK();
    LOG(DEBUG, "allocating device numbers\n");
    result = alloc_chrdev_region(&device_number, UDP_WRITE_MINOR, 1, DEV_CLS);
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

static result_t create_node(void) {
    result_t result = OK();
    LOG(DEBUG, "creating node\n");
    LOG(DEBUG, "creating class\n");
    cls = class_create(DEV_CLS);
    if (IS_ERR_OR_NULL(cls)) {
        LOG(ERR, "failed creating class");
        return ERROR(PTR_ERR(cls));
    }

    LOG(DEBUG, "creating device\n");
    dev = device_create(cls, NULL, device_number, NULL, DEV_NAME);
    if (IS_ERR_OR_NULL(dev)) {
        LOG(ERR, "failed creating device node");
        return ERROR(PTR_ERR(dev));
    }
    LOG(INFO, "node created\n");
    return result;
}

static int __init test_init(void)
{
    result_t result = allocate_device_number();
    CHECK(result);

    result = register_device();
    CHECK(result);

    result = create_node();
    CHECK(result);

    LOG(INFO, "module loaded\n");
    return 0;

fail:
    test_exit();
    return result;
}

static void __exit test_exit(void)
{
    if (!IS_ERR_OR_NULL(cls)) {
        if (!IS_ERR_OR_NULL(dev)) {
            LOG(DEBUG, "deleting device %d:%d from %s\n", MAJOR(device_number), MINOR(device_number), cls->name);
            device_destroy(cls, device_number);
        }
        LOG(DEBUG, "deleting class %s\n", cls->name);
        class_destroy(cls);
    }

    if (my_cdev != NULL) {
        LOG(DEBUG, "deleting device registration\n");
        cdev_del(my_cdev);
    }

    LOG(DEBUG, "unregistering device numbers\n");
    unregister_chrdev_region(device_number, 1);

    LOG(INFO, "module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lior Katz");
MODULE_DESCRIPTION("Minimal test driver");
