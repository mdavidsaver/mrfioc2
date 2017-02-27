#ifndef MRFEVDEV_H
#define MRFEVDEV_H

#include <asm/ioctl.h>
#include <linux/types.h>

#define MRFEV_MAGIC 'V'

#define MRFEV_GET_VERSION _IOR(MRFEV_MAGIC, 1, u32)
#define MRFEV_GET_VERSION_CURRENT 0

#define MRFEV_SET_INTERESTED _IOW(MRFEV_MAGIC, 2, u32)
#define MRFEV_GET_INTERESTED _IOR(MRFEV_MAGIC, 2, u32)

#define MRFEV_GET_ACTIVE _IOR(MRFEV_MAGIC, 3, u32)

#define MRFEV_ABORT _IO(MRFEV_MAGIC, 4)

#endif // MRFEVDEV_H
