/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <time.h>
#include <unistd.h>

#include "lib/util.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/vring.h"

#define PERF_BLOCK_SIZE 4096
#define DEFAULT_ITERATIONS 10000
#define DEFAULT_ROUNDS 5
#define DEFAULT_WARMUP 1000
#define QUEUE_SIZE 128
#define MAX_QUEUES 16

struct perf_workload {
    const char *device;
    const char *name;
    uint16_t device_id;
    uint16_t queue;
    uint32_t request_size;
    uint8_t *status;
    struct vring *response_vr;
};

static void shutdown_guest(int status)
{
    fflush(stdout);
    sync();
    reboot(RB_POWER_OFF);
    _exit(status);
}

static unsigned read_cmdline_value(const char *name, unsigned fallback)
{
    FILE *file = fopen("/proc/cmdline", "r");
    char cmdline[4096];
    char key[64];

    if (!file)
        return fallback;
    if (!fgets(cmdline, sizeof(cmdline), file)) {
        fclose(file);
        return fallback;
    }
    fclose(file);

    snprintf(key, sizeof(key), "%s=", name);
    char *value = strstr(cmdline, key);
    if (!value)
        return fallback;
    value += strlen(key);
    unsigned long parsed = strtoul(value, NULL, 10);
    if (parsed == 0 || parsed > UINT32_MAX)
        return fallback;
    return (unsigned)parsed;
}

static void read_cmdline_string(const char *name, char *value,
                                size_t value_size, const char *fallback)
{
    FILE *file = fopen("/proc/cmdline", "r");
    char cmdline[4096];
    char key[64];

    snprintf(value, value_size, "%s", fallback);
    if (!file)
        return;
    if (!fgets(cmdline, sizeof(cmdline), file)) {
        fclose(file);
        return;
    }
    fclose(file);
    snprintf(key, sizeof(key), "%s=", name);
    char *start = strstr(cmdline, key);
    if (!start)
        return;
    start += strlen(key);
    size_t length = strcspn(start, " \n");
    if (length == 0 || length >= value_size)
        return;
    memcpy(value, start, length);
    value[length] = '\0';
}

static uint64_t elapsed_ns(const struct timespec *start,
                           const struct timespec *end)
{
    int64_t seconds = end->tv_sec - start->tv_sec;
    int64_t nanoseconds = end->tv_nsec - start->tv_nsec;

    return (uint64_t)(seconds * 1000000000LL + nanoseconds);
}

static int run_requests(struct virtio_dev *dev, struct vring *vr,
                        struct vring *response_vr, uint8_t *status,
                        unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        uint16_t expected = vr->used->idx + 1;
        uint16_t response_expected = 0;

        if (status)
            *status = 0xff;
        if (response_vr) {
            response_expected = response_vr->used->idx + 1;
            vring_submit(response_vr, 0);
            virtio_pci_kick(dev, response_vr->queue);
        }
        vring_submit(vr, 0);
        virtio_pci_kick(dev, vr->queue);
        while (vr->used->idx != expected)
            __sync_synchronize();
        while (response_vr && response_vr->used->idx != response_expected)
            __sync_synchronize();
        if (status && *status != VIRTIO_BLK_S_OK)
            return -1;
    }
    return 0;
}

static int setup_blk(struct vring *vr, struct perf_workload *workload)
{
    struct virtio_blk_outhdr *header = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    header->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(header), sizeof(*header),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), PERF_BLOCK_SIZE,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    workload->status = status;
    return 3;
}

static int setup_rng(struct vring *vr, struct perf_workload *workload)
{
    uint8_t *data = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(data), PERF_BLOCK_SIZE,
                       VRING_DESC_F_WRITE, 0);
    workload->status = NULL;
    return 1;
}

static int setup_net(struct vring *vr, struct perf_workload *workload)
{
    struct virtio_net_hdr *header = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    memset(frame, 0xff, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;
    memset(frame + 14, 0x42, 50);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(header), sizeof(*header),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 64, 0, 0);
    workload->status = NULL;
    return 2;
}

static int setup_vsock(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    struct virtio_vsock_hdr *header = vv_alloc_pages(1);

    if (!dev->device_cfg || dev->device_cfg_length < sizeof(uint64_t))
        return -1;
    header->src_cid = *(volatile uint64_t *)dev->device_cfg;
    header->dst_cid = 2;
    header->src_port = 1234;
    header->dst_port = 5678;
    header->type = VIRTIO_VSOCK_TYPE_STREAM;
    header->op = VIRTIO_VSOCK_OP_REQUEST;
    header->buf_alloc = 262144;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(header), sizeof(*header), 0, 0);
    workload->status = NULL;
    return 1;
}

static int select_workload(const char *device, struct perf_workload *workload)
{
    if (strcmp(device, "blk") == 0) {
        *workload = (struct perf_workload){
            device, "blk_read", VIRTIO_PCI_DEVICE_BLK, 0,
            PERF_BLOCK_SIZE, NULL, NULL
        };
    } else if (strcmp(device, "rng") == 0) {
        *workload = (struct perf_workload){
            device, "rng_fill", VIRTIO_PCI_DEVICE_RNG, 0,
            PERF_BLOCK_SIZE, NULL, NULL
        };
    } else if (strcmp(device, "net") == 0) {
        *workload = (struct perf_workload){
            device, "net_tx", VIRTIO_PCI_DEVICE_NET, 1, 64, NULL, NULL
        };
    } else if (strcmp(device, "vsock") == 0) {
        *workload = (struct perf_workload){
            device, "vsock_roundtrip", VIRTIO_PCI_DEVICE_VSOCK, 1,
            sizeof(struct virtio_vsock_hdr), NULL, NULL
        };
    } else {
        return -1;
    }
    return 0;
}

int main(void)
{
    struct virtio_dev dev;
    struct vring queues[MAX_QUEUES];
    struct vring *vr;
    struct perf_workload workload;
    struct timespec start;
    struct timespec end;
    char device[16];

    if (getpid() != 1) {
        fprintf(stderr, "perf guest must run as PID 1\n");
        return 1;
    }

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    unsigned iterations = read_cmdline_value("vv.perf_iterations",
                                              DEFAULT_ITERATIONS);
    unsigned rounds = read_cmdline_value("vv.perf_rounds", DEFAULT_ROUNDS);
    unsigned warmup = read_cmdline_value("vv.perf_warmup", DEFAULT_WARMUP);
    read_cmdline_string("vv.perf_device", device, sizeof(device), "blk");

    if (select_workload(device, &workload) < 0) {
        printf("VVPERF error=unsupported_device\n");
        shutdown_guest(1);
    }

    if (virtio_pci_find(workload.device_id, &dev) < 0 ||
        virtio_pci_init(&dev) < 0) {
        printf("VVPERF error=device_init\n");
        shutdown_guest(1);
    }

    uint16_t queue_count = dev.common->num_queues;
    if (queue_count <= workload.queue || queue_count > MAX_QUEUES) {
        printf("VVPERF error=queue_count\n");
        shutdown_guest(1);
    }
    for (uint16_t queue = 0; queue < queue_count; queue++) {
        dev.common->queue_select = queue;
        __sync_synchronize();
        uint16_t queue_size = dev.common->queue_size;
        if (queue_size > QUEUE_SIZE)
            queue_size = QUEUE_SIZE;
        if (queue_size == 0) {
            printf("VVPERF error=queue_size\n");
            shutdown_guest(1);
        }
        vring_alloc(&queues[queue], queue_size);
        vring_attach(&dev, &queues[queue], queue);
    }
    vr = &queues[workload.queue];

    int descriptor_count;
    if (workload.device_id == VIRTIO_PCI_DEVICE_BLK)
        descriptor_count = setup_blk(vr, &workload);
    else if (workload.device_id == VIRTIO_PCI_DEVICE_RNG)
        descriptor_count = setup_rng(vr, &workload);
    else if (workload.device_id == VIRTIO_PCI_DEVICE_NET)
        descriptor_count = setup_net(vr, &workload);
    else
        descriptor_count = setup_vsock(&dev, vr, &workload);
    if (descriptor_count < 0 || vr->size < descriptor_count) {
        printf("VVPERF error=request_setup\n");
        shutdown_guest(1);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK) {
        struct virtio_vsock_hdr *response = vv_alloc_pages(1);

        vring_raw_set_desc(&queues[0], 0, vv_virt_to_phys(response),
                           sizeof(*response), VRING_DESC_F_WRITE, 0);
        workload.response_vr = &queues[0];
    }

    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    if (run_requests(&dev, vr, workload.response_vr, workload.status,
                     warmup) < 0) {
        printf("VVPERF error=warmup_io\n");
        shutdown_guest(1);
    }

    for (unsigned round = 0; round < rounds; round++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        int result = run_requests(&dev, vr, workload.response_vr,
                                  workload.status, iterations);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (result < 0) {
            printf("VVPERF error=measured_io\n");
            shutdown_guest(1);
        }

        uint64_t duration_ns = elapsed_ns(&start, &end);
                printf("VVPERF workload=%s round=%u block_size=%u "
               "iterations=%u duration_ns=%llu\n",
                             workload.name, round + 1, workload.request_size, iterations,
               (unsigned long long)duration_ns);
    }
    shutdown_guest(0);
    return 0;
}