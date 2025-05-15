#include <linux/module.h>
#include <linux/platform_device.h>

#include "pcd_platform.h"

/* Format pr_info() so it prints funtion name first */
#undef pr_fmt
#define pr_fmt(fmt) "%s : " fmt, __func__

#define NO_OF_DEVICES   (4)

/* 1. Create plaform data */
struct pcdev_platform_data pcdev_pdata[NO_OF_DEVICES] = {
    [0] = {
        .size = 512,
        .perm = PERM_RDWR,
        .sn = "PCDEV1"
    },
    [1] = {
        .size = 256,
        .perm = PERM_RDWR,
        .sn = "PCDEV2"
    },
    [2] = {
        .size = 128,
        .perm = PERM_RDONLY,
        .sn = "PCDEV3"
    },
    [3] = {
        .size = 64,
        .perm = PERM_WRONLY,
        .sn = "PCDEV4"
    }
};

/* Prototypes */
void pcdev_release(struct device *dev);

/* 2. Create 2 platform devices */

struct platform_device platform_pcdev1 = {
    .name = PCD_DEVICE_NAME "A1x",
    .id = 0,
    .dev = {
        .platform_data = &pcdev_pdata[0],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev2 = {
    .name = PCD_DEVICE_NAME "B1x",
    .id = 1,
    .dev = {
        .platform_data = &pcdev_pdata[1],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev3 = {
    .name = PCD_DEVICE_NAME "C1x",
    .id = 2,
    .dev = {
        .platform_data = &pcdev_pdata[2],
        .release = pcdev_release
    }
};

struct platform_device platform_pcdev4 = {
    .name = PCD_DEVICE_NAME "D1x",
    .id = 3,
    .dev = {
        .platform_data = &pcdev_pdata[3],
        .release = pcdev_release
    }
};

struct platform_device *plat_devices_array[] = {
    &platform_pcdev1, &platform_pcdev2, &platform_pcdev3, &platform_pcdev4
};

void pcdev_release(struct device *dev)
{
    pr_info("PCD plat device released\n");
}

/* Init and Deinit */

static int __init pcdev_platform_init(void)
{
    /* Register platform device */
    // platform_device_register(&platform_pcdev1);
    // platform_device_register(&platform_pcdev2);

    int rc = platform_add_devices(plat_devices_array, ARRAY_SIZE(plat_devices_array));
    if (rc) {
        pr_err("PCD plat init failed rc: %d\n", rc);
        return rc;
    }

    pr_info("PCD plat module loaded\n");

    return 0;
}

static void __exit pcdev_platform_exit(void)
{
    /* Unregister platform device */
    platform_device_unregister(&platform_pcdev1);
    platform_device_unregister(&platform_pcdev2);
    platform_device_unregister(&platform_pcdev3);
    platform_device_unregister(&platform_pcdev4);
    pr_info("PCD plat module unloaded\n");
}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kieran");
MODULE_DESCRIPTION("A pseudo char driver using internal memory");
MODULE_INFO(board, "Internal mem char driver");