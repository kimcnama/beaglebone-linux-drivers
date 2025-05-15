#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/platform_device.h>

#include "pcd_platform.h"

/* Format pr_info() so it prints funtion name first */
#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

#define MAX_N_DEVICES 2

/* prototypes */
static int pcd_open(struct inode *inode, struct file *fh);
static int pcd_release(struct inode *inode, struct file *fh);
static ssize_t pcd_read(struct file *fh, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos);
static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence);

static int pcd_plt_drv_probe(struct platform_device *dev);
static int pcd_plt_drv_remove(struct platform_device *dev);

/* PCD driver data - statically alloc */
struct pcddrv_priv_data {
    int total_devices;
    dev_t dev_num_base;
    struct class *class_pcd;
    struct device *device_pcd;
};

/* PCD device data - dynamic alloc with device creation */
struct pcdev_priv_data {
    struct pcdev_platform_data pdata;
    char *buf;
    dev_t dev_num;
    struct cdev cdev;
};

struct pcddrv_priv_data pcdrv_data;

/* char device structures */
static struct file_operations pcd_fops = {
    .owner = THIS_MODULE,
    .llseek = pcd_llseek,
    .read = pcd_read,
    .write = pcd_write,
    .open = pcd_open,
    .release = pcd_release
};

/* Driver struct */
/* Arbritary device configs */
struct device_config {
    int cfg_item1;
    int cfg_item2;
};

enum pcdev_names {
    PCDEVA1x = 0,
    PCDEVB1x,
    PCDEVC1x,
    PCDEVD1x
};

static struct device_config dev_cfgs[] = {
    [PCDEVA1x] = {.cfg_item1 = 60, .cfg_item2 = 21},
    [PCDEVB1x] = {.cfg_item1 = 50, .cfg_item2 = 22},
    [PCDEVC1x] = {.cfg_item1 = 40, .cfg_item2 = 23},
    [PCDEVD1x] = {.cfg_item1 = 30, .cfg_item2 = 24},
};

static const struct platform_device_id plt_devs_ids[] = {
    [0] = {.name = PCD_DEVICE_NAME "A1x", .driver_data = PCDEVA1x},
    [1] = {.name = PCD_DEVICE_NAME "B1x", .driver_data = PCDEVB1x},
    [2] = {.name = PCD_DEVICE_NAME "C1x", .driver_data = PCDEVC1x},
    [3] = {.name = PCD_DEVICE_NAME "D1x", .driver_data = PCDEVD1x}
};

static struct platform_driver pcdev_plt_drv = {
    .probe = pcd_plt_drv_probe,
    .remove = pcd_plt_drv_remove,
    .id_table = plt_devs_ids,
    .driver = {
        .name = PCD_DEVICE_NAME
    }
};

static int check_permission(int dev_perm, int acc_mode)
{
    if (dev_perm == PERM_RDWR)
        return 0;
    
    if (dev_perm == PERM_RDONLY && (acc_mode & FMODE_READ) && !(acc_mode & FMODE_WRITE))
        return 0;
    
    if (dev_perm == PERM_WRONLY && !(acc_mode & FMODE_READ) && (acc_mode & FMODE_WRITE))
        return 0;

    return -EPERM;
}

static int pcd_open(struct inode *inode, struct file *fh)
{
    pr_info("PCD Device open called\n");
    return 0;
}

/* Only called when references to driver count reaches 0
* so not necessarily called on close(). */
static int pcd_release(struct inode *inode, struct file *fh)
{
    pr_info("PCD Device release called\n");
    return 0;
}

static ssize_t pcd_read(struct file *fh, char __user *buf, size_t count, loff_t *f_pos)
{
    pr_info("PCD Device read called for %zu bytes cur f_pos=%lld\n", count, *f_pos);
    return 0;
}

static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos)
{
    pr_info("PCD Device write called for %zu bytes cur f_pos=%lld\n", count, *f_pos);

    return -ENOMEM;
}

static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence)
{
    pr_info("PCD Device seek called cur f_pos=%lld + %lld\n", fh->f_pos, f_pos);
    return 0;
}

/* Called when matched platform device is found */
static int pcd_plt_drv_probe(struct platform_device *dev)
{
    int rc;
    struct pcdev_priv_data *dev_data;
    struct pcdev_platform_data *pdata;

    pr_info("PCD Device detected\n");
    
    /* 1. Get platform data */
    pdata = (struct pcdev_platform_data*)dev_get_platdata(&dev->dev);
    if (!pdata) {
        pr_err("No platform data available\n");
        rc = -EINVAL;
        goto out;
    }

    /* 2. Dynamically alloc memory for device private data */
    dev_data = devm_kzalloc(&dev->dev, sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data) {
        pr_err("No slab space available\n");
        rc = -ENOMEM;
        goto out;
    }

    dev_data->pdata.size = pdata->size;
    dev_data->pdata.perm = pdata->perm;
    dev_data->pdata.sn = pdata->sn;
    pr_info("Dev SN = %s\n", dev_data->pdata.sn);
    pr_info("Dev size = %d\n", dev_data->pdata.size);
    pr_info("Dev perm = %d\n", dev_data->pdata.perm);

    pr_info("cfg1 = %d\n", dev_cfgs[dev->id_entry->driver_data].cfg_item1);
    pr_info("cfg2 = %d\n", dev_cfgs[dev->id_entry->driver_data].cfg_item2);

    /* Set the allocated dev_data field to driver data field of platform device
    so it can be accessed in removed function to free data later */
    dev->dev.driver_data = dev_data;

    /* 3. Dynamically alloc mem for the device buf using size info from plat data */
    dev_data->buf = devm_kzalloc(&dev->dev, dev_data->pdata.size, GFP_KERNEL);
    if (!dev_data->buf) {
        pr_err("No slab space available\n");
        rc = -ENOMEM;
        goto dev_data_free;
    }

    /* 4. Get the device num */
    dev_data->dev_num = pcdrv_data.dev_num_base + dev->id;

    /* 5. Do cdev init and cdev add */
    cdev_init(&dev_data->cdev, &pcd_fops);
    dev_data->cdev.owner = THIS_MODULE;

    rc = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
    if (rc < 0)
        goto free_buf;

    /* 6. Create device file for detected platform device */
    pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pcdev-%d", dev->id);
    if (IS_ERR(pcdrv_data.device_pcd)) {
        pr_err("Device create failed\n");
        rc = PTR_ERR(pcdrv_data.device_pcd);
        goto cdev_del;
    }

    pcdrv_data.total_devices++;

    return 0;

    /* 7. Error handling */
cdev_del:
    cdev_del(&dev_data->cdev);
free_buf:
    devm_kfree(&dev->dev, dev_data->buf);
dev_data_free:
    devm_kfree(&dev->dev, dev_data);
out:
    return rc;
}

/* Called when device is removed from system */
static int pcd_plt_drv_remove(struct platform_device *dev)
{
    /* Doing error handling from pcd_plt_drv_probe() */
    struct pcdev_priv_data *dev_data = (struct pcdev_priv_data*)dev->dev.driver_data;

    /* 1. Remove device created */
    device_destroy(pcdrv_data.class_pcd, dev_data->dev_num);

    /* 2. Remove a cdev entry from the system */
    cdev_del(&dev_data->cdev);

    /* 3. Free the memory held by the device */
    kfree(dev_data->buf);
    kfree(dev_data);

    pcdrv_data.total_devices--;

    pr_info("PCD Device removed\n");
    return 0;
}

/* Init and deinit */

static int __init pcd_driver_init(void)
{
    int rc;

    pr_info("PCD plat driver init\n");

    /* 1. Dynamically allocate a device num */
    rc = alloc_chrdev_region(&pcdrv_data.dev_num_base, 0, MAX_N_DEVICES, "pcddevs");
    if (rc < 0)
        goto out;

    /* 2. Create device class under /sys/class */
    pcdrv_data.class_pcd = class_create("pcd_class");
    if (IS_ERR(pcdrv_data.class_pcd)) {
        pr_info("Class creation failed\n");
        rc = PTR_ERR(pcdrv_data.class_pcd);
        unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_N_DEVICES);
        return rc;
    }

    /* 3. Register a platform driver */
    platform_driver_register(&pcdev_plt_drv);

    return rc;

out:
    pr_info("PCD module insertion failed\n");
    return rc;
}

static void __exit pcd_driver_cleanup(void)
{
    pr_info("PCD plat driver exit\n");

    /* 1. Unregister plat driver */
    platform_driver_unregister(&pcdev_plt_drv);

    /* 2. Destroy device class */
    class_destroy(pcdrv_data.class_pcd);

    /* 3. Dealloc device num */
    unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_N_DEVICES);
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kieran");
MODULE_DESCRIPTION("A pseudo char driver using internal memory");
MODULE_INFO(board, "Internal mem char driver");
