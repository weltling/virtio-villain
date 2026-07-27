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

/*
 * Allocate one locked, zeroed page whose guest physical base sits as
 * close as possible below ram_top. On a device that fills a writable
 * descriptor front to back before it faults on the part past the end of
 * RAM, the in-range prefix it writes is [base, ram_top). Placing base
 * near the top keeps that prefix small and high in RAM, away from the
 * init image low in RAM, so a "huge len past RAM" descriptor cannot
 * clobber init and crash the guest before the test reports its verdict.
 *
 * Writes the guest physical base to *phys_out. Returns NULL if no page
 * below ram_top could be obtained, in which case the caller should skip.
 */
static inline void *vv_alloc_page_near_ram_top(uint64_t ram_top,
                                               uint64_t *phys_out)
{
    /*
     * Grab a chunk of pages and keep the one with the highest PFN still
     * below ram_top. A larger chunk raises the odds of landing near the
     * top; shrink on failure so a small guest can still satisfy it.
     */
    static const size_t try_pages[] = {
        16384, 8192, 4096, 2048, 1024, 256, 1,
    };
    uint8_t *region = MAP_FAILED;
    size_t region_pages = 0;

    for (size_t t = 0; t < sizeof(try_pages) / sizeof(try_pages[0]); t++) {
        region = mmap(NULL, try_pages[t] * PAGE_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
        if (region != MAP_FAILED) {
            region_pages = try_pages[t];
            break;
        }
    }
    if (region == MAP_FAILED)
        return NULL;
    memset(region, 0, region_pages * PAGE_SIZE);

    void *best = NULL;
    uint64_t best_phys = 0;
    for (size_t i = 0; i < region_pages; i++) {
        uint8_t *pg = region + i * PAGE_SIZE;
        uint64_t phys = vv_virt_to_phys(pg);
        if (phys < ram_top && phys > best_phys) {
            best_phys = phys;
            best = pg;
        }
    }
    if (!best)
        return NULL;

    *phys_out = best_phys;
    return best;
}

/*
 * Same as vv_alloc_page_near_ram_top but falls back to a plain page when
 * the top of RAM cannot be determined, so a caller that must run on
 * every target still gets a usable buffer. Writes the guest physical
 * base to *phys_out.
 */
static inline void *vv_alloc_page_high(uint64_t *phys_out)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top) {
        void *near = vv_alloc_page_near_ram_top(ram_top, phys_out);
        if (near)
            return near;
    }
    void *p = vv_alloc_pages(1);
    *phys_out = vv_virt_to_phys(p);
    return p;
}

#endif /* VV_UTIL_H */
