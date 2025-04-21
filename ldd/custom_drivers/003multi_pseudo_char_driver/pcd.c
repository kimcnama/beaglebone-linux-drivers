#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/uaccess.h>

#define NO_OF_DEVICES    (4)
#define DEV1_MEM_SIZE    (1024)
#define DEV2_MEM_SIZE    (1024)
#define DEV3_MEM_SIZE    (1024)
#define DEV4_MEM_SIZE    (1024)

enum {
    PERM_RDONLY = 0x1,
    PERM_WRONLY = 0x10,
    PERM_RDWR = 0x11,
};

/* Format pr_info() so it prints funtion name first */
#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

/* pseudo device's memory */
static char dev1_buffer[DEV1_MEM_SIZE];
static char dev2_buffer[DEV2_MEM_SIZE];
static char dev3_buffer[DEV3_MEM_SIZE];
static char dev4_buffer[DEV4_MEM_SIZE];

/* prototypes */
static int pcd_open(struct inode *inode, struct file *fh);
static int pcd_release(struct inode *inode, struct file *fh);
static ssize_t pcd_read(struct file *fh, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos);
static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence);

/* pcd device private data */
struct pcdev_priv_data {
    char *buf;
    unsigned size;
    const char *sn;
    int perm;
    struct cdev cdev;
};

/* pcd drivers private data */
struct pcddrv_priv_data {
    int total_devices;
    dev_t dev_num;
    struct class *class_pcd;
    struct device *device_pcd;
    struct pcdev_priv_data pcdev_data[NO_OF_DEVICES];
};

/* char device structures */
static struct file_operations pcd_fops = {
    .owner = THIS_MODULE,
    .llseek = pcd_llseek,
    .read = pcd_read,
    .write = pcd_write,
    .open = pcd_open,
    .release = pcd_release
};

struct pcddrv_priv_data pcdrv_data = {
    .total_devices = NO_OF_DEVICES,
    .pcdev_data = {
        [0] = {
            .buf = dev1_buffer,
            .size = DEV1_MEM_SIZE,
            .sn = "PCDDEV1",
            .perm = PERM_RDONLY
        },
        [1] = {
            .buf = dev2_buffer,
            .size = DEV2_MEM_SIZE,
            .sn = "PCDDEV2",
            .perm = PERM_WRONLY
        },
        [2] = {
            .buf = dev3_buffer,
            .size = DEV3_MEM_SIZE,
            .sn = "PCDDEV3",
            .perm = PERM_RDWR
        },
        [3] = {
            .buf = dev4_buffer,
            .size = DEV4_MEM_SIZE,
            .sn = "PCDDEV4",
            .perm = PERM_RDONLY
        }
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
    int rc;
    int minor_n;
    struct pcdev_priv_data *prv_data;

    /* Find out what device is being accessed */
    minor_n = MINOR(inode->i_rdev);
    pr_info("PCD Device open sys called on device %d\n", minor_n);

    /* Get devices private data struct */
    prv_data = container_of(inode->i_cdev, struct pcdev_priv_data, cdev);

    /* Set private data of file handle for subsequent fh calls */
    fh->private_data = prv_data;

    rc = check_permission(prv_data->perm, fh->f_mode);

    if (rc)
        pr_info("PCD Device open failed for device %d rc: %d\n", minor_n, rc);
    else 
        pr_info("PCD Device open success for device %d\n", minor_n);
    
    return rc;
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
    struct pcdev_priv_data *prv_data = fh->private_data;
    pr_info("PCD Device on dev %s read called for %zu bytes cur f_pos=%lld\n", prv_data->sn, count, *f_pos);
    
    if (*f_pos >= prv_data->size)
        return 0;

    if ((*f_pos + count) > prv_data->size)
        count = prv_data->size - *f_pos;

    if (copy_to_user(buf, &prv_data->buf[*f_pos], count))
        return -EFAULT;

    *f_pos += count;

    pr_info("PCD Device read on dev %s successfully read %zu bytes, new f_pos=%lld\n", prv_data->sn, count, *f_pos);

    return count;
}

static ssize_t pcd_write(struct file *fh, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct pcdev_priv_data *prv_data = fh->private_data;
    pr_info("PCD Device on dev %s write called for %zu bytes cur f_pos=%lld\n", prv_data->sn, count, *f_pos);

    if ((*f_pos + count) > prv_data->size)
        count = prv_data->size - *f_pos;
    
    if (!count)
        return -ENOMEM;
    
    if (copy_from_user(&prv_data->buf[*f_pos], buf, count))
        return -EFAULT;

    *f_pos += count;

    pr_info("PCD Device write on dev %s successfully read %zu bytes, new f_pos=%lld\n", prv_data->sn, count, *f_pos);

    return count;
}

static loff_t pcd_llseek(struct file *fh, loff_t f_pos, int whence)
{
    loff_t tmp;
    struct pcdev_priv_data *prv_data = fh->private_data;
    pr_info("PCD Device on dev %s seek called cur f_pos=%lld + %lld\n", prv_data->sn, fh->f_pos, f_pos);

    switch(whence) {
    case SEEK_SET:
        if (f_pos > prv_data->size || f_pos < 0)
            return -EINVAL;
        fh->f_pos = f_pos;
        break;
    case SEEK_CUR:
        tmp = fh->f_pos + f_pos;
        if (tmp > prv_data->size || tmp < 0)
                return -EINVAL;
        fh->f_pos = tmp;
        break;
    case SEEK_END:
        tmp = prv_data->size + f_pos;
        if (tmp > prv_data->size || tmp < 0)
                return -EINVAL;
        fh->f_pos = tmp;
        break;
    default:
        return -EINVAL;
    }

    pr_info("PCD Device %s seek new f_pos=%lld\n", prv_data->sn, fh->f_pos);

    return fh->f_pos;
}

static int __init pcd_driver_init(void)
{
    int rc;
    int i;

    /* 1. Dynamically allocate a device driver number */
    rc = alloc_chrdev_region(&pcdrv_data.dev_num, 0, NO_OF_DEVICES, "pcd_devs");
    if (rc < 0)
        goto out;

    /* 2. Create device class under /sys/class/ */
    pcdrv_data.class_pcd = class_create("pcd_class");
    if (IS_ERR(pcdrv_data.class_pcd)) {
        pr_info("Class creation failed\n");
        rc = PTR_ERR(pcdrv_data.class_pcd);
        goto unreg_chrdev;
    }

    for (i = 0; i < NO_OF_DEVICES; ++i) {

        pr_info("PCD Device init of maj: %u min: %u\n",
            MAJOR(pcdrv_data.dev_num + i), MINOR(pcdrv_data.dev_num + i));

        /* 3. Init the cdev structure with fops */
        cdev_init(&pcdrv_data.pcdev_data[i].cdev, &pcd_fops);
        pcdrv_data.pcdev_data[i].cdev.owner = THIS_MODULE;

        /* 4. Register cdev structure with virtual file sys (VFS) */
        rc = cdev_add(&pcdrv_data.pcdev_data[i].cdev, pcdrv_data.dev_num + i, 1);
        if (rc < 0)
            goto cdev_del;
        
        /* 5. Populate the sysfs with device information */
        pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, pcdrv_data.dev_num + i, NULL, "pcdev-%d", i + 1);
        if (IS_ERR(pcdrv_data.device_pcd)) {
            pr_info("Device creation failed\n");
            rc = PTR_ERR(pcdrv_data.device_pcd);
            goto cls_del;
        }
    }

    pr_info("PCD Device module init successful\n");

    return 0;

cls_del:
cdev_del:
    for (; i >= 0; --i) {
        device_destroy(pcdrv_data.class_pcd, pcdrv_data.dev_num + i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);
unreg_chrdev:
    unregister_chrdev_region(pcdrv_data.dev_num, NO_OF_DEVICES);
out:
    pr_info("PCD module insertion failed\n");
    return rc;
}

static void __exit pcd_driver_cleanup(void)
{
    /* Perform actions of init in reverse order */
    int i;
    for (i = 0; i < NO_OF_DEVICES; ++i) {

        pr_info("PCD Device cleaning up maj: %u min: %u\n",
            MAJOR(pcdrv_data.dev_num + i), MINOR(pcdrv_data.dev_num + i));

        device_destroy(pcdrv_data.class_pcd, pcdrv_data.dev_num + i);
        cdev_del(&pcdrv_data.pcdev_data[i].cdev);
    }
    class_destroy(pcdrv_data.class_pcd);
    unregister_chrdev_region(pcdrv_data.dev_num, NO_OF_DEVICES);
}

module_init(pcd_driver_init);
module_exit(pcd_driver_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kieran");
MODULE_DESCRIPTION("A pseudo char driver using internal memory");
MODULE_INFO(board, "Internal mem char driver");
