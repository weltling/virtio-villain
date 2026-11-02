/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_MSIX_HELPERS_H
#define VV_MSIX_HELPERS_H

#include "tests/test.h"
#include "lib/irq.h"

struct msix_test {
    struct virtio_dev *dev;
    struct vv_irq irq;
    struct vring queue[2];
    struct virtio_blk_outhdr *header[2];
    uint8_t *data[2];
    uint8_t *status[2];
    uint16_t completed[2];
    unsigned queues;
    int fd;
    int observing;
    uint8_t capability;
    volatile uint8_t *table;
    volatile uint8_t *pba;
};

test_result_t msix_run(struct virtio_dev *dev, unsigned queues,
                       test_result_t (*body)(struct msix_test *));
test_result_t msix_activate(struct msix_test *ctx, uint16_t first,
                            uint16_t second);
test_result_t msix_queue_vector(struct msix_test *ctx, unsigned queue,
                                uint16_t vector);
test_result_t msix_config_vector(struct msix_test *ctx, uint16_t vector);
test_result_t msix_vector_mask(struct msix_test *ctx, unsigned vector, int masked);
test_result_t msix_function_mask(struct msix_test *ctx, int masked);
test_result_t msix_read(struct msix_test *ctx, unsigned queue);
test_result_t msix_counts(struct msix_test *ctx, uint64_t counts[2]);
test_result_t msix_expect(struct msix_test *ctx, const uint64_t before[2],
                          unsigned delivered);
test_result_t msix_settle(struct msix_test *ctx, const uint64_t before[2],
                          unsigned allowed);
test_result_t msix_pending(struct msix_test *ctx, unsigned expected);
test_result_t msix_prepare_live(struct msix_test *ctx);
test_result_t msix_finish_live(struct msix_test *ctx);
test_result_t msix_wait_host(struct msix_test *ctx, const char *first,
                             const char *second, unsigned *matched);

#define MSIX_CHECK(expression) do { \
    test_result_t _result = (expression); \
    if (_result != TEST_PASS) \
        return _result; \
} while (0)

#endif
