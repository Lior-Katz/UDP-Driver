#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

static int __init test_init(void)
{
    printk(KERN_INFO "test_driver: module loaded\n");
    return 0;
}

static void __exit test_exit(void)
{
    printk(KERN_INFO "test_driver: module unloaded\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Lior Katz");
MODULE_DESCRIPTION("Minimal test driver");