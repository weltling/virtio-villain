/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_UTIL_H
#define VV_UTIL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define PAGE_SIZE 4096

#define vv_log(fmt, ...) \
    fprintf(stderr, "[vv] " fmt "\n", ##__VA_ARGS__)

#define vv_die(fmt, ...) do { \
    fprintf(stderr, "[vv] FATAL: " fmt "\n", ##__VA_ARGS__); \
    exit(1); \
} while (0)

/* Allocate a page aligned, locked, zeroed buffer. */
static inline void *vv_alloc_pages(size_t n)
{
    void *p = mmap(NULL, n * PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
    if (p == MAP_FAILED)
        vv_die("mmap %zu pages failed", n);
    memset(p, 0, n * PAGE_SIZE);
    return p;
}

/* Convert a userspace virtual address to a guest physical address. */
static inline uint64_t vv_virt_to_phys(void *vaddr)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0)
        vv_die("open pagemap");
    uint64_t page = (uint64_t)vaddr / PAGE_SIZE;
    uint64_t entry;
    if (pread(fd, &entry, 8, page * 8) != 8) {
        close(fd);
        vv_die("pread pagemap");
    }
    close(fd);
    if (!(entry & (1ULL << 63)))
        vv_die("page not present for %p", vaddr);
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    return pfn * PAGE_SIZE + ((uint64_t)vaddr & (PAGE_SIZE - 1));
}

/* Return the first byte past the highest System RAM region in
 * /proc/iomem, or 0 if it cannot be determined. */
static inline uint64_t vv_parse_ram_top(void)
{
    FILE *f = fopen("/proc/iomem", "r");
    if (!f)
        return 0;

    uint64_t top = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        uint64_t lo, hi;
        char rest[128];
        if (sscanf(line, " %lx-%lx : %127[^\n]", &lo, &hi, rest) == 3) {
            if (strstr(rest, "System RAM") && hi + 1 > top)
                top = hi + 1;
        }
    }
    fclose(f);
    return top;
}

#endif /* VV_UTIL_H */
