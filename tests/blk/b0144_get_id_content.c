/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0144: GET_ID returns printable bytes within bound
 *
 * Spec 5.2.6.1 says VIRTIO_BLK_T_GET_ID writes up to 20 bytes of
 * device identifier into the writable buffer. The result is not
 * required to be NUL terminated but every byte that the device
 * actually writes should be a sane value rather than the leftover
 * sentinel the driver pre filled. This catches devices that ack
 * the request with status OK but never touch the buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_GET_ID 8
#define VIRTIO_BLK_ID_BYTES 20

static test_result_t test_blk_get_id_content(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    memset(data, 0xAA, VIRTIO_BLK_ID_BYTES);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), VIRTIO_BLK_ID_BYTES,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*st != 0)
        TREJECT("*st != 0");

    /* Check at least one byte was written and it is not the sentinel */
    int touched = 0;
    for (int i = 0; i < VIRTIO_BLK_ID_BYTES; i++) {
        if (data[i] != 0xAA) {
            touched = 1;
            break;
        }
    }
    if (!touched)
        TREJECT("!touched");

    return TEST_PASS;
}

REGISTER_TEST(B0144, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_content,
              "GET_ID writes identifier bytes into the buffer",
              VIRTIO_SPEC_V1_2, "5.2.6.1");
