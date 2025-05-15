#ifndef __PCD_PLATFORM_H
#define __PCD_PLATFORM_H

#define PCD_DEVICE_NAME     "pcddev-"

enum {
    PERM_RDONLY = 0x1,
    PERM_WRONLY = 0x10,
    PERM_RDWR = 0x11,
};

struct pcdev_platform_data {
    int size;
    int perm;
    const char *sn;
};

#endif /* #ifndef __PCD_PLATFORM_H */