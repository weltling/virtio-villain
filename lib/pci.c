/* SPDX-License-Identifier: Apache-2.0 */
#include "pci.h"
#include "util.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int pci_find_device(uint16_t vendor, uint16_t device, char *slot, size_t len)
{
    DIR *d = opendir("/sys/bus/pci/devices");
    if (!d)
        return -1;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        char path[512];
        FILE *f;
        unsigned v, dev;

        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", ent->d_name);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%x", &v) != 1) { fclose(f); continue; }
        fclose(f);

        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", ent->d_name);
        f = fopen(path, "r");
        if (!f) continue;
        if (fscanf(f, "%x", &dev) != 1) { fclose(f); continue; }
        fclose(f);

        if (v == vendor && dev == device) {
            snprintf(slot, len, "/sys/bus/pci/devices/%s", ent->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

void pci_enable(const char *slot)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/enable", slot);
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        ssize_t r = write(fd, "1", 1);
        (void)r;
        close(fd);
    }
}

int pci_cfg_open(const char *slot)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/config", slot);
    return open(path, O_RDWR);
}

uint8_t pci_cfg_read8(int fd, uint32_t offset)
{
    uint8_t val = 0;
    ssize_t r = pread(fd, &val, 1, offset);
    (void)r;
    return val;
}

uint16_t pci_cfg_read16(int fd, uint32_t offset)
{
    uint16_t val = 0;
    ssize_t r = pread(fd, &val, 2, offset);
    (void)r;
    return val;
}

uint32_t pci_cfg_read32(int fd, uint32_t offset)
{
    uint32_t val = 0;
    ssize_t r = pread(fd, &val, 4, offset);
    (void)r;
    return val;
}

void pci_cfg_write8(int fd, uint32_t offset, uint8_t val)
{
    ssize_t r = pwrite(fd, &val, 1, offset);
    (void)r;
}

void pci_cfg_write32(int fd, uint32_t offset, uint32_t val)
{
    ssize_t r = pwrite(fd, &val, 4, offset);
    (void)r;
}

volatile void *pci_map_bar(const char *slot, int bar)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/resource%d", slot, bar);
    int fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0)
        return NULL;

    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;
    if (size == 0)
        size = PAGE_SIZE;

    void *base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED)
        return NULL;
    return (volatile void *)base;
}

uint64_t pci_bar_phys(const char *slot, int bar)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/resource", slot);
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;

    uint64_t start = 0, end = 0, flags = 0;
    int i;
    for (i = 0; i <= bar; i++) {
        if (fscanf(f, "%lx %lx %lx", &start, &end, &flags) != 3) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    if (end <= start)
        return 0;
    return start;
}
