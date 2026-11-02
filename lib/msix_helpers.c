/* SPDX-License-Identifier: Apache-2.0 */
#include "msix_helpers.h"
#include "lib/pci.h"

#include <errno.h>
#include <time.h>

static int timeout_ms(void)
{
    return VV_TIMEOUT_MS * (vv_wait_scale > 0 ? vv_wait_scale : 1);
}

static test_result_t config_read(struct msix_test *ctx, unsigned offset,
                                 void *value, size_t size)
{
    ssize_t bytes = pread(ctx->fd, value, size, offset);
    if (bytes != (ssize_t)size)
        TFAIL("PCI configuration read at 0x%x returned %zd of %zu bytes: %s",
              offset, bytes, size, bytes < 0 ? strerror(errno) : "short read");
    return TEST_PASS;
}

static test_result_t config_write(struct msix_test *ctx, unsigned offset,
                                  const void *value, size_t size)
{
    ssize_t bytes = pwrite(ctx->fd, value, size, offset);
    if (bytes != (ssize_t)size)
        TFAIL("PCI configuration write at 0x%x returned %zd of %zu bytes: %s",
              offset, bytes, size, bytes < 0 ? strerror(errno) : "short write");
    return TEST_PASS;
}

static volatile uint8_t *map_region(struct virtio_dev *dev, uint32_t location)
{
    unsigned bar = location & 7u;
    if (bar > 5)
        return NULL;
    volatile uint8_t *base = pci_map_bar(dev->slot, bar);
    return base ? base + (location & ~7u) : NULL;
}

test_result_t msix_vector_mask(struct msix_test *ctx, unsigned vector, int masked)
{
    if (vector >= 2)
        TFAIL("invalid MSI-X table vector %u", vector);
    volatile uint8_t *control = ctx->table + vector * 16 + 12;
    uint32_t value = vv_mmio_read32(control);
    vv_mmio_write32(control, masked ? value | 1u : value & ~1u);
    __sync_synchronize();
    if ((vv_mmio_read32(control) & 1u) != !!masked)
        TFAIL("MSI-X vector %u mask readback is incorrect", vector);
    return TEST_PASS;
}

test_result_t msix_function_mask(struct msix_test *ctx, int masked)
{
    uint16_t control;
    MSIX_CHECK(config_read(ctx, ctx->capability + 2, &control, sizeof(control)));
    control = masked ? control | 0x4000u : control & ~0x4000u;
    MSIX_CHECK(config_write(ctx, ctx->capability + 2, &control, sizeof(control)));
    uint16_t actual;
    MSIX_CHECK(config_read(ctx, ctx->capability + 2, &actual, sizeof(actual)));
    if (!(actual & 0x8000u) || !!(actual & 0x4000u) != !!masked)
        TFAIL("MSI-X function mask readback 0x%04x, expected mask %d and Enable",
              actual, !!masked);
    return TEST_PASS;
}

test_result_t msix_queue_vector(struct msix_test *ctx, unsigned queue,
                                uint16_t vector)
{
    if (queue >= ctx->queues)
        TFAIL("invalid MSI-X test queue %u", queue);
    volatile struct virtio_pci_common_cfg *cfg = ctx->dev->common;
    cfg->queue_select = queue;
    __sync_synchronize();
    cfg->queue_msix_vector = vector;
    __sync_synchronize();
    uint16_t actual = cfg->queue_msix_vector;
    if (vector != VIRTIO_MSI_NO_VECTOR && actual == VIRTIO_MSI_NO_VECTOR)
        TSKIP("device could not allocate MSI-X vector %u for queue %u",
              vector, queue);
    if (actual != vector)
        TFAIL("queue %u vector 0x%04x, expected 0x%04x", queue, actual, vector);
    return TEST_PASS;
}

test_result_t msix_config_vector(struct msix_test *ctx, uint16_t vector)
{
    ctx->dev->common->msix_config = vector;
    __sync_synchronize();
    uint16_t actual = ctx->dev->common->msix_config;
    if (vector != VIRTIO_MSI_NO_VECTOR && actual == VIRTIO_MSI_NO_VECTOR)
        TSKIP("device could not allocate configuration MSI-X vector %u", vector);
    if (actual != vector)
        TFAIL("configuration vector 0x%04x, expected 0x%04x", actual, vector);
    return TEST_PASS;
}

static test_result_t setup(struct msix_test *ctx)
{
    struct virtio_dev *dev = ctx->dev;
    ctx->fd = pci_cfg_open(dev->slot);
    if (ctx->fd < 0)
        TFAIL("open PCI configuration: %s", strerror(errno));
    uint8_t config[256];
    MSIX_CHECK(config_read(ctx, 0, config, sizeof(config)));
    ctx->capability = pci_find_capability(ctx->fd, PCI_CAP_ID_MSIX);
    if (!ctx->capability)
        TSKIP("MSI-X capability unavailable");
    uint32_t control, table, pba;
    MSIX_CHECK(config_read(ctx, ctx->capability, &control, sizeof(control)));
    if (((control >> 16) & 0x7ffu) + 1 < 2)
        TSKIP("test requires two MSI-X vectors");
    if (ctx->queues > dev->common->num_queues)
        TSKIP("test requires %u block queues", ctx->queues);
    if (ctx->queues == 2 &&
        !virtio_pci_feature_offered(dev, VIRTIO_BLK_F_MQ))
        TSKIP("block multiqueue feature unavailable");
    MSIX_CHECK(config_read(ctx, ctx->capability + 4, &table, sizeof(table)));
    MSIX_CHECK(config_read(ctx, ctx->capability + 8, &pba, sizeof(pba)));
    ctx->table = map_region(dev, table);
    ctx->pba = map_region(dev, pba);
    if (!ctx->table || !ctx->pba)
        TFAIL("map MSI-X table or pending bit array: %s", strerror(errno));
    virtio_pci_reset(dev);
    int observed = vv_irq_open(&ctx->irq, dev->slot, 2);
    if (observed == VV_IRQ_UNAVAILABLE)
        TSKIP("IRQ infrastructure unavailable: %s", ctx->irq.error);
    if (observed < 0)
        TFAIL("IRQ observer setup: %s", ctx->irq.error);
    ctx->observing = 1;
    unsigned __int128 features = 1ULL << VIRTIO_F_VERSION_1;
    if (ctx->queues == 2)
        features |= 1ULL << VIRTIO_BLK_F_MQ;
    if (virtio_pci_init_features(dev, features) < 0)
        TFAIL("device reset or MSI-X test feature negotiation failed");
    if (ctx->queues == 2) {
        if (!dev->device_cfg || dev->device_cfg_length < 36)
            TFAIL("negotiated block multiqueue configuration is unavailable");
        uint16_t maximum = vv_mmio_read16(
            (volatile uint8_t *)dev->device_cfg + 34);
        if (maximum < ctx->queues)
            TSKIP("block configuration advertises fewer than two request queues");
    }
    uint16_t command;
    MSIX_CHECK(config_read(ctx, 4, &command, sizeof(command)));
    command |= 6u;
    MSIX_CHECK(config_write(ctx, 4, &command, sizeof(command)));
    MSIX_CHECK(msix_vector_mask(ctx, 0, 1));
    MSIX_CHECK(msix_vector_mask(ctx, 1, 1));
    MSIX_CHECK(msix_function_mask(ctx, 0));
    MSIX_CHECK(msix_config_vector(ctx, VIRTIO_MSI_NO_VECTOR));
    for (unsigned q = 0; q < ctx->queues; q++) {
        dev->common->queue_select = q;
        __sync_synchronize();
        uint16_t size = dev->common->queue_size;
        if (size < 3)
            TSKIP("queue %u cannot hold a block read chain", q);
        vring_alloc(&ctx->queue[q], size < 16 ? size : 16);
        vring_prepare(dev, &ctx->queue[q], q);
        ctx->queue[q].avail->flags = 0;
        ctx->header[q] = vv_alloc_pages(1);
        ctx->data[q] = vv_alloc_pages(1);
        ctx->status[q] = vv_alloc_pages(1);
        ctx->header[q]->type = VIRTIO_BLK_T_IN;
    }
    MSIX_CHECK(msix_pending(ctx, 0));
    uint64_t counts[2];
    MSIX_CHECK(msix_counts(ctx, counts));
    if (counts[0] || counts[1])
        TFAIL("MSI-X interrupt delivered before event mappings and I/O");
    return TEST_PASS;
}

test_result_t msix_activate(struct msix_test *ctx, uint16_t first,
                            uint16_t second)
{
    uint16_t vectors[] = { first, second };
    for (unsigned q = 0; q < ctx->queues; q++) {
        MSIX_CHECK(msix_queue_vector(ctx, q, vectors[q]));
        if (ctx->dev->common->queue_enable)
            TFAIL("queue %u was not disabled by reset", q);
        ctx->dev->common->queue_enable = 1;
        __sync_synchronize();
        if (ctx->dev->common->queue_enable != 1)
            TFAIL("queue %u could not be enabled", q);
    }
    ctx->dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    if (!(ctx->dev->common->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device did not retain DRIVER_OK");
    return TEST_PASS;
}

test_result_t msix_read(struct msix_test *ctx, unsigned queue)
{
    if (queue >= ctx->queues)
        TFAIL("invalid read queue %u", queue);
    struct vring *vr = &ctx->queue[queue];
    *ctx->status[queue] = 0xff;
    vv_desc(vr, 0, ctx->header[queue], sizeof(*ctx->header[queue]),
            VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, ctx->data[queue], 512,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, ctx->status[queue], 1, VRING_DESC_F_WRITE, 0);
    uint16_t previous = ctx->completed[queue];
    uint16_t expected = previous + 1;
    vring_submit(vr, 0);
    virtio_pci_kick(ctx->dev, queue);
    for (int i = 0; i < timeout_ms(); i++) {
        __sync_synchronize();
        if (vr->used->idx != previous) {
            if (vr->used->idx != expected ||
                vr->used->ring[previous % vr->size].id != 0)
                TFAIL("queue %u returned an unexpected completion", queue);
            if (*ctx->status[queue] != 0)
                TFAIL("queue %u block status 0x%02x", queue, *ctx->status[queue]);
            ctx->completed[queue] = expected;
            return TEST_PASS;
        }
        usleep(1000);
    }
    TFAIL("queue %u request did not complete", queue);
}

test_result_t msix_counts(struct msix_test *ctx, uint64_t counts[2])
{
    for (unsigned i = 0; i < 2; i++)
        if (vv_irq_read(&ctx->irq, i, &counts[i]) < 0)
            TFAIL("read interrupt count: %s", ctx->irq.error);
    return TEST_PASS;
}

static test_result_t observe(struct msix_test *ctx, const uint64_t before[2],
                              unsigned allowed, unsigned required)
{
    unsigned seen = 0;
    for (int i = 0; i < timeout_ms(); i++) {
        uint64_t count[2];
        MSIX_CHECK(msix_counts(ctx, count));
        seen = (count[0] > before[0] ? 1u : 0u) |
               (count[1] > before[1] ? 2u : 0u);
        if (seen & ~allowed)
            TFAIL("unexpected delivered MSI-X vectors 0x%x, allowed 0x%x",
                  seen, allowed);
        if (seen == 3 && required == 3)
            return TEST_PASS;
        usleep(1000);
    }
    if ((seen & required) != required)
        TFAIL("delivered MSI-X vectors 0x%x, expected 0x%x", seen, required);
    return TEST_PASS;
}

test_result_t msix_expect(struct msix_test *ctx, const uint64_t before[2],
                          unsigned delivered)
{
    return observe(ctx, before, delivered, delivered);
}

test_result_t msix_settle(struct msix_test *ctx, const uint64_t before[2],
                          unsigned allowed)
{
    return observe(ctx, before, allowed, 0);
}

test_result_t msix_pending(struct msix_test *ctx, unsigned expected)
{
    unsigned actual = 0;
    for (int i = 0; i < timeout_ms(); i++) {
        actual = vv_mmio_read32(ctx->pba) & 3u;
        if (actual == expected)
            return TEST_PASS;
        usleep(1000);
    }
    TFAIL("MSI-X pending bits 0x%x, expected 0x%x", actual, expected);
}

test_result_t msix_prepare_live(struct msix_test *ctx)
{
    MSIX_CHECK(msix_activate(ctx, 0, 0));
    MSIX_CHECK(msix_queue_vector(ctx, 0, 1));
    MSIX_CHECK(msix_vector_mask(ctx, 1, 0));
    uint64_t before[2];
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_expect(ctx, before, 2));
    MSIX_CHECK(msix_pending(ctx, 0));
    return msix_vector_mask(ctx, 1, 1);
}

test_result_t msix_finish_live(struct msix_test *ctx)
{
    ctx->dev->common->queue_select = 0;
    __sync_synchronize();
    if (ctx->dev->common->queue_msix_vector != 1)
        TFAIL("lifecycle transition did not preserve live queue vector 1");
    MSIX_CHECK(msix_pending(ctx, 2));
    uint64_t before[2];
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_vector_mask(ctx, 1, 0));
    MSIX_CHECK(msix_expect(ctx, before, 2));
    MSIX_CHECK(msix_pending(ctx, 0));
    MSIX_CHECK(msix_counts(ctx, before));
    MSIX_CHECK(msix_read(ctx, 0));
    MSIX_CHECK(msix_expect(ctx, before, 2));
    return msix_pending(ctx, 0);
}

test_result_t msix_wait_host(struct msix_test *ctx, const char *first,
                             const char *second, unsigned *matched)
{
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
        TFAIL("read host synchronization clock: %s", strerror(errno));
    uint64_t deadline = (uint64_t)start.tv_sec * 1000 + start.tv_nsec / 1000000
                        + 10u * timeout_ms();
    for (;;) {
        MSIX_CHECK(msix_read(ctx, 0));
        if (!memcmp(ctx->data[0], first, strlen(first))) {
            *matched = 0;
            return TEST_PASS;
        }
        if (second && !memcmp(ctx->data[0], second, strlen(second))) {
            *matched = 1;
            return TEST_PASS;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
            TFAIL("read host synchronization clock: %s", strerror(errno));
        if ((uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000 >= deadline)
            TFAIL("host did not acknowledge MSI-X lifecycle action");
        usleep(1000);
    }
}

test_result_t msix_run(struct virtio_dev *dev, unsigned queues,
                       test_result_t (*body)(struct msix_test *))
{
    struct msix_test ctx = { .dev = dev, .queues = queues, .fd = -1 };
    if (queues < 1 || queues > 2)
        TFAIL("invalid MSI-X test queue count %u", queues);
    test_result_t result = setup(&ctx);
    if (result == TEST_PASS)
        result = body(&ctx);
    virtio_pci_reset(dev);
    if (dev->common->device_status != 0) {
        printf("vv-fail device reset did not complete after MSI-X test\n");
        result = TEST_FAIL;
    }
    if (ctx.observing && vv_irq_close(&ctx.irq) < 0)
        result = TEST_FAIL;
    if (ctx.fd >= 0 && close(ctx.fd) < 0) {
        printf("vv-fail close MSI-X PCI configuration: %s\n", strerror(errno));
        result = TEST_FAIL;
    }
    return result;
}
