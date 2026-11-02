/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_PLAN9_UTIL_H
#define VV_PLAN9_UTIL_H

#include "lib/virtio_test.h"
#include "lib/virtio_9p.h"

#define PLAN9_TEST_BUFFER_SIZE 8192
#define PLAN9_RESPONSE_SIZE 256

struct plan9_test_ctx {
    struct virtio_dev *dev;
    struct vring *vr;
    uint8_t *buffer;
    uint32_t next_offset;
    uint16_t next_desc;
    uint16_t next_avail;
    uint16_t next_tag;
};

struct plan9_chain_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
};

static inline test_result_t
plan9_submit_chain(struct plan9_test_ctx *ctx,
                   const struct plan9_chain_desc *descs,
                   uint16_t desc_count,
                   uint32_t writable_capacity,
                   uint32_t *used_len);

static inline void plan9_test_init(struct plan9_test_ctx *ctx,
                                   struct virtio_dev *dev,
                                   struct vring *vr)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->dev = dev;
    ctx->vr = vr;
    ctx->buffer = vv_alloc_pages(2);
    ctx->next_tag = 1;
    memset(ctx->buffer, 0, PLAN9_TEST_BUFFER_SIZE);
}

static inline void *plan9_alloc(struct plan9_test_ctx *ctx, uint32_t size)
{
    uint32_t offset = (ctx->next_offset + 7u) & ~7u;
    if (offset > PLAN9_TEST_BUFFER_SIZE ||
        size > PLAN9_TEST_BUFFER_SIZE - offset)
        return NULL;

    void *ptr = ctx->buffer + offset;
    memset(ptr, 0, size);
    ctx->next_offset = offset + size;
    return ptr;
}

static inline test_result_t plan9_transact(struct plan9_test_ctx *ctx,
                                           void *request,
                                           uint32_t request_len,
                                           uint16_t request_flags,
                                           void *response,
                                           uint32_t response_len,
                                           uint16_t response_flags,
                                           uint32_t *used_len)
{
    struct plan9_chain_desc descs[] = {
        {
            .addr = vv_virt_to_phys(request),
            .len = request_len,
            .flags = request_flags,
        },
        {
            .addr = vv_virt_to_phys(response),
            .len = response_len,
            .flags = response_flags,
        },
    };
    uint32_t writable_capacity =
        (response_flags & VRING_DESC_F_WRITE) ? response_len : 0;

    return plan9_submit_chain(ctx, descs, 2, writable_capacity, used_len);
}

static inline test_result_t
plan9_submit_chain(struct plan9_test_ctx *ctx,
                   const struct plan9_chain_desc *descs,
                   uint16_t desc_count,
                   uint32_t writable_capacity,
                   uint32_t *used_len)
{
    struct vring *vr = ctx->vr;
    uint16_t head = ctx->next_desc;
    uint16_t used_before = vr->used->idx;

    if (desc_count == 0)
        TFAIL("cannot submit an empty descriptor chain");
    if (head >= vr->size || desc_count > vr->size - head)
        TFAIL("test exhausted descriptor table");

    for (uint16_t i = 0; i < desc_count; i++) {
        uint16_t flags = descs[i].flags;
        uint16_t next = 0;
        if (i + 1 < desc_count) {
            flags |= VRING_DESC_F_NEXT;
            next = head + i + 1;
        }
        vring_raw_set_desc(vr, head + i, descs[i].addr, descs[i].len,
                           flags, next);
    }

    vring_raw_set_avail(vr, ctx->next_avail % vr->size, head);
    ctx->next_desc += desc_count;
    ctx->next_avail++;
    vring_raw_set_avail_idx(vr, ctx->next_avail);

    test_result_t result =
        vv_kick_and_wait(ctx->dev, vr, 0, VV_TIMEOUT_MS);
    if (result != TEST_PASS)
        return result;

    __sync_synchronize();
    struct vring_used_elem *used =
        &vr->used->ring[used_before % vr->size];
    if (used->id != head)
        TFAIL("used id=%u, expected request head %u", used->id, head);
    if (used->len > writable_capacity)
        TFAIL("used len=%u exceeds writable capacity %u",
              used->len, writable_capacity);

    if (used_len)
        *used_len = used->len;
    return TEST_PASS;
}

static inline int plan9_response_matches(const void *response,
                                         uint32_t used_len,
                                         uint8_t type,
                                         uint16_t tag)
{
    if (used_len < sizeof(struct p9_header))
        return 0;

    const struct p9_header *header = response;
    return header->size >= sizeof(*header) &&
           header->size <= used_len &&
           header->type == type &&
           header->tag == tag;
}

static inline struct p9_version *
plan9_build_version(struct plan9_test_ctx *ctx)
{
    struct p9_version *request = plan9_alloc(ctx, sizeof(*request));
    if (!request)
        return NULL;

    request->header.size = sizeof(*request);
    request->header.type = P9_TVERSION;
    request->header.tag = P9_NOTAG;
    request->msize = PLAN9_TEST_BUFFER_SIZE;
    request->version_len = P9_VERSION_LEN;
    memcpy(request->version, P9_VERSION, P9_VERSION_LEN);
    return request;
}

static inline test_result_t
plan9_validate_version_response(const struct p9_version *response,
                                uint32_t used_len)
{
    if (!plan9_response_matches(response, used_len,
                                P9_RVERSION, P9_NOTAG))
        TFAIL("invalid Rversion header (used len=%u)", used_len);
    if (used_len < sizeof(*response))
        TFAIL("Rversion is only %u bytes", used_len);
    if (response->msize == 0 ||
        response->msize > PLAN9_TEST_BUFFER_SIZE)
        TFAIL("negotiated msize=%u", response->msize);
    if (response->version_len != P9_VERSION_LEN ||
        memcmp(response->version, P9_VERSION, P9_VERSION_LEN) != 0)
        TFAIL("device did not negotiate " P9_VERSION);

    return TEST_PASS;
}

static inline test_result_t plan9_version(struct plan9_test_ctx *ctx)
{
    struct p9_version *request = plan9_build_version(ctx);
    struct p9_version *response =
        plan9_alloc(ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    test_result_t result =
        plan9_transact(ctx, request, sizeof(*request), 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       &used_len);
    if (result != TEST_PASS)
        return result;

    return plan9_validate_version_response(response, used_len);
}

static inline uint8_t *plan9_put_u16(uint8_t *p, uint16_t value)
{
    memcpy(p, &value, sizeof(value));
    return p + sizeof(value);
}

static inline uint8_t *plan9_put_u32(uint8_t *p, uint32_t value)
{
    memcpy(p, &value, sizeof(value));
    return p + sizeof(value);
}

static inline uint8_t *plan9_put_u64(uint8_t *p, uint64_t value)
{
    memcpy(p, &value, sizeof(value));
    return p + sizeof(value);
}

static inline uint8_t *plan9_put_string(uint8_t *p, const char *value)
{
    uint16_t len = strlen(value);
    p = plan9_put_u16(p, len);
    memcpy(p, value, len);
    return p + len;
}

static inline uint8_t *
plan9_build_attach(struct plan9_test_ctx *ctx,
                   uint32_t fid,
                   uint16_t *tag_out,
                   uint32_t *len_out)
{
    uint8_t *request = plan9_alloc(ctx, 64);
    if (!request)
        return NULL;

    uint16_t tag = ctx->next_tag++;
    struct p9_header *header = (struct p9_header *)request;
    header->type = P9_TATTACH;
    header->tag = tag;

    uint8_t *p = request + sizeof(*header);
    p = plan9_put_u32(p, fid);
    p = plan9_put_u32(p, P9_NOFID);
    p = plan9_put_string(p, "root");
    p = plan9_put_string(p, "");
    p = plan9_put_u32(p, 0);
    header->size = p - request;
    *tag_out = tag;
    *len_out = header->size;
    return request;
}

static inline test_result_t plan9_attach_root(struct plan9_test_ctx *ctx,
                                              uint32_t fid,
                                              struct p9_qid *qid)
{
    uint16_t tag;
    uint32_t request_len;
    uint8_t *request =
        plan9_build_attach(ctx, fid, &tag, &request_len);
    uint8_t *response = plan9_alloc(ctx, PLAN9_RESPONSE_SIZE);
    if (!request || !response)
        TFAIL("test buffer exhausted");

    uint32_t used_len = 0;
    test_result_t result =
        plan9_transact(ctx, request, request_len, 0,
                       response, PLAN9_RESPONSE_SIZE, VRING_DESC_F_WRITE,
                       &used_len);
    if (result != TEST_PASS)
        return result;
    if (!plan9_response_matches(response, used_len, P9_RATTACH, tag))
        TFAIL("invalid Rattach header (used len=%u)", used_len);
    if (used_len < sizeof(struct p9_header) + sizeof(*qid))
        TFAIL("Rattach is only %u bytes", used_len);

    memcpy(qid, response + sizeof(struct p9_header), sizeof(*qid));
    return TEST_PASS;
}

static inline uint8_t *
plan9_build_walk(struct plan9_test_ctx *ctx,
                 uint32_t fid,
                 uint32_t newfid,
                 const char *const *names,
                 uint16_t name_count,
                 uint16_t *tag_out,
                 uint32_t *len_out)
{
    uint32_t size = sizeof(struct p9_header) + 10;
    for (uint16_t i = 0; i < name_count; i++)
        size += 2 + strlen(names[i]);

    uint8_t *request = plan9_alloc(ctx, size);
    if (!request)
        return NULL;

    uint16_t tag = ctx->next_tag++;
    struct p9_header *header = (struct p9_header *)request;
    header->size = size;
    header->type = P9_TWALK;
    header->tag = tag;

    uint8_t *p = request + sizeof(*header);
    p = plan9_put_u32(p, fid);
    p = plan9_put_u32(p, newfid);
    p = plan9_put_u16(p, name_count);
    for (uint16_t i = 0; i < name_count; i++)
        p = plan9_put_string(p, names[i]);

    if ((uint32_t)(p - request) != size)
        return NULL;
    *tag_out = tag;
    *len_out = size;
    return request;
}

static inline struct p9_getattr *
plan9_build_getattr(struct plan9_test_ctx *ctx,
                    uint32_t fid,
                    uint16_t *tag_out)
{
    struct p9_getattr *request = plan9_alloc(ctx, sizeof(*request));
    if (!request)
        return NULL;

    uint16_t tag = ctx->next_tag++;
    request->header.size = sizeof(*request);
    request->header.type = P9_TGETATTR;
    request->header.tag = tag;
    request->fid = fid;
    request->request_mask = 0x7ff;
    *tag_out = tag;
    return request;
}

#endif /* VV_PLAN9_UTIL_H */
