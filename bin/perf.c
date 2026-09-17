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

#include "lib/perf_engine.h"
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
#define MAX_WORKLOAD_QUEUES 2

enum perf_request_result {
    PERF_REQUEST_OK,
    PERF_REQUEST_PREPARE,
    PERF_REQUEST_SUBMIT,
    PERF_REQUEST_COMPLETE,
    PERF_REQUEST_VALIDATE,
};

struct perf_queue_request {
    struct vring *vr;
    struct perf_request_slot slot;
};

struct perf_workload;

struct perf_workload_ops {
    int (*prepare)(struct virtio_dev *dev, struct vring *vr,
                   struct perf_workload *workload);
    void (*reset)(struct perf_workload *workload);
    int (*validate)(struct perf_workload *workload, const uint32_t *lengths);
    int (*cleanup)(struct perf_workload *workload);
};

struct perf_workload {
    const char *device;
    const char *operation;
    uint16_t device_id;
    uint16_t queue;
    uint32_t request_size;
    const struct perf_workload_ops *ops;
    struct perf_queue_request requests[MAX_WORKLOAD_QUEUES];
    unsigned request_count;
    unsigned operations_per_iteration;
    uint8_t *status;
    struct virtio_vsock_hdr *vsock_request;
    struct virtio_vsock_hdr *response;
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

static int submit_slot(struct virtio_dev *dev, struct vring *vr,
                       struct perf_request_slot *slot)
{
    if (perf_slot_submit(slot) < 0)
        return -1;
    vring_submit(vr, slot->head);
    virtio_pci_kick(dev, vr->queue);
    return 0;
}

static int complete_slot(struct vring *vr, struct perf_request_slot *slot,
                         uint16_t used_idx, uint32_t *len)
{
    uint16_t expected = used_idx + 1;

    while (vr->used->idx != expected)
        __sync_synchronize();
    struct vring_used_elem *used = &vr->used->ring[used_idx % vr->size];
    if (perf_slot_complete(slot, used->id) < 0)
        return -1;
    if (len)
        *len = used->len;
    return 0;
}

static enum perf_request_result run_requests(struct virtio_dev *dev,
                                             struct perf_workload *workload,
                                             unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        uint16_t used_idx[MAX_WORKLOAD_QUEUES];
        uint32_t used_len[MAX_WORKLOAD_QUEUES] = {0};

        workload->ops->reset(workload);
        for (unsigned request = 0; request < workload->request_count;
             request++) {
            struct perf_queue_request *queue_request =
                &workload->requests[request];

            used_idx[request] = queue_request->vr->used->idx;
            if (perf_slot_prepare(&queue_request->slot) < 0)
                return PERF_REQUEST_PREPARE;
            if (submit_slot(dev, queue_request->vr,
                            &queue_request->slot) < 0)
                return PERF_REQUEST_SUBMIT;
        }
        for (unsigned request = 0; request < workload->request_count;
             request++) {
            struct perf_queue_request *queue_request =
                &workload->requests[request];
            if (complete_slot(queue_request->vr, &queue_request->slot,
                              used_idx[request], &used_len[request]) < 0)
                return PERF_REQUEST_COMPLETE;
        }
        if (workload->ops->validate(workload, used_len) < 0)
            return PERF_REQUEST_VALIDATE;
        int cleanup_request = workload->ops->cleanup(workload);
        if (cleanup_request >= 0) {
            struct perf_queue_request *queue_request =
                &workload->requests[cleanup_request];
            uint16_t cleanup_used_idx = queue_request->vr->used->idx;

            if (perf_slot_prepare(&queue_request->slot) < 0)
                return PERF_REQUEST_PREPARE;
            if (submit_slot(dev, queue_request->vr,
                            &queue_request->slot) < 0)
                return PERF_REQUEST_SUBMIT;
            if (complete_slot(queue_request->vr, &queue_request->slot,
                              cleanup_used_idx, NULL) < 0)
                return PERF_REQUEST_COMPLETE;
        }
    }
    return PERF_REQUEST_OK;
}

static const char *request_error(enum perf_request_result result)
{
    switch (result) {
    case PERF_REQUEST_PREPARE:
        return "request_prepare";
    case PERF_REQUEST_SUBMIT:
        return "request_submit";
    case PERF_REQUEST_COMPLETE:
        return "request_complete";
    case PERF_REQUEST_VALIDATE:
        return "request_validate";
    default:
        return "request_unknown";
    }
}

static int prepare_blk(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    struct virtio_blk_outhdr *header = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    (void)dev;
    header->type = VIRTIO_BLK_T_IN;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(header), sizeof(*header),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), PERF_BLOCK_SIZE,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    workload->status = status;
    workload->requests[0].vr = vr;
    perf_slot_init(&workload->requests[0].slot, 0, 0, header);
    workload->request_count = 1;
    workload->operations_per_iteration = 1;
    return 3;
}

static void reset_blk(struct perf_workload *workload)
{
    *workload->status = 0xff;
}

static int validate_blk(struct perf_workload *workload,
                        const uint32_t *lengths)
{
    (void)lengths;
    return *workload->status == VIRTIO_BLK_S_OK ? 0 : -1;
}

static int prepare_rng(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    uint8_t *data = vv_alloc_pages(1);

    (void)dev;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(data), PERF_BLOCK_SIZE,
                       VRING_DESC_F_WRITE, 0);
    workload->requests[0].vr = vr;
    perf_slot_init(&workload->requests[0].slot, 0, 0, data);
    workload->request_count = 1;
    workload->operations_per_iteration = 1;
    return 1;
}

static void reset_nop(struct perf_workload *workload)
{
    (void)workload;
}

static int cleanup_nop(struct perf_workload *workload)
{
    (void)workload;
    return -1;
}

static int validate_rng(struct perf_workload *workload,
                        const uint32_t *lengths)
{
    return lengths[0] == workload->request_size ? 0 : -1;
}

static int prepare_net(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    struct virtio_net_hdr *header = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);

    (void)dev;
    memset(frame, 0xff, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08;
    frame[13] = 0x00;
    memset(frame + 14, 0x42, 50);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(header), sizeof(*header),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 64, 0, 0);
    workload->requests[0].vr = vr;
    perf_slot_init(&workload->requests[0].slot, 0, 0, header);
    workload->request_count = 1;
    workload->operations_per_iteration = 1;
    return 2;
}

static int validate_net(struct perf_workload *workload,
                        const uint32_t *lengths)
{
    (void)workload;
    return lengths[0] == 0 ? 0 : -1;
}

static int prepare_vsock(struct virtio_dev *dev, struct vring *vr,
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
    workload->requests[1].vr = vr;
    perf_slot_init(&workload->requests[1].slot, 0, 0, header);
    workload->request_count = 2;
    workload->operations_per_iteration = 3;
    workload->vsock_request = header;
    return 1;
}

static void reset_vsock(struct perf_workload *workload)
{
    workload->vsock_request->src_port++;
    if (workload->vsock_request->src_port == 0)
        workload->vsock_request->src_port = 1024;
    workload->vsock_request->op = VIRTIO_VSOCK_OP_REQUEST;
    memset(workload->response, 0, sizeof(*workload->response));
}

static int validate_vsock(struct perf_workload *workload,
                          const uint32_t *lengths)
{
    struct virtio_vsock_hdr *request = workload->vsock_request;
    struct virtio_vsock_hdr *response = workload->response;

    if (lengths[0] == sizeof(*response) &&
        response->src_cid == request->dst_cid &&
        response->dst_cid == request->src_cid &&
        response->src_port == request->dst_port &&
        response->dst_port == request->src_port &&
        response->type == request->type &&
        response->op == VIRTIO_VSOCK_OP_RESPONSE && response->len == 0)
        return 0;
    printf("VVPERF_DETAIL type=vsock_response op=%u rx_len=%u tx_len=%u "
           "src_port=%u dst_port=%u\n",
           response->op, lengths[0], lengths[1],
           response->src_port, response->dst_port);
    return -1;
}

static int cleanup_vsock(struct perf_workload *workload)
{
    workload->vsock_request->op = VIRTIO_VSOCK_OP_RST;
    return 1;
}

static const struct perf_workload_ops blk_ops = {
    prepare_blk, reset_blk, validate_blk, cleanup_nop
};
static const struct perf_workload_ops rng_ops = {
    prepare_rng, reset_nop, validate_rng, cleanup_nop
};
static const struct perf_workload_ops net_ops = {
    prepare_net, reset_nop, validate_net, cleanup_nop
};
static const struct perf_workload_ops vsock_ops = {
    prepare_vsock, reset_vsock, validate_vsock, cleanup_vsock
};

static int select_workload(const char *device, struct perf_workload *workload)
{
    if (strcmp(device, "blk") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "blk_read",
            .device_id = VIRTIO_PCI_DEVICE_BLK,
            .queue = 0,
            .request_size = PERF_BLOCK_SIZE,
            .ops = &blk_ops,
        };
    } else if (strcmp(device, "rng") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "rng_fill",
            .device_id = VIRTIO_PCI_DEVICE_RNG,
            .queue = 0,
            .request_size = PERF_BLOCK_SIZE,
            .ops = &rng_ops,
        };
    } else if (strcmp(device, "net") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "net_tx",
            .device_id = VIRTIO_PCI_DEVICE_NET,
            .queue = 1,
            .request_size = 64,
            .ops = &net_ops,
        };
    } else if (strcmp(device, "vsock") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "vsock_roundtrip",
            .device_id = VIRTIO_PCI_DEVICE_VSOCK,
            .queue = 1,
            .request_size = sizeof(struct virtio_vsock_hdr),
            .ops = &vsock_ops,
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
    char experiment[16];
    char changed[32];

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
    read_cmdline_string("vv.perf_experiment", experiment,
                        sizeof(experiment), "queue");
    read_cmdline_string("vv.perf_changed", changed, sizeof(changed), "none");

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

    int descriptor_count = workload.ops->prepare(&dev, vr, &workload);
    if (descriptor_count < 0 || vr->size < descriptor_count) {
        printf("VVPERF error=request_setup\n");
        shutdown_guest(1);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK) {
        workload.response = vv_alloc_pages(1);

        vring_raw_set_desc(&queues[0], 0, vv_virt_to_phys(workload.response),
                           sizeof(*workload.response),
                           VRING_DESC_F_WRITE, 0);
        workload.requests[0].vr = &queues[0];
        perf_slot_init(&workload.requests[0].slot, 0, 0,
                   workload.response);
    }

    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    enum perf_request_result result = run_requests(&dev, &workload, warmup);
    if (result != PERF_REQUEST_OK) {
        printf("VVPERF error=%s phase=warmup\n", request_error(result));
        shutdown_guest(1);
    }

    for (unsigned round = 0; round < rounds; round++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        result = run_requests(&dev, &workload, iterations);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (result != PERF_REQUEST_OK) {
            printf("VVPERF error=%s phase=measured\n",
                   request_error(result));
            shutdown_guest(1);
        }

        uint64_t duration_ns = elapsed_ns(&start, &end);
        uint64_t queue_operations = workload.operations_per_iteration;
        uint64_t queue_requests = iterations * queue_operations;

        printf("VVPERF version=3 experiment=%s changed=%s "
               "workload=%s operation=%s round=%u request_bytes=%u "
               "iterations=%u duration_ns=%llu queue_format=split "
               "queue_depth=1 batch_size=1 submissions=%llu "
               "completions=%llu notifications=%llu timing_mode=throughput "
               "clock_source=monotonic features=0x0\n",
               experiment, changed, workload.device, workload.operation,
               round + 1, workload.request_size, iterations,
               (unsigned long long)duration_ns,
               (unsigned long long)queue_requests,
               (unsigned long long)queue_requests,
               (unsigned long long)queue_requests);
    }
    shutdown_guest(0);
    return 0;
}