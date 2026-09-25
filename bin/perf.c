/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <stdbool.h>
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
#include "lib/vring_packed.h"

#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_ITERATIONS 10000
#define DEFAULT_ROUNDS 5
#define DEFAULT_WARMUP 1000
#define QUEUE_SIZE 128
#define MAX_QUEUE_DEPTH 32
#define MAX_QUEUES 16
#define MAX_WORKLOAD_QUEUES 2
#define MAX_DEVICE_QUEUES 16
#define MAX_CHAIN_DESCRIPTORS 3

enum perf_block_pattern {
    PERF_BLOCK_FIXED,
    PERF_BLOCK_SEQUENTIAL,
    PERF_BLOCK_RANDOM,
};

struct perf_latency {
    bool enabled;
    unsigned sample_every;
    unsigned start_request;
    unsigned complete_request;
    uint64_t tsc_hz;
    uint64_t start[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    uint32_t start_aux[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    bool selected[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    uint64_t *samples;
    unsigned sample_count;
    unsigned sample_capacity;
};

enum perf_request_result {
    PERF_REQUEST_OK,
    PERF_REQUEST_PREPARE,
    PERF_REQUEST_SUBMIT,
    PERF_REQUEST_COMPLETE,
    PERF_REQUEST_VALIDATE,
};

struct perf_queue_request {
    struct vring *vr;
    struct vring_packed *packed_vr;
    struct perf_request_slot slots[MAX_QUEUE_DEPTH];
    struct perf_descriptor_chain {
        struct vring_desc descriptors[MAX_CHAIN_DESCRIPTORS];
        struct vring_packed_desc *indirect;
        unsigned count;
        bool use_indirect;
    } chains[MAX_QUEUE_DEPTH];
};

struct perf_workload;

struct perf_workload_ops {
    int (*prepare)(struct virtio_dev *dev, struct vring *vr,
                   struct perf_workload *workload);
    void (*reset)(struct perf_workload *workload, unsigned queue,
                  unsigned slot, unsigned operation);
    int (*validate)(struct perf_workload *workload, unsigned queue,
                    unsigned slot,
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
    struct perf_queue_request requests[MAX_DEVICE_QUEUES];
    struct vring *vrings;
    unsigned request_count;
    unsigned queue_depth;
    unsigned device_queues;
    unsigned latency_start_request;
    unsigned latency_complete_request;
    bool event_idx;
    bool indirect;
    bool packed;
    bool net_receive;
    unsigned net_header_size;
    bool block_write;
    enum perf_block_pattern block_pattern;
    uint64_t block_request_count;
    struct virtio_blk_outhdr *block_header[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    uint8_t *block_data[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    uint8_t *status[MAX_DEVICE_QUEUES][MAX_QUEUE_DEPTH];
    struct virtio_vsock_hdr *vsock_request[MAX_QUEUE_DEPTH];
    struct virtio_vsock_hdr *response[MAX_QUEUE_DEPTH];
    struct virtio_net_hdr_mrg *net_header[MAX_QUEUE_DEPTH];
    uint8_t *net_frame[MAX_QUEUE_DEPTH];
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

#if defined(__x86_64__) || defined(__i386__)
static void perf_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                       uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(0));
}

static uint64_t perf_rdtscp(uint32_t *aux)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile("rdtscp" : "=a"(low), "=d"(high), "=c"(*aux)
                     : : "memory");
    __asm__ volatile("lfence" : : : "memory");
    return ((uint64_t)high << 32) | low;
}

static int perf_latency_init(struct perf_latency *latency,
                             unsigned sample_every, unsigned iterations,
                             unsigned start_request,
                             unsigned complete_request)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    struct timespec start_time;
    struct timespec end_time;
    uint32_t start_aux;
    uint32_t end_aux;

    perf_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001)
        return -1;
    perf_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1U << 27)))
        return -1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    uint64_t start_tsc = perf_rdtscp(&start_aux);
    usleep(20000);
    uint64_t end_tsc = perf_rdtscp(&end_aux);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
    uint64_t duration_ns = elapsed_ns(&start_time, &end_time);
    if (start_aux != end_aux || end_tsc <= start_tsc || duration_ns == 0)
        return -1;

    latency->enabled = true;
    latency->sample_every = sample_every;
    latency->start_request = start_request;
    latency->complete_request = complete_request;
    latency->tsc_hz = (uint64_t)(((__uint128_t)(end_tsc - start_tsc) *
                                  1000000000ULL) / duration_ns);
    latency->sample_capacity = 1 + (iterations - 1) / sample_every;
    size_t pages = (latency->sample_capacity * sizeof(*latency->samples) +
                    PAGE_SIZE - 1) / PAGE_SIZE;
    latency->samples = vv_alloc_pages(pages);
    return latency->tsc_hz ? 0 : -1;
}
#else
static int perf_latency_init(struct perf_latency *latency,
                             unsigned sample_every, unsigned iterations,
                             unsigned start_request,
                             unsigned complete_request)
{
    (void)latency;
    (void)sample_every;
    (void)iterations;
    (void)start_request;
    (void)complete_request;
    return -1;
}
#endif

static void perf_latency_start(struct perf_latency *latency,
                               unsigned request, unsigned operation,
                               unsigned queue, unsigned first, unsigned count)
{
#if defined(__x86_64__) || defined(__i386__)
    if (!latency->enabled || request != latency->start_request)
        return;
    bool selected = false;
    for (unsigned i = 0; i < count; i++) {
        unsigned slot = first + i;

        latency->selected[queue][slot] =
            (operation + i) % latency->sample_every == 0;
        selected |= latency->selected[queue][slot];
    }
    if (!selected)
        return;
    uint32_t aux;
    uint64_t tsc = perf_rdtscp(&aux);
    for (unsigned i = 0; i < count; i++) {
        unsigned slot = first + i;

        if (latency->selected[queue][slot]) {
            latency->start[queue][slot] = tsc;
            latency->start_aux[queue][slot] = aux;
        }
    }
#else
    (void)latency;
    (void)request;
    (void)operation;
    (void)queue;
    (void)first;
    (void)count;
#endif
}

static int perf_latency_complete(struct perf_latency *latency,
                                 unsigned request, unsigned queue,
                                 unsigned slot)
{
#if defined(__x86_64__) || defined(__i386__)
    if (!latency->enabled || request != latency->complete_request ||
        !latency->selected[queue][slot])
        return 0;
    uint32_t aux;
    uint64_t end = perf_rdtscp(&aux);
    if (aux != latency->start_aux[queue][slot] ||
        end <= latency->start[queue][slot] ||
        latency->sample_count >= latency->sample_capacity)
        return -1;
    uint64_t cycles = end - latency->start[queue][slot];
    latency->samples[latency->sample_count++] =
        (uint64_t)(((__uint128_t)cycles * 1000000000ULL) / latency->tsc_hz);
    latency->selected[queue][slot] = false;
#else
    (void)latency;
    (void)request;
    (void)queue;
    (void)slot;
#endif
    return 0;
}

static int submit_slots(struct virtio_dev *dev,
                        struct perf_queue_request *request,
                        unsigned first, unsigned count,
                        struct perf_run_stats *stats,
                        struct perf_latency *latency,
                        unsigned request_index, unsigned queue,
                        unsigned operation, bool event_idx)
{
    uint16_t heads[MAX_QUEUE_DEPTH];

    for (unsigned i = 0; i < count; i++) {
        struct perf_request_slot *slot = &request->slots[first + i];

        if (perf_slot_submit(slot) < 0)
            return -1;
        heads[i] = slot->head;
    }
    perf_latency_start(latency, request_index, operation, queue, first, count);
    if (request->packed_vr) {
        for (unsigned i = 0; i < count; i++) {
            unsigned slot_index = first + i;
            struct perf_descriptor_chain *chain =
                &request->chains[slot_index];
            struct vring_packed_desc descriptors[MAX_CHAIN_DESCRIPTORS];
            unsigned packed_count = chain->count;

            for (unsigned entry = 0; entry < chain->count; entry++) {
                descriptors[entry] = (struct vring_packed_desc){
                    .addr = chain->descriptors[entry].addr,
                    .len = chain->descriptors[entry].len,
                    .id = request->slots[slot_index].id,
                    .flags = chain->descriptors[entry].flags,
                };
            }
            if (chain->indirect == NULL && chain->use_indirect) {
                chain->indirect = vv_alloc_pages(1);
            }
            if (chain->indirect) {
                for (unsigned entry = 0; entry < chain->count; entry++) {
                    chain->indirect[entry] = descriptors[entry];
                    chain->indirect[entry].id =
                        request->slots[slot_index].id;
                }
                descriptors[0] = (struct vring_packed_desc){
                    .addr = vv_virt_to_phys(chain->indirect),
                    .len = chain->count * sizeof(*chain->indirect),
                    .id = request->slots[slot_index].id,
                    .flags = VRING_PACKED_DESC_F_INDIRECT,
                };
                packed_count = 1;
            }
            if (vring_packed_submit_chain(
                    request->packed_vr, descriptors, packed_count,
                    request->slots[slot_index].id,
                    &(uint16_t){0}, &(uint8_t){0}) < 0)
                return -1;
        }
        virtio_pci_kick(dev, request->packed_vr->queue);
        perf_stats_submit(stats, count, true);
        return 0;
    }
    uint16_t old_idx = request->vr->avail->idx;
    vring_submit_batch(request->vr, heads, count);
    bool notify = !event_idx ||
                  vring_need_event(vring_avail_event(request->vr),
                                   request->vr->avail->idx, old_idx);
    if (notify)
        virtio_pci_kick(dev, request->vr->queue);
    perf_stats_submit(stats, count, notify);
    return 0;
}

static int complete_next(struct perf_queue_request *request,
                         unsigned active_slots, uint16_t *used_idx,
                         uint32_t *lengths, struct perf_run_stats *stats,
                         struct perf_latency *latency,
                         unsigned request_index, unsigned queue)
{
    if (request->packed_vr) {
        for (;;) {
            uint16_t id;
            uint32_t len;

            if (vring_packed_next_used(request->packed_vr, &id, &len) < 0) {
                __sync_synchronize();
                continue;
            }
            int slot = perf_slots_complete(request->slots, active_slots, id);
            if (slot < 0)
                return -1;
            unsigned descriptor_count =
                request->chains[slot].use_indirect ? 1 :
                request->chains[slot].count;
            vring_packed_advance_used(request->packed_vr, descriptor_count);
            perf_stats_complete(stats);
            if (perf_latency_complete(latency, request_index, queue, slot) < 0)
                return -1;
            if (lengths)
                lengths[slot] = len;
            return slot;
        }
    }
    while (request->vr->used->idx == *used_idx)
        __sync_synchronize();
    struct vring_used_elem *used =
        &request->vr->used->ring[*used_idx % request->vr->size];
    int slot = perf_slots_complete(request->slots, active_slots, used->id);
    if (slot < 0)
        return -1;
    (*used_idx)++;
    perf_stats_complete(stats);
    if (perf_latency_complete(latency, request_index, queue, slot) < 0)
        return -1;
    if (lengths)
        lengths[slot] = used->len;
    return slot;
}

static enum perf_request_result run_requests(struct virtio_dev *dev,
                                             struct perf_workload *workload,
                                             unsigned count,
                                             unsigned batch_size,
                                             struct perf_run_stats *stats,
                                             struct perf_latency *latency)
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
            workload->ops->reset(workload, 0, slot, completed + slot);
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
                                 stats, latency, request, 0,
                                 completed + first,
                                 workload->event_idx) < 0)
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
                                         &used_idx[request], lengths, stats,
                                         latency, request, 0);
                if (slot < 0)
                    return PERF_REQUEST_COMPLETE;
                used_len[slot][request] = lengths[slot];
            }
        }
        int cleanup_request = -1;
        for (unsigned slot = 0; slot < active_slots; slot++) {
            if (workload->ops->validate(workload, 0, slot,
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
                                 stats, latency, cleanup_request, 0,
                                 completed + first,
                                 workload->event_idx) < 0)
                    return PERF_REQUEST_SUBMIT;
            }
            for (unsigned completion = 0; completion < active_slots;
                 completion++) {
                if (complete_next(queue_request, active_slots,
                                  &used_idx[cleanup_request], NULL,
                                  stats, latency, cleanup_request, 0) < 0)
                    return PERF_REQUEST_COMPLETE;
            }
        }
        completed += active_slots;
    }
    return PERF_REQUEST_OK;
}

static enum perf_request_result run_block_requests(
    struct virtio_dev *dev, struct perf_workload *workload, unsigned count,
    unsigned batch_size, struct perf_run_stats *stats,
    struct perf_latency *latency)
{
    uint16_t used_idx[MAX_DEVICE_QUEUES];

    for (unsigned queue = 0; queue < workload->device_queues; queue++)
        used_idx[queue] = workload->requests[queue].vr->used->idx;
    for (unsigned completed = 0; completed < count;) {
        unsigned wave = workload->queue_depth * workload->device_queues;

        if (wave > count - completed)
            wave = count - completed;
        for (unsigned queue = 0; queue < workload->device_queues; queue++) {
            struct perf_queue_request *request = &workload->requests[queue];
            unsigned offset = queue * workload->queue_depth;
            unsigned active_slots = 0;

            if (offset < wave) {
                active_slots = wave - offset;
                if (active_slots > workload->queue_depth)
                    active_slots = workload->queue_depth;
            }
            for (unsigned slot = 0; slot < active_slots; slot++) {
                workload->ops->reset(workload, queue, slot,
                                     completed + offset + slot);
                if (perf_slot_prepare(&request->slots[slot]) < 0)
                    return PERF_REQUEST_PREPARE;
            }
            for (unsigned first = 0; first < active_slots;
                 first += batch_size) {
                unsigned batch_count = batch_size;

                if (batch_count > active_slots - first)
                    batch_count = active_slots - first;
                if (submit_slots(dev, request, first, batch_count, stats,
                                 latency, 0, queue,
                                 completed + offset + first,
                                 workload->event_idx) < 0)
                    return PERF_REQUEST_SUBMIT;
            }
        }
        for (unsigned queue = 0; queue < workload->device_queues; queue++) {
            struct perf_queue_request *request = &workload->requests[queue];
            unsigned offset = queue * workload->queue_depth;
            unsigned active_slots = 0;

            if (offset < wave) {
                active_slots = wave - offset;
                if (active_slots > workload->queue_depth)
                    active_slots = workload->queue_depth;
            }
            for (unsigned completion = 0; completion < active_slots;
                 completion++) {
                int slot = complete_next(request, active_slots,
                                         &used_idx[queue], NULL, stats,
                                         latency, 0, queue);
                if (slot < 0)
                    return PERF_REQUEST_COMPLETE;
                if (workload->ops->validate(workload, queue, slot, NULL) < 0)
                    return PERF_REQUEST_VALIDATE;
            }
        }
        completed += wave;
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

static uint16_t prepare_chain(struct vring *vr, unsigned slot,
                              const struct vring_desc *descriptors,
                              unsigned count, bool indirect,
                              struct perf_queue_request *request)
{
    uint16_t head = indirect ? slot : slot * count;

    memcpy(request->chains[slot].descriptors, descriptors,
           count * sizeof(*descriptors));
    request->chains[slot].count = count;
        request->chains[slot].use_indirect = indirect;
    if (indirect) {
        struct vring_desc *table = vv_alloc_pages(1);

        memcpy(table, descriptors, count * sizeof(*table));
        vring_raw_set_desc(vr, head, vv_virt_to_phys(table),
                           count * sizeof(*table), VRING_DESC_F_INDIRECT, 0);
        return head;
    }
    for (unsigned index = 0; index < count; index++) {
        uint16_t next = descriptors[index].flags & VRING_DESC_F_NEXT ?
                        head + descriptors[index].next : 0;

        vring_raw_set_desc(vr, head + index, descriptors[index].addr,
                           descriptors[index].len, descriptors[index].flags,
                           next);
    }
    return head;
}

static int prepare_blk(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    (void)vr;
    if (!dev->device_cfg || dev->device_cfg_length < sizeof(uint64_t))
        return -1;
    uint64_t capacity = virtio_load64(dev->device_cfg);
    unsigned request_sectors = workload->request_size / 512;

    workload->block_request_count = capacity / request_sectors;
    if (workload->block_request_count == 0)
        return -1;
    for (unsigned queue = 0; queue < workload->device_queues; queue++) {
        struct perf_queue_request *request = &workload->requests[queue];

        request->vr = &workload->vrings[queue];
        for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
            struct virtio_blk_outhdr *header = vv_alloc_pages(1);
            uint8_t *data = vv_alloc_pages(1);
            uint8_t *status = vv_alloc_pages(1);
            struct vring_desc descriptors[3] = {
                {
                    .addr = vv_virt_to_phys(header),
                    .len = sizeof(*header),
                    .flags = VRING_DESC_F_NEXT,
                    .next = 1,
                },
                {
                    .addr = vv_virt_to_phys(data),
                    .len = workload->request_size,
                    .flags = VRING_DESC_F_NEXT |
                             (workload->block_write ? 0 : VRING_DESC_F_WRITE),
                    .next = 2,
                },
                {
                    .addr = vv_virt_to_phys(status),
                    .len = 1,
                    .flags = VRING_DESC_F_WRITE,
                },
            };

            header->type = workload->block_write ? VIRTIO_BLK_T_OUT :
                                                   VIRTIO_BLK_T_IN;
            if (workload->block_write)
                memset(data, 0x42, workload->request_size);
            uint16_t head = prepare_chain(request->vr, slot, descriptors, 3,
                                          workload->indirect, request);
            workload->block_header[queue][slot] = header;
            workload->block_data[queue][slot] = data;
            workload->status[queue][slot] = status;
            perf_slot_init(&request->slots[slot], head, head, header);
        }
    }
    workload->request_count = 1;
    return workload->queue_depth * (workload->indirect ? 1 : 3);
}

static void set_blk_write(struct perf_workload *workload, bool write)
{
    workload->block_write = write;
    for (unsigned queue = 0; queue < workload->device_queues; queue++) {
        struct perf_queue_request *request = &workload->requests[queue];

        for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
            struct vring_desc *data = &request->chains[slot].descriptors[1];

            workload->block_header[queue][slot]->type =
                write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
            data->flags = VRING_DESC_F_NEXT |
                          (write ? 0 : VRING_DESC_F_WRITE);
            if (write)
                memset(workload->block_data[queue][slot], 0x42,
                       workload->request_size);
            if (!workload->packed) {
                uint16_t head = slot * request->chains[slot].count;

                vring_raw_set_desc(request->vr, head + 1, data->addr,
                                   data->len, data->flags, head + 2);
            }
        }
    }
}

static void reset_blk(struct perf_workload *workload, unsigned queue,
                      unsigned slot, unsigned operation)
{
    uint64_t request = 0;

    if (workload->block_pattern == PERF_BLOCK_SEQUENTIAL) {
        request = operation % workload->block_request_count;
    } else if (workload->block_pattern == PERF_BLOCK_RANDOM) {
        uint64_t value = operation + 0x9e3779b97f4a7c15ULL;

        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        request = (value ^ (value >> 31)) % workload->block_request_count;
    }
    workload->block_header[queue][slot]->sector =
        request * (workload->request_size / 512);
    *workload->status[queue][slot] = 0xff;
}

static int validate_blk(struct perf_workload *workload,
                        unsigned queue, unsigned slot,
                        const uint32_t *lengths)
{
    (void)lengths;
    return *workload->status[queue][slot] == VIRTIO_BLK_S_OK ? 0 : -1;
}

static int prepare_rng(struct virtio_dev *dev, struct vring *vr,
                       struct perf_workload *workload)
{
    (void)dev;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        uint8_t *data = vv_alloc_pages(1);
        struct vring_desc descriptor = {
            .addr = vv_virt_to_phys(data),
            .len = DEFAULT_BLOCK_SIZE,
            .flags = VRING_DESC_F_WRITE,
        };

        uint16_t head = prepare_chain(vr, slot, &descriptor, 1,
                                      workload->indirect,
                                      &workload->requests[0]);
        perf_slot_init(&workload->requests[0].slots[slot], head, head, data);
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth;
}

static void reset_nop(struct perf_workload *workload, unsigned queue,
                      unsigned slot, unsigned operation)
{
    (void)workload;
    (void)queue;
    (void)slot;
    (void)operation;
}

static int cleanup_nop(struct perf_workload *workload, unsigned slot)
{
    (void)workload;
    (void)slot;
    return -1;
}

static int validate_rng(struct perf_workload *workload,
                        unsigned queue, unsigned slot,
                        const uint32_t *lengths)
{
    (void)queue;
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
        struct vring_desc descriptors[2] = {
            {
                .addr = vv_virt_to_phys(header),
                .len = sizeof(*header),
                .flags = VRING_DESC_F_NEXT,
                .next = 1,
            },
            {
                .addr = vv_virt_to_phys(frame),
                .len = 64,
            },
        };

        memset(frame, 0xff, 6);
        memset(frame + 6, 0x02, 6);
        frame[12] = 0x08;
        frame[13] = 0x00;
        memset(frame + 14, 0x42, 50);
        uint16_t head = prepare_chain(vr, slot, descriptors, 2,
                                      workload->indirect,
                                      &workload->requests[0]);
        perf_slot_init(&workload->requests[0].slots[slot], head, head,
                       header);
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth * (workload->indirect ? 1 : 2);
}

static int validate_net(struct perf_workload *workload,
                        unsigned queue, unsigned slot,
                        const uint32_t *lengths)
{
    (void)workload;
    (void)queue;
    (void)slot;
    return lengths[0] == 0 ? 0 : -1;
}

static void fill_net_frame(uint8_t *frame)
{
    memset(frame, 0xff, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x88;
    frame[13] = 0xb5;
    memset(frame + 14, 0x42, 50);
}

static int prepare_net_rx(struct virtio_dev *dev, struct vring *vr,
                          struct perf_workload *workload)
{
    (void)dev;
    for (unsigned slot = 0; slot < workload->queue_depth; slot++) {
        struct virtio_net_hdr_mrg *header = vv_alloc_pages(1);
        uint8_t *frame = vv_alloc_pages(1);
        struct vring_desc descriptors[2] = {
            {
                .addr = vv_virt_to_phys(header),
                .len = workload->net_header_size,
                .flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT,
                .next = 1,
            },
            {
                .addr = vv_virt_to_phys(frame),
                .len = 64,
                .flags = VRING_DESC_F_WRITE,
            },
        };

        uint16_t head = prepare_chain(vr, slot, descriptors, 2,
                                      workload->indirect,
                                      &workload->requests[0]);
        perf_slot_init(&workload->requests[0].slots[slot], head, head,
                       header);
        workload->net_header[slot] = header;
        workload->net_frame[slot] = frame;
    }
    workload->requests[0].vr = vr;
    workload->request_count = 1;
    return workload->queue_depth * (workload->indirect ? 1 : 2);
}

static void reset_net_rx(struct perf_workload *workload, unsigned queue,
                         unsigned slot, unsigned operation)
{
    (void)queue;
    (void)operation;
    memset(workload->net_header[slot], 0, workload->net_header_size);
    memset(workload->net_frame[slot], 0, workload->request_size);
}

static int validate_net_rx(struct perf_workload *workload,
                           unsigned queue, unsigned slot,
                           const uint32_t *lengths)
{
    uint8_t expected[64];
    uint8_t *frame = workload->net_frame[slot];

    (void)queue;
    fill_net_frame(expected);
    if (lengths[0] != workload->net_header_size + workload->request_size)
        return -1;
    if (memcmp(frame, expected, workload->request_size) == 0)
        return 0;
    if (frame[12] != 0x08 || frame[13] != 0x00 || frame[14] != 0x45 ||
        frame[16] != 0x00 || frame[17] != 50 || frame[23] != 17 ||
        frame[38] != 0 || frame[39] != 30)
        return -1;
    for (unsigned index = 42; index < workload->request_size; index++)
        if (frame[index] != 0x42)
            return -1;
    return 0;
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
        struct vring_desc descriptor = {
            .addr = vv_virt_to_phys(header),
            .len = sizeof(*header),
        };
        uint16_t head = prepare_chain(vr, slot, &descriptor, 1,
                                      workload->indirect,
                                      &workload->requests[1]);
        perf_slot_init(&workload->requests[1].slots[slot], head, head,
                       header);
        workload->vsock_request[slot] = header;
    }
    workload->requests[1].vr = vr;
    workload->request_count = 2;
    return workload->queue_depth;
}

static void reset_vsock(struct perf_workload *workload, unsigned queue,
                        unsigned slot, unsigned operation)
{
    (void)queue;
    (void)operation;
    struct virtio_vsock_hdr *request = workload->vsock_request[slot];

    request->src_port += workload->queue_depth;
    if (request->src_port < 1024)
        request->src_port = 1024 + slot;
    request->op = VIRTIO_VSOCK_OP_REQUEST;
    memset(workload->response[slot], 0, sizeof(*workload->response[slot]));
}

static int validate_vsock(struct perf_workload *workload,
                          unsigned queue, unsigned slot,
                          const uint32_t *lengths)
{
    (void)queue;
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
static const struct perf_workload_ops net_rx_ops = {
    prepare_net_rx, reset_net_rx, validate_net_rx, cleanup_nop
};
static const struct perf_workload_ops vsock_ops = {
    prepare_vsock, reset_vsock, validate_vsock, cleanup_vsock
};

static int select_workload(const char *device, const char *block_operation,
                           const char *block_pattern, unsigned block_request_size,
                           const char *net_operation,
                           struct perf_workload *workload)
{
    if (strcmp(device, "blk") == 0) {
        bool write;
        enum perf_block_pattern pattern;

        if (strcmp(block_operation, "read") == 0)
            write = false;
        else if (strcmp(block_operation, "write") == 0)
            write = true;
        else
            return -1;
        if (strcmp(block_pattern, "fixed") == 0)
            pattern = PERF_BLOCK_FIXED;
        else if (strcmp(block_pattern, "sequential") == 0)
            pattern = PERF_BLOCK_SEQUENTIAL;
        else if (strcmp(block_pattern, "random") == 0)
            pattern = PERF_BLOCK_RANDOM;
        else
            return -1;
        *workload = (struct perf_workload){
            .device = device,
            .operation = write ? "blk_write" : "blk_read",
            .device_id = VIRTIO_PCI_DEVICE_BLK,
            .queue = 0,
            .request_size = block_request_size,
            .ops = &blk_ops,
            .block_write = write,
            .block_pattern = pattern,
        };
    } else if (strcmp(device, "rng") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "rng_fill",
            .device_id = VIRTIO_PCI_DEVICE_RNG,
            .queue = 0,
            .request_size = DEFAULT_BLOCK_SIZE,
            .ops = &rng_ops,
        };
    } else if (strcmp(device, "net") == 0) {
        bool receive;

        if (strcmp(net_operation, "transmit") == 0)
            receive = false;
        else if (strcmp(net_operation, "receive") == 0)
            receive = true;
        else
            return -1;
        *workload = (struct perf_workload){
            .device = device,
            .operation = receive ? "net_rx" : "net_tx",
            .device_id = VIRTIO_PCI_DEVICE_NET,
            .queue = receive ? 0 : 1,
            .request_size = 64,
            .ops = receive ? &net_rx_ops : &net_ops,
            .net_receive = receive,
        };
    } else if (strcmp(device, "vsock") == 0) {
        *workload = (struct perf_workload){
            .device = device,
            .operation = "vsock_roundtrip",
            .device_id = VIRTIO_PCI_DEVICE_VSOCK,
            .queue = 1,
            .request_size = sizeof(struct virtio_vsock_hdr),
            .ops = &vsock_ops,
            .latency_start_request = 1,
            .latency_complete_request = 0,
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
    struct vring_packed packed_queues[MAX_QUEUES];
    struct vring *vr;
    struct perf_workload workload;
    struct perf_run_stats stats;
    struct perf_latency latency = {0};
    struct timespec start;
    struct timespec end;
    char device[16];
    char experiment[16];
    char changed[32];
    char timing_mode[16];
    char queue_format[16];
    char descriptor_layout[16];
    char notification_policy[16];
    char block_operation[16];
    char block_pattern[16];
    char net_operation[16];

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
    unsigned block_prefill = read_cmdline_value("vv.perf_block_prefill", 0);
    unsigned queue_depth = read_cmdline_value("vv.perf_queue_depth", 1);
    unsigned batch_size = read_cmdline_value("vv.perf_batch_size", 1);
    unsigned device_queues = read_cmdline_value("vv.perf_device_queues", 1);
    unsigned sample_every = read_cmdline_value("vv.perf_sample_every", 10);
    unsigned block_request_size = read_cmdline_value(
        "vv.perf_block_request_size", DEFAULT_BLOCK_SIZE);
    unsigned net_header_size = read_cmdline_value("vv.perf_net_header_size",
                                                   sizeof(struct virtio_net_hdr));
    read_cmdline_string("vv.perf_device", device, sizeof(device), "blk");
    read_cmdline_string("vv.perf_experiment", experiment,
                        sizeof(experiment), "queue");
    read_cmdline_string("vv.perf_changed", changed, sizeof(changed), "none");
    read_cmdline_string("vv.perf_timing_mode", timing_mode,
                        sizeof(timing_mode), "throughput");
    read_cmdline_string("vv.perf_queue_format", queue_format,
                        sizeof(queue_format), "split");
    read_cmdline_string("vv.perf_descriptor_layout", descriptor_layout,
                        sizeof(descriptor_layout), "direct");
    read_cmdline_string("vv.perf_notification_policy", notification_policy,
                        sizeof(notification_policy), "always");
    read_cmdline_string("vv.perf_block_operation", block_operation,
                        sizeof(block_operation), "read");
    read_cmdline_string("vv.perf_block_pattern", block_pattern,
                        sizeof(block_pattern), "fixed");
    read_cmdline_string("vv.perf_net_operation", net_operation,
                        sizeof(net_operation), "transmit");

    if (select_workload(device, block_operation, block_pattern,
                        block_request_size, net_operation, &workload) < 0) {
        printf("VVPERF error=unsupported_device\n");
        shutdown_guest(1);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_BLK &&
        (block_request_size < 512 || block_request_size > PAGE_SIZE ||
         block_request_size % 512 != 0)) {
        printf("VVPERF error=block_request_size\n");
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
    if (device_queues > MAX_DEVICE_QUEUES ||
        (device_queues & (device_queues - 1)) != 0 ||
        (workload.device_id != VIRTIO_PCI_DEVICE_BLK && device_queues != 1)) {
        printf("VVPERF error=device_queues\n");
        shutdown_guest(1);
    }
    workload.queue_depth = queue_depth;
    workload.device_queues = device_queues;
    workload.net_header_size = net_header_size;
    workload.vrings = queues;
    if (workload.net_receive && net_header_size != sizeof(struct virtio_net_hdr) &&
        net_header_size != sizeof(struct virtio_net_hdr_mrg)) {
        printf("VVPERF error=net_header_size\n");
        shutdown_guest(1);
    }
    if (strcmp(descriptor_layout, "indirect") == 0)
        workload.indirect = true;
    else if (strcmp(descriptor_layout, "direct") != 0) {
        printf("VVPERF error=descriptor_layout\n");
        shutdown_guest(1);
    }
    if (strcmp(notification_policy, "event_idx") == 0)
        workload.event_idx = true;
    else if (strcmp(notification_policy, "always") != 0) {
        printf("VVPERF error=notification_policy\n");
        shutdown_guest(1);
    }
    if (strcmp(queue_format, "packed") == 0)
        workload.packed = true;
    else if (strcmp(queue_format, "split") != 0) {
        printf("VVPERF error=queue_format\n");
        shutdown_guest(1);
    }
    if (workload.packed && workload.event_idx) {
        printf("VVPERF error=packed_event_idx_unsupported\n");
        shutdown_guest(1);
    }
    if (strcmp(timing_mode, "latency") == 0) {
        if (perf_latency_init(&latency, sample_every, iterations,
                              workload.latency_start_request,
                              workload.latency_complete_request) < 0) {
            printf("VVPERF error=rdtscp_unavailable\n");
            shutdown_guest(1);
        }
    } else if (strcmp(timing_mode, "throughput") != 0) {
        printf("VVPERF error=timing_mode\n");
        shutdown_guest(1);
    }

    if (virtio_pci_find(workload.device_id, &dev) < 0) {
        printf("VVPERF error=device_init\n");
        shutdown_guest(1);
    }
    unsigned __int128 wanted_features = 0;
    if (device_queues > 1) {
        if (!virtio_pci_feature_offered(&dev, VIRTIO_BLK_F_MQ)) {
            printf("VVPERF error=device_queues_unsupported\n");
            shutdown_guest(1);
        }
        wanted_features = (unsigned __int128)1 << VIRTIO_BLK_F_MQ;
    }
    if (workload.indirect) {
        if (!virtio_pci_feature_offered(&dev, VIRTIO_F_INDIRECT_DESC)) {
            printf("VVPERF error=indirect_unsupported\n");
            shutdown_guest(1);
        }
        wanted_features |= (unsigned __int128)1 << VIRTIO_F_INDIRECT_DESC;
    }
    if (workload.event_idx) {
        if (!virtio_pci_feature_offered(&dev, VIRTIO_F_EVENT_IDX)) {
            printf("VVPERF error=event_idx_unsupported\n");
            shutdown_guest(1);
        }
        wanted_features |= (unsigned __int128)1 << VIRTIO_F_EVENT_IDX;
    }
    if (workload.packed) {
        if (!virtio_pci_feature_offered(&dev, VIRTIO_F_RING_PACKED)) {
            printf("VVPERF error=packed_unsupported\n");
            shutdown_guest(1);
        }
        wanted_features |= (unsigned __int128)1 << VIRTIO_F_RING_PACKED;
    }
    if (virtio_pci_init_features(&dev, wanted_features) < 0) {
        printf("VVPERF error=device_init\n");
        shutdown_guest(1);
    }

    uint16_t queue_count = dev.common->num_queues;
    if (queue_count <= workload.queue || queue_count > MAX_QUEUES ||
        (workload.device_id == VIRTIO_PCI_DEVICE_BLK &&
         queue_count < device_queues)) {
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
        queues[queue].queue = queue;
        if (workload.packed) {
            vring_packed_alloc(&packed_queues[queue], queue_size);
            vring_packed_attach(&dev, &packed_queues[queue], queue);
        } else {
            vring_attach(&dev, &queues[queue], queue);
        }
    }
    vr = &queues[workload.queue];

    unsigned descriptors_per_slot = 1;
    if (!workload.indirect && workload.device_id == VIRTIO_PCI_DEVICE_BLK)
        descriptors_per_slot = 3;
    else if (!workload.indirect && workload.device_id == VIRTIO_PCI_DEVICE_NET)
        descriptors_per_slot = 2;
    if (vr->size / descriptors_per_slot < workload.queue_depth ||
        (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK &&
         queues[0].size < workload.queue_depth)) {
        printf("VVPERF error=queue_depth_unsupported\n");
        shutdown_guest(1);
    }
    int descriptor_count = workload.ops->prepare(&dev, vr, &workload);
    bool request_size_invalid = descriptor_count < 0;
    for (unsigned queue = 0; queue < device_queues; queue++)
        request_size_invalid |= queues[queue].size < descriptor_count;
    if (request_size_invalid) {
        printf("VVPERF error=request_setup\n");
        shutdown_guest(1);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_VSOCK) {
        for (unsigned slot = 0; slot < workload.queue_depth; slot++) {
            workload.response[slot] = vv_alloc_pages(1);
            struct vring_desc descriptor = {
                .addr = vv_virt_to_phys(workload.response[slot]),
                .len = sizeof(*workload.response[slot]),
                .flags = VRING_DESC_F_WRITE,
            };
            uint16_t head = prepare_chain(&queues[0], slot, &descriptor, 1,
                                          workload.indirect,
                                          &workload.requests[0]);
            perf_slot_init(&workload.requests[0].slots[slot], head, head,
                           workload.response[slot]);
        }
        workload.requests[0].vr = &queues[0];
    }
    if (workload.packed) {
        unsigned packed_request_count =
            workload.device_id == VIRTIO_PCI_DEVICE_BLK ?
            workload.device_queues : workload.request_count;

        for (unsigned request = 0; request < packed_request_count;
             request++) {
            unsigned queue = workload.requests[request].vr->queue;

            workload.requests[request].packed_vr = &packed_queues[queue];
        }
    }

    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    perf_stats_init(&stats);
    enum perf_request_result result;
    if (block_prefill != 0) {
        if (workload.device_id != VIRTIO_PCI_DEVICE_BLK ||
            workload.block_write || workload.indirect) {
            printf("VVPERF error=block_prefill\n");
            shutdown_guest(1);
        }
        set_blk_write(&workload, true);
        result = run_block_requests(&dev, &workload, block_prefill,
                                    batch_size, &stats,
                                    &(struct perf_latency){0});
        set_blk_write(&workload, false);
        if (result != PERF_REQUEST_OK) {
            printf("VVPERF error=%s phase=prefill\n",
                   request_error(result));
            shutdown_guest(1);
        }
        perf_stats_init(&stats);
    }
    if (workload.device_id == VIRTIO_PCI_DEVICE_BLK)
        result = run_block_requests(&dev, &workload, warmup, batch_size,
                                    &stats, &(struct perf_latency){0});
    else
        result = run_requests(&dev, &workload, warmup, batch_size, &stats,
                              &(struct perf_latency){0});
    if (result != PERF_REQUEST_OK) {
        printf("VVPERF error=%s phase=warmup\n", request_error(result));
        shutdown_guest(1);
    }

    for (unsigned round = 0; round < rounds; round++) {
        perf_stats_init(&stats);
        latency.sample_count = 0;
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        if (workload.device_id == VIRTIO_PCI_DEVICE_BLK)
            result = run_block_requests(&dev, &workload, iterations,
                                        batch_size, &stats, &latency);
        else
            result = run_requests(&dev, &workload, iterations, batch_size,
                                  &stats, &latency);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);
        if (result != PERF_REQUEST_OK) {
            printf("VVPERF error=%s phase=measured\n",
                   request_error(result));
            shutdown_guest(1);
        }

        uint64_t duration_ns = elapsed_ns(&start, &end);

        for (unsigned sample = 0; sample < latency.sample_count; sample++)
            printf("VVPERF_LATENCY round=%u sample=%u latency_ns=%llu\n",
                   round + 1, sample + 1,
                   (unsigned long long)latency.samples[sample]);

        printf("VVPERF version=3 experiment=%s changed=%s "
               "workload=%s operation=%s address_pattern=%s round=%u "
               "request_bytes=%u "
               "iterations=%u duration_ns=%llu queue_format=%s "
               "descriptor_layout=%s notification_policy=%s "
               "queue_depth=%u batch_size=%u device_queues=%u "
               "submissions=%llu "
               "completions=%llu notifications=%llu timing_mode=%s "
               "clock_source=%s sample_every=%u features=0x%llx\n",
               experiment, changed, workload.device, workload.operation,
               workload.device_id == VIRTIO_PCI_DEVICE_BLK ?
                   block_pattern : "none",
               round + 1, workload.request_size, iterations,
               (unsigned long long)duration_ns, queue_format,
               descriptor_layout,
               notification_policy,
               workload.queue_depth,
               batch_size, device_queues,
               (unsigned long long)stats.submissions,
               (unsigned long long)stats.completions,
               (unsigned long long)stats.notifications, timing_mode,
               latency.enabled ? "RDTSCP" : "CLOCK_MONOTONIC_RAW",
               latency.enabled ? sample_every : 0,
               (unsigned long long)wanted_features);
    }
    shutdown_guest(0);
    return 0;
}