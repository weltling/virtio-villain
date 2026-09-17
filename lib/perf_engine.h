/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_PERF_ENGINE_H
#define VV_PERF_ENGINE_H

#include <stdint.h>

enum perf_slot_state {
    PERF_SLOT_FREE,
    PERF_SLOT_PREPARED,
    PERF_SLOT_SUBMITTED,
    PERF_SLOT_COMPLETED,
};

struct perf_request_slot {
    uint16_t head;
    uint16_t id;
    enum perf_slot_state state;
    void *workload_memory;
};

struct perf_run_stats {
    uint64_t submissions;
    uint64_t completions;
    uint64_t notifications;
};

void perf_slot_init(struct perf_request_slot *slot, uint16_t head,
                    uint16_t id, void *workload_memory);
int perf_slot_prepare(struct perf_request_slot *slot);
int perf_slot_submit(struct perf_request_slot *slot);
int perf_slot_complete(struct perf_request_slot *slot, uint32_t id);
int perf_slots_complete(struct perf_request_slot *slots, unsigned count,
                        uint32_t id);
void perf_stats_init(struct perf_run_stats *stats);
void perf_stats_submit(struct perf_run_stats *stats, unsigned count);
void perf_stats_complete(struct perf_run_stats *stats);

#endif /* VV_PERF_ENGINE_H */