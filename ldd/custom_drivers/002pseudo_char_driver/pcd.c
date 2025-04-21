#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#define DEV_MEM_SIZE    (512)

/* Format pr_info() so it prints funtion name first */
#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

/* pseudo device's memory */
static char device_buffer[DEV_MEM_SIZE];

/* to hold device number */
static dev_t dev_num;

/* prototypes */
static int pcd_open(struct inode *inode, struct file *fh);
static int pcd_release(struct inode *inode, struct file *fh);
static ssize_t pcd_read(struct file *fh, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos);
static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence);


/* char device structures */
static struct cdev pcd_cdev;
static struct file_operations pcd_fops = {
    .owner = THIS_MODULE,
    .llseek = pcd_llseek,
    .read = pcd_read,
    .write = pcd_write,
    .open = pcd_open,
    .release = pcd_release
};

/* Class create vars */
struct class *class_pcd;
struct device *device_pcd;

static int pcd_open(struct inode *inode, struct file *fh)
{
    pr_info("PCD Device open sys called\n");
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
    
    if (*f_pos >= DEV_MEM_SIZE)
        return 0;

    if ((*f_pos + count) > DEV_MEM_SIZE)
        count = DEV_MEM_SIZE - *f_pos;

    if (copy_to_user(buf, &device_buffer[*f_pos], count))
        return -EFAULT;

    *f_pos += count;

    pr_info("PCD Device read successfully read %zu bytes, new f_pos=%lld\n", count, *f_pos);

    return count;
}

static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos)
{
    pr_info("PCD Device write called for %zu bytes cur f_pos=%lld\n", count, *f_pos);

    if ((*f_pos + count) > DEV_MEM_SIZE)
        count = DEV_MEM_SIZE - *f_pos;
    
    if (!count)
        return -ENOMEM;
    
    if (copy_from_user(&device_buffer[*f_pos], buf, count))
        return -EFAULT;

    *f_pos += count;

    pr_info("PCD Device write successfully read %zu bytes, new f_pos=%lld\n", count, *f_pos);

    return count;
}

static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence)
{
    loff_t tmp;

    pr_info("PCD Device seek called cur f_pos=%lld + %lld\n", fh->f_pos, f_pos);

    switch(whence) {
    case SEEK_SET:
        if (f_pos > DEV_MEM_SIZE || f_pos < 0)
            return -EINVAL;
        fh->f_pos = f_pos;
        break;
    case SEEK_CUR:
        tmp = fh->f_pos + f_pos;
        if (tmp > DEV_MEM_SIZE || tmp < 0)
                return -EINVAL;
        fh->f_pos = tmp;
        break;
    case SEEK_END:
        tmp = DEV_MEM_SIZE + f_pos;
        if (tmp > DEV_MEM_SIZE || tmp < 0)
                return -EINVAL;
        fh->f_pos = tmp;
        break;
    default:
        return -EINVAL;
    }

    pr_info("PCD Device seek new f_pos=%lld\n", fh->f_pos);

    return fh->f_pos;
}

static int __init pcd_driver_init(void)
{
    int rc;

    /* 1. Dynamically allocate a device driver number */
    rc = alloc_chrdev_region(&dev_num, 0, 1, "pcd_devices");
    if (rc < 0)
        goto out;

    /* 2. Init the cdev structure with fops */
    cdev_init(&pcd_cdev, &pcd_fops);
    pcd_cdev.owner = THIS_MODULE;

    /* 3. Register cdev structure with virtual file sys (VFS) */
    rc = cdev_add(&pcd_cdev, dev_num, 1);
    if (rc < 0)
        goto unreg_chrdev;

    /* 4. Create device class under /sys/class/ */
    class_pcd = class_create("pcd_class");
    if (IS_ERR(class_pcd)) {
        pr_info("Class creation failed\n");
        rc = PTR_ERR(class_pcd);
        goto cdev_del;
    }
        

    /* 5. Populate the sysfs with device information */
    device_pcd = device_create(class_pcd, NULL, dev_num, NULL, "pcd");
    if (IS_ERR(device_pcd)) {
        pr_info("Device creation failed\n");
        rc = PTR_ERR(device_pcd);
        goto cls_destroy;
    }

    pr_info("PCD Device module init successful maj: %u min: %u\n",
        MAJOR(dev_num), MINOR(dev_num));

    return 0;

cls_destroy:
    class_destroy(class_pcd);
cdev_del:
    cdev_del(&pcd_cdev);
unreg_chrdev:
    unregister_chrdev_region(dev_num, 1);
out:
    pr_info("PCD module insertion failed\n");
    return rc;
}

static void __exit pcd_driver_cleanup(void)
{
    /* Perform actions of init in reverse order */
    device_destroy(class_pcd, dev_num);
    class_destroy(class_pcd);
    cdev_del(&pcd_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("PCD Device cleaning up maj: %u min: %u\n",
        MAJOR(dev_num), MINOR(dev_num));
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kieran");
MODULE_DESCRIPTION("A pseudo char driver using internal memory");
MODULE_INFO(board, "Internal mem char driver");
