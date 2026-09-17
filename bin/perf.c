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
#define MAX_QUEUE_DEPTH 16
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
    struct perf_request_slot slots[MAX_QUEUE_DEPTH];
};

struct perf_workload;

struct perf_workload_ops {
    int (*prepare)(struct virtio_dev *dev, struct vring *vr,
                   struct perf_workload *workload);
    void (*reset)(struct perf_workload *workload, unsigned slot);
    int (*validate)(struct perf_workload *workload, unsigned slot,
                    const uint32_t *lengths);
    int (*cleanup)(struct perf_workload *workload, unsigned slot);
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
    unsigned queue_depth;
    uint8_t *status[MAX_QUEUE_DEPTH];
    struct virtio_vsock_hdr *vsock_request[MAX_QUEUE_DEPTH];
    struct virtio_vsock_hdr *response[MAX_QUEUE_DEPTH];
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

static int submit_slots(struct virtio_dev *dev,
                        struct perf_queue_request *request,
                        unsigned first, unsigned count,
                        struct perf_run_stats *stats)
{
    uint16_t heads[MAX_QUEUE_DEPTH];

    for (unsigned i = 0; i < count; i++) {
        struct perf_request_slot *slot = &request->slots[first + i];

        if (perf_slot_submit(slot) < 0)
            return -1;
        heads[i] = slot->head;
    }
    vring_submit_batch(request->vr, heads, count);
    virtio_pci_kick(dev, request->vr->queue);
    perf_stats_submit(stats, count);
    return 0;
}

static int complete_next(struct perf_queue_request *request,
                         unsigned active_slots, uint16_t *used_idx,
                         uint32_t *lengths, struct perf_run_stats *stats)
{
    while (request->vr->used->idx == *used_idx)
        __sync_synchronize();
    struct vring_used_elem *used =
        &request->vr->used->ring[*used_idx % request->vr->size];
    int slot = perf_slots_complete(request->slots, active_slots, used->id);
    if (slot < 0)
        return -1;
    (*used_idx)++;
    perf_stats_complete(stats);
    if (lengths)
        lengths[slot] = used->len;
    return slot;
}

static enum perf_request_result run_requests(struct virtio_dev *dev,
                                             struct perf_workload *workload,
                                             unsigned count,
                                             unsigned batch_size,
                                             struct perf_run_stats *stats)
{
    uint16_t used_idx[MAX_WORKLOAD_QUEUES];

    for (unsigned request = 0; request < workload->request_count; request++)
        used_idx[request] = workload->requests[request].vr->used->idx;
    for (unsigned completed = 0; completed < count;) {
        unsigned active_slots = workload->queue_depth;
        uint32_t used_len[MAX_QUEUE_DEPTH][MAX_WORKLOAD_QUEUES] = {{0}};

        if (active_slots > count - completed)
            active_slots = count - completed;
        for (unsigned slot = 0; slot < active_slots; slot++)
            workload->ops->reset(workload, slot);
        for (unsigned request = 0; request < workload->request_count;
             request++) {
            struct perf_queue_request *queue_request =
                &workload->requests[request];

            for (unsigned slot = 0; slot < active_slots; slot++)
                if (perf_slot_prepare(&queue_request->slots[slot]) < 0)
                    return PERF_REQUEST_PREPARE;
            for (unsigned first = 0; first < active_slots;
                 first += batch_size) {
                unsigned batch_count = batch_size;

                if (batch_count > active_slots - first)
                    batch_count = active_slots - first;
                if (submit_slots(dev, queue_request, first, batch_count,
                                 stats) < 0)
                    return PERF_REQUEST_SUBMIT;
            }
        }
        for (unsigned request = 0; request < workload->request_count;
             request++) {
            struct perf_queue_request *queue_request =
                &workload->requests[request];

            for (unsigned completion = 0; completion < active_slots;
                 completion++) {
                uint32_t lengths[MAX_QUEUE_DEPTH] = {0};
                int slot = complete_next(queue_request, active_slots,
                                         &used_idx[request], lengths, stats);
                if (slot < 0)
                    return PERF_REQUEST_COMPLETE;
                used_len[slot][request] = lengths[slot];
            }
        }
        int cleanup_request = -1;
        for (unsigned slot = 0; slot < active_slots; slot++) {
            if (workload->ops->validate(workload, slot,
                                        used_len[slot]) < 0)
                return PERF_REQUEST_VALIDATE;
            int slot_cleanup_request = workload->ops->cleanup(workload, slot);
            if (slot_cleanup_request >= 0) {
                struct perf_queue_request *queue_request =
                    &workload->requests[slot_cleanup_request];

                if (perf_slot_prepare(&queue_request->slots[slot]) < 0)
                    return PERF_REQUEST_PREPARE;
                if (cleanup_request >= 0 &&
                    cleanup_request != slot_cleanup_request)
                    return PERF_REQUEST_SUBMIT;
                cleanup_request = slot_cleanup_request;
            }
        }
        if (cleanup_request >= 0) {
            struct perf_queue_request *queue_request =
                &workload->requests[cleanup_request];

            for (unsigned first = 0; first < active_slots;
                 first += batch_size) {
                unsigned batch_count = batch_size;

                if (batch_count > active_slots - first)
                    batch_count = active_slots - first;
                if (submit_slots(dev, queue_request, first, batch_count,
                                 stats) < 0)
                    return PERF_REQUEST_SUBMIT;
            }
            for (unsigned completion = 0; completion < active_slots;
                 completion++) {
                if (complete_next(queue_request, active_slots,
                                  &used_idx[cleanup_request], NULL,
                                  stats) < 0)
                    return PERF_REQUEST_COMPLETE;
            }
        }
        completed += active_slots;
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
    (void)dev;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        struct virtio_blk_outhdr *header = vv_alloc_pages(1);
        uint8_t *data = vv_alloc_pages(1);
        uint8_t *status = vv_alloc_pages(1);
        uint16_t head = slot * 3;

        header->type = VIRTIO_BLK_T_IN;
        vring_raw_set_desc(vr, head, vv_virt_to_phys(header), sizeof(*header),
                           VRING_DESC_F_NEXT, head + 1);
        vring_raw_set_desc(vr, head + 1, vv_virt_to_phys(data),
                           PERF_BLOCK_SIZE,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, head + 2);
        vring_raw_set_desc(vr, head + 2, vv_virt_to_phys(status), 1,
                           VRING_DESC_F_WRITE, 0);
        workload->status[slot] = status;
        perf_slot_init(&workload->requests[0].slots[slot], head, head,
                       header);
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth * 3;
}

static void reset_blk(struct perf_workload *workload, unsigned slot)
{
    *workload->status[slot] = 0xff;
}

static int validate_blk(struct perf_workload *workload,
                        unsigned slot,
                        const uint32_t *lengths)
{
    (void)lengths;
    return *workload->status[slot] == VIRTIO_BLK_S_OK ? 0 : -1;
}

static int prepare_rng(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    (void)dev;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        uint8_t *data = vv_alloc_pages(1);

        vring_raw_set_desc(vr, slot, vv_virt_to_phys(data), PERF_BLOCK_SIZE,
                           VRING_DESC_F_WRITE, 0);
        perf_slot_init(&workload->requests[0].slots[slot], slot, slot, data);
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth;
}

static void reset_nop(struct perf_workload *workload, unsigned slot)
{
    (void)workload;
    (void)slot;
}

static int cleanup_nop(struct perf_workload *workload, unsigned slot)
{
    (void)workload;
    (void)slot;
    return -1;
}

static int validate_rng(struct perf_workload *workload,
                        unsigned slot,
                        const uint32_t *lengths)
{
    (void)slot;
    return lengths[0] == workload->request_size ? 0 : -1;
}

static int prepare_net(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    (void)dev;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        struct virtio_net_hdr *header = vv_alloc_pages(1);
        uint8_t *frame = vv_alloc_pages(1);
        uint16_t head = slot * 2;

        memset(frame, 0xff, 6);
        memset(frame + 6, 0x02, 6);
        frame[12] = 0x08;
        frame[13] = 0x00;
        memset(frame + 14, 0x42, 50);
        vring_raw_set_desc(vr, head, vv_virt_to_phys(header), sizeof(*header),
                           VRING_DESC_F_NEXT, head + 1);
        vring_raw_set_desc(vr, head + 1, vv_virt_to_phys(frame), 64, 0, 0);
        perf_slot_init(&workload->requests[0].slots[slot], head, head,
                       header);
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth * 2;
}

static int validate_net(struct perf_workload *workload,
                        unsigned slot,
                        const uint32_t *lengths)
{
    (void)workload;
    (void)slot;
    return lengths[0] == 0 ? 0 : -1;
}

static int prepare_vsock(struct virtio_dev *dev, struct vring *vr,
                         struct perf_workload *workload)
{
    if (!dev->device_cfg || dev->device_cfg_length < sizeof(uint64_t))
        return -1;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        struct virtio_vsock_hdr *header = vv_alloc_pages(1);

        header->src_cid = *(volatile uint64_t *)dev->device_cfg;
        header->dst_cid = 2;
        header->src_port = 1234 + slot;
        header->dst_port = 5678;
        header->type = VIRTIO_VSOCK_TYPE_STREAM;
        header->op = VIRTIO_VSOCK_OP_REQUEST;
        header->buf_alloc = 262144;
        vring_raw_set_desc(vr, slot, vv_virt_to_phys(header),
                           sizeof(*header), 0, 0);
        perf_slot_init(&workload->requests[1].slots[slot], slot, slot,
                       header);
        workload->vsock_request[slot] = header;
    }
    workload->requests[1].vr = vr;
    workload->request_count = 2;
    return workload->queue_depth;
}

static void reset_vsock(struct perf_workload *workload, unsigned slot)
{
    struct virtio_vsock_hdr *request = workload->vsock_request[slot];

    request->src_port += workload->queue_depth;
    if (request->src_port < 1024)
        request->src_port = 1024 + slot;
    request->op = VIRTIO_VSOCK_OP_REQUEST;
    memset(workload->response[slot], 0, sizeof(*workload->response[slot]));
}

static int validate_vsock(struct perf_workload *workload,
                          unsigned slot,
                          const uint32_t *lengths)
{
    struct virtio_vsock_hdr *request = workload->vsock_request[slot];
    struct virtio_vsock_hdr *response = workload->response[slot];

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

static int cleanup_vsock(struct perf_workload *workload, unsigned slot)
{
    workload->vsock_request[slot]->op = VIRTIO_VSOCK_OP_RST;
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
    struct perf_run_stats stats;
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
    unsigned queue_depth = read_cmdline_value("vv.perf_queue_depth", 1);
    unsigned batch_size = read_cmdline_value("vv.perf_batch_size", 1);
    read_cmdline_string("vv.perf_device", device, sizeof(device), "blk");
    read_cmdline_string("vv.perf_experiment", experiment,
                        sizeof(experiment), "queue");
    read_cmdline_string("vv.perf_changed", changed, sizeof(changed), "none");

    if (select_workload(device, &workload) < 0) {
        printf("VVPERF error=unsupported_device\n");
        shutdown_guest(1);
    }
    if (queue_depth > MAX_QUEUE_DEPTH ||
        (queue_depth & (queue_depth - 1)) != 0) {
        printf("VVPERF error=queue_depth\n");
        shutdown_guest(1);
    }
    if (batch_size > queue_depth ||
        (batch_size & (batch_size - 1)) != 0) {
        printf("VVPERF error=batch_size\n");
        shutdown_guest(1);
    }
    workload.queue_depth = queue_depth;

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

    unsigned descriptors_per_slot = 1;
    if (workload.device_id == VIRTIO_PCI_DEVICE_BLK)
        descriptors_per_slot = 3;
    else if (workload.device_id == VIRTIO_PCI_DEVICE_NET)
        descriptors_per_slot = 2;
    if (vr->size / descriptors_per_slot < workload.queue_depth ||
        (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK &&
         queues[0].size < workload.queue_depth)) {
        printf("VVPERF error=queue_depth_unsupported\n");
        shutdown_guest(1);
    }
    int descriptor_count = workload.ops->prepare(&dev, vr, &workload);
    if (descriptor_count < 0 || vr->size < descriptor_count) {
        printf("VVPERF error=request_setup\n");
        shutdown_guest(1);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK) {
        for (unsigned slot = 0; slot < workload.queue_depth; slot++) {
            workload.response[slot] = vv_alloc_pages(1);
            vring_raw_set_desc(&queues[0], slot,
                               vv_virt_to_phys(workload.response[slot]),
                               sizeof(*workload.response[slot]),
                               VRING_DESC_F_WRITE, 0);
            perf_slot_init(&workload.requests[0].slots[slot], slot, slot,
                           workload.response[slot]);
        }
        workload.requests[0].vr = &queues[0];
    }

    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    perf_stats_init(&stats);
    enum perf_request_result result = run_requests(&dev, &workload, warmup,
                                                   batch_size, &stats);
    if (result != PERF_REQUEST_OK) {
        printf("VVPERF error=%s phase=warmup\n", request_error(result));
        shutdown_guest(1);
    }

    for (unsigned round = 0; round < rounds; round++) {
        perf_stats_init(&stats);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        result = run_requests(&dev, &workload, iterations, batch_size,
                              &stats);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        if (result != PERF_REQUEST_OK) {
            printf("VVPERF error=%s phase=measured\n",
                   request_error(result));
            shutdown_guest(1);
        }

        uint64_t duration_ns = elapsed_ns(&start, &end);

        printf("VVPERF version=3 experiment=%s changed=%s "
               "workload=%s operation=%s round=%u request_bytes=%u "
               "iterations=%u duration_ns=%llu queue_format=split "
               "queue_depth=%u batch_size=%u submissions=%llu "
               "completions=%llu notifications=%llu timing_mode=throughput "
               "clock_source=CLOCK_MONOTONIC_RAW features=0x0\n",
               experiment, changed, workload.device, workload.operation,
               round + 1, workload.request_size, iterations,
               (unsigned long long)duration_ns, workload.queue_depth,
               batch_size, (unsigned long long)stats.submissions,
               (unsigned long long)stats.completions,
               (unsigned long long)stats.notifications);
    }
    shutdown_guest(0);
    return 0;
}