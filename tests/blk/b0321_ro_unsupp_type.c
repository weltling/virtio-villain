/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0321: ro_unsupp_type
 *
 * VIRTIO_BLK_F_RO only changes the result of write requests to IOERR
 * (spec 5.2.6.2). A request type the device does not implement at all
 * (GET_LIFETIME without VIRTIO_BLK_F_LIFETIME, an unassigned type) is
 * still unsupported and should return VIRTIO_BLK_S_UNSUPP, so a driver
 * can tell "not implemented" from "media error". The sidecar launches
 * the VM with a read only disk.
 */
#include "tests/test.h"

/* A request type that the spec does not assign. */
#define UNASSIGNED_TYPE 0x7fu

/*
 * Submit header + one writable data descriptor + 1 byte status and
 * wait for completion. On TEST_PASS, *status holds the status byte
 * (0xFF if the device did not write it).
 */
static test_result_t blk_req(struct virtio_dev *dev, struct vring *vr,
                             uint32_t type, void *buf, uint32_t len,
                             uint8_t *status)
{
    static struct virtio_blk_outhdr *hdr;
    static uint8_t *st;
    if (!hdr) {
        hdr = vv_alloc_pages(1);
        st = vv_alloc_pages(1);
    }
    hdr->type = type;
    *st = 0xFF;
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, buf, len, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    uint16_t idx = vr->avail->idx;
    vring_raw_set_avail(vr, idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, idx + 1);
    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    __sync_synchronize();
    *status = *st;
    return TEST_PASS;
}

static test_result_t test_blk_ro_unsupp_type(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint8_t st;
    int bad = 0;

    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_RO))
        TSKIP("device not read only");

    if (!virtio_pci_feature_offered(dev, VIRTIO_BLK_F_LIFETIME)) {
        VV_TRY(blk_req(dev, vr, VIRTIO_BLK_T_GET_LIFETIME, buf,
                       sizeof(struct virtio_blk_lifetime), &st));
        if (st != VIRTIO_BLK_S_UNSUPP) {
            printf("vv-info RO GET_LIFETIME status %u (want UNSUPP)\n", st);
            bad++;
        }
    }

    VV_TRY(blk_req(dev, vr, UNASSIGNED_TYPE, buf, VIRTIO_BLK_SECTOR_SIZE, &st));
    if (st != VIRTIO_BLK_S_UNSUPP) {
        printf("vv-info RO type 0x%x status %u (want UNSUPP)\n",
               UNASSIGNED_TYPE, st);
        bad++;
    }

    if (bad)
        TFAIL("%d unsupported types not UNSUPP on RO device", bad);
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0321, VIRTIO_PCI_DEVICE_BLK, test_blk_ro_unsupp_type,
                       "Read-only device: unsupported types return UNSUPP",
                       VIRTIO_SPEC_V1_2, "5.2.6.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_RO), 0);
