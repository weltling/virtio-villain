/* SPDX-License-Identifier: Apache-2.0 */
#ifndef FUZZ_INPUT_H
#define FUZZ_INPUT_H

#include <stdint.h>

#define FUZZ_BLOB_SIZE 4096
#define FUZZ_MAX_DESCS 128
#define FUZZ_MAX_QUEUE_SIZE 256

/*
 * On-disk descriptor (no address — guest assigns addresses at runtime).
 */
struct fuzz_desc {
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

/*
 * Fuzz input blob header. Followed by:
 *   struct fuzz_desc descs[num_descs];
 *   uint16_t avail_ring[avail_count];
 *   uint8_t payload[...];  // fills remainder of blob
 */
struct fuzz_input {
    uint16_t queue_size;
    uint16_t num_descs;
    uint16_t avail_idx;
    uint16_t avail_count;
} __attribute__((packed));

/*
 * Parse a fuzz blob. Returns pointers into the blob for each section.
 * Returns 0 on success, -1 if the blob is malformed.
 */
static inline int fuzz_parse(const uint8_t *blob, uint32_t blob_len,
                             const struct fuzz_input **hdr,
                             const struct fuzz_desc **descs,
                             const uint16_t **avail_entries,
                             const uint8_t **payload,
                             uint32_t *payload_len)
{
    if (blob_len < sizeof(struct fuzz_input))
        return -1;

    const struct fuzz_input *h = (const struct fuzz_input *)blob;

    /* Sanity clamp */
    uint16_t nd = h->num_descs;
    if (nd > FUZZ_MAX_DESCS)
        nd = FUZZ_MAX_DESCS;

    uint16_t ac = h->avail_count;
    if (ac > FUZZ_MAX_QUEUE_SIZE)
        ac = FUZZ_MAX_QUEUE_SIZE;

    uint32_t needed = sizeof(struct fuzz_input)
                    + nd * sizeof(struct fuzz_desc)
                    + ac * sizeof(uint16_t);
    if (needed > blob_len)
        return -1;

    *hdr = h;
    *descs = (const struct fuzz_desc *)(blob + sizeof(struct fuzz_input));
    *avail_entries = (const uint16_t *)(blob + sizeof(struct fuzz_input)
                                        + nd * sizeof(struct fuzz_desc));
    *payload = blob + needed;
    *payload_len = blob_len - needed;
    return 0;
}

#endif /* FUZZ_INPUT_H */
