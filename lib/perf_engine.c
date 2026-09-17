/* SPDX-License-Identifier: Apache-2.0 */
#include "perf_engine.h"

void perf_slot_init(struct perf_request_slot *slot, uint16_t head,
                    uint16_t id, void *workload_memory)
{
    slot->head = head;
    slot->id = id;
    slot->state = PERF_SLOT_FREE;
    slot->workload_memory = workload_memory;
}

int perf_slot_prepare(struct perf_request_slot *slot)
{
    if (slot->state != PERF_SLOT_FREE &&
        slot->state != PERF_SLOT_COMPLETED)
        return -1;
    slot->state = PERF_SLOT_PREPARED;
    return 0;
}

int perf_slot_submit(struct perf_request_slot *slot)
{
    if (slot->state != PERF_SLOT_PREPARED)
        return -1;
    slot->state = PERF_SLOT_SUBMITTED;
    return 0;
}

int perf_slot_complete(struct perf_request_slot *slot, uint32_t id)
{
    if (slot->state != PERF_SLOT_SUBMITTED || id != slot->id)
        return -1;
    slot->state = PERF_SLOT_COMPLETED;
    return 0;
}

int perf_slots_complete(struct perf_request_slot *slots, unsigned count,
                        uint32_t id)
{
    for (unsigned i = 0; i < count; i++) {
        if (slots[i].id == id)
            return perf_slot_complete(&slots[i], id) < 0 ? -1 : (int)i;
    }
    return -1;
}

void perf_stats_init(struct perf_run_stats *stats)
{
    stats->submissions = 0;
    stats->completions = 0;
    stats->notifications = 0;
}

void perf_stats_submit(struct perf_run_stats *stats, unsigned count)
{
    stats->submissions += count;
    stats->notifications++;
}

void perf_stats_complete(struct perf_run_stats *stats)
{
    stats->completions++;
}