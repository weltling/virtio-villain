/* SPDX-License-Identifier: Apache-2.0 */
#include "virtio_mmio.h"
#include "util.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/*
 * Discover an MMIO virtio device by scanning platform devices in sysfs.
 * QEMU microvm and ARM virt expose devices as /sys/bus/platform/devices/
 * entries with compatible "virtio,mmio".
 *
 * Alternatively, parse /proc/device-tree nodes with compatible = "virtio,mmio".
 */
static int is_virtio_mmio_device(const char *platform_dir, const char *name)
{
    /* Direct name match (ARM virt, older QEMU) */
    if (strstr(name, "virtio"))
        return 1;
    if (strstr(name, "LNRO0005"))
        return 1;

    /* ACPI-enumerated: check modalias for "acpi:LNRO0005" (virtio-mmio HID) */
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/modalias", platform_dir, name);
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    char buf[64] = {0};
    (void)read(fd, buf, sizeof(buf) - 1);
    close(fd);
    return strstr(buf, "LNRO0005") != NULL;
}

static int scan_platform_devices(struct virtio_mmio_dev *dev)
{
    const char *platform_dir = "/sys/bus/platform/devices";
    DIR *dir = opendir(platform_dir);
    if (!dir)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;
        if (!is_virtio_mmio_device(platform_dir, ent->d_name))
            continue;

        /* Try sysfs resources file first (works when driver is bound) */
        char res_path[512];
        snprintf(res_path, sizeof(res_path), "%s/%s/resources",
                 platform_dir, ent->d_name);

        FILE *f = fopen(res_path, "r");
        if (f) {
            char line[256];
            uint64_t start = 0, end = 0;
            unsigned long flags = 0;
            if (fgets(line, sizeof(line), f)) {
                if (sscanf(line, "%lx %lx %lx", &start, &end, &flags) == 3) {
                    if (start != 0 && end > start) {
                        dev->phys_base = start;
                        dev->size = (uint32_t)(end - start + 1);
                        fclose(f);
                        closedir(dir);
                        return 0;
                    }
                }
            }
            fclose(f);
        }

        /* Fallback: find resource in /proc/iomem by device name */
        f = fopen("/proc/iomem", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (!strstr(line, ent->d_name))
                    continue;
                uint64_t start = 0, end = 0;
                if (sscanf(line, " %lx-%lx", &start, &end) == 2) {
                    if (start != 0 && end > start) {
                        dev->phys_base = start;
                        dev->size = (uint32_t)(end - start + 1);
                        fclose(f);
                        closedir(dir);
                        return 0;
                    }
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return -1;
}

/*
 * Fallback: scan device tree for virtio,mmio compatible nodes.
 */
static int scan_device_tree(struct virtio_mmio_dev *dev)
{
    const char *dt_dir = "/proc/device-tree";
    DIR *dir = opendir(dt_dir);
    if (!dir)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char compat_path[512];
        snprintf(compat_path, sizeof(compat_path), "%s/%s/compatible",
                 dt_dir, ent->d_name);

        int fd = open(compat_path, O_RDONLY);
        if (fd < 0)
            continue;

        char buf[64] = {0};
        (void)read(fd, buf, sizeof(buf) - 1);
        close(fd);

        if (strstr(buf, "virtio,mmio") == NULL)
            continue;

        /* Read reg property: <address> <size> in big-endian cells */
        char reg_path[512];
        snprintf(reg_path, sizeof(reg_path), "%s/%s/reg", dt_dir, ent->d_name);
        fd = open(reg_path, O_RDONLY);
        if (fd < 0)
            continue;

        /* Assume #address-cells=2, #size-cells=2 (64-bit each) */
        uint32_t cells[4];
        ssize_t n = read(fd, cells, sizeof(cells));
        close(fd);
        if (n < (ssize_t)sizeof(cells))
            continue;

        /* Convert from big-endian */
        uint64_t addr = ((uint64_t)__builtin_bswap32(cells[0]) << 32) |
                        __builtin_bswap32(cells[1]);
        uint64_t size = ((uint64_t)__builtin_bswap32(cells[2]) << 32) |
                        __builtin_bswap32(cells[3]);

        if (addr != 0 && size > 0) {
            dev->phys_base = addr;
            dev->size = (uint32_t)size;
            closedir(dir);
            return 0;
        }
    }
    closedir(dir);
    return -1;
}

int virtio_mmio_find(struct virtio_mmio_dev *dev)
{
    memset(dev, 0, sizeof(*dev));

    /* Try platform devices first, then device tree */
    if (scan_platform_devices(dev) < 0 && scan_device_tree(dev) < 0)
        return -1;

    /* Map the MMIO region via /dev/mem */
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0)
        return -1;

    uint64_t page_mask = PAGE_SIZE - 1;
    uint64_t page_base = dev->phys_base & ~page_mask;
    uint32_t page_offset = dev->phys_base & page_mask;
    uint32_t map_size = dev->size + page_offset;

    void *mapped = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, page_base);
    close(fd);
    if (mapped == MAP_FAILED)
        return -1;

    dev->base = (volatile void *)((char *)mapped + page_offset);

    /* Validate magic value */
    uint32_t magic = mmio_read32(dev, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC)
        return -1;

    /* Check version (must be 2 for modern/non-legacy) */
    uint32_t version = mmio_read32(dev, VIRTIO_MMIO_VERSION);
    if (version < 1)
        return -1;

    dev->device_id = mmio_read32(dev, VIRTIO_MMIO_DEVICE_ID);
    return 0;
}

int virtio_mmio_init(struct virtio_mmio_dev *dev)
{
    /* Reset */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();

    /* Acknowledge */
    mmio_write32(dev, VIRTIO_MMIO_STATUS, VIRTIO_MMIO_STATUS_ACKNOWLEDGE);
    __sync_synchronize();

    /* Driver */
    uint32_t status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_DRIVER);
    __sync_synchronize();

    /* Negotiate zero features */
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    mmio_write32(dev, VIRTIO_MMIO_DRIVER_FEATURES, 0);

    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 status | VIRTIO_MMIO_STATUS_FEATURES_OK);
    __sync_synchronize();

    status = mmio_read32(dev, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_MMIO_STATUS_FEATURES_OK))
        return -1;

    return 0;
}

void virtio_mmio_reset(struct virtio_mmio_dev *dev)
{
    mmio_write32(dev, VIRTIO_MMIO_STATUS, 0);
    __sync_synchronize();
}

void virtio_mmio_kick(struct virtio_mmio_dev *dev, uint16_t queue)
{
    mmio_write32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, queue);
    __sync_synchronize();
}
