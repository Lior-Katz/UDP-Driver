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

#define __CHECK_FAIL_ERR(r)  ((r) != RESULT_OK)
#define __CHECK_FAIL_COND(r) (!(r))

#define __CHECK_IMPL(pred, expr)            \
    do {                                    \
        if (pred((expr))) {                 \
            goto fail;                      \
        }                                   \
    } while (0)

#define __CHECK_IMPL_RET(pred, expr, ret)   \
    do {                                    \
        if (pred((expr))) {                 \
            result = (ret);                 \
            goto fail;                      \
        }                                   \
    } while (0)

#define __CHECK_GET(_1,_2,NAME,...) NAME

#define CHECK(...) \
    __CHECK_GET(__VA_ARGS__, __CHECK_IMPL_RET, __CHECK_IMPL) \
    (__CHECK_FAIL_ERR, __VA_ARGS__)

#define CHECK_COND(...) \
    __CHECK_GET(__VA_ARGS__, __CHECK_IMPL_RET, __CHECK_IMPL) \
    (__CHECK_FAIL_COND, __VA_ARGS__)

typedef int result_t;

ssize_t udp_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static dev_t device_number;
struct cdev* my_cdev = NULL;
struct class* cls = NULL;
struct device* dev = NULL;
struct file_operations udp_fops = {
	.owner =    THIS_MODULE,
	.write =    udp_write,
};

result_t _parse_input(struct sockaddr_in* const address, char* data, char * const buf, size_t* const count) {
    result_t result = -EINVAL;
    
    const char* ip_start = buf;
    char* port_delim = strchr(buf, ':');
    CHECK_COND (port_delim != NULL);

    *port_delim = '\0';
    char* port_start = port_delim + 1;
    
    const char* end;
    CHECK_COND(in4_pton(ip_start, port_delim - ip_start, (u8*)&address->sin_addr.s_addr, -1, &end) != 0);
    CHECK_COND(end == port_delim);

    char* space = strchr(port_start, ' ');
    CHECK_COND (space != NULL);
    *space = '\0';
    u16 port;
    int err = kstrtou16(port_start, 10, &port);
    CHECK(err, err);
    
    address->sin_port = htons(port);
    *count -= (space - buf + 1);
    memcpy(data, space + 1, *count);

    address->sin_family = AF_INET;
    result = OK();
fail:
    return result;
}

result_t parse_input(struct sockaddr_in* address, char* data, const char __user * const buf, size_t* const count) {
    result_t result = OK();
    char* kbuf = kmalloc((*count) + 1, GFP_KERNEL | __GFP_ZERO);
    CHECK_COND(kbuf != NULL, -ENOMEM);
    CHECK(copy_from_user(kbuf, buf, *count), -EFAULT);
    result = _parse_input(address, data, kbuf, count);
fail:
    if (kbuf != NULL) {
        kfree(kbuf);
    }
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
    ssize_t result;
    struct sockaddr_in address = {0};
    char* data = kmalloc(count, GFP_KERNEL | __GFP_ZERO);
    if (data == NULL) {
        result = -ENOMEM;
        goto fail_no_release;
    }
    size_t data_len = count;
    result = parse_input(&address, data, buf, &data_len);
    CHECK(result);

    struct msghdr msg = build_message(&address);
    struct kvec kv = {
            .iov_base = data,
            .iov_len = data_len,
    };
        
    struct socket *sock = NULL;
    
    result = sock_create_kern(&init_net, PF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
    CHECK_COND(result >= 0);
    result = kernel_sendmsg(sock, &msg, &kv, 1, data_len);
    CHECK_COND(result >= 0);
    result += (count - data_len);

fail:
    if (sock != NULL) {
        sock_release(sock);
    }
    kfree(data);
fail_no_release:
    return result;
}

static result_t allocate_device_number(void) {
    result_t result = OK();
    LOG(DEBUG, "allocating device numbers\n");
    result = alloc_chrdev_region(&device_number, UDP_WRITE_MINOR, 1, DEV_CLS);
    CHECK_COND(result >= 0);
    LOG(INFO, "allocated %d, %d\n", MAJOR(device_number), MINOR(device_number));
fail:
    return result;
}

static result_t register_device(void) {
    result_t result = OK();
    LOG(DEBUG, "registering device\n");
    my_cdev = cdev_alloc();
    CHECK_COND(my_cdev != NULL, -ENOMEM);

    my_cdev->ops = &udp_fops;
    my_cdev->owner = THIS_MODULE;
    result = cdev_add(my_cdev, device_number, 1);
    CHECK(result);
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

static void __exit udp_write_exit(void)
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

static int __init udp_write_init(void)
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
    udp_write_exit();
    return result;
}

module_init(udp_write_init);
module_exit(udp_write_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lior Katz");
MODULE_DESCRIPTION("Character device allowing userspace to send UDP packets via write() using \"<ip>:<port> <data>\" format");