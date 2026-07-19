/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RTC0024: RTC packed virtqueue read.
 *
 * Spec 2.8/RTC.5: Submit a READ_TIME request using the packed ring
 * format. Basic positive path for packed vring support.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rtc_packed_read(struct virtio_dev *dev,
                                          struct vring_packed *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4096);

    struct rtc_req_read *req = (void *)buf;
    req->msg_type = VIRTIO_RTC_REQ_READ;
    req->clock_id = 0;

    struct rtc_resp_read *resp = (void *)(buf + 256);

    /* Packed: request readable, response writable, two descriptors */
    vring_packed_set_desc(vr, 0, vv_virt_to_phys(req),
                          (uint32_t)sizeof(*req), 0,
                          VRING_PACKED_DESC_F_NEXT);
    vring_packed_set_desc(vr, 1, vv_virt_to_phys(resp),
                          (uint32_t)sizeof(*resp), 1,
                          VRING_PACKED_DESC_F_WRITE);
    __sync_synchronize();

    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(RTC0024, VIRTIO_PCI_DEVICE_RTC, test_rtc_packed_read,
                     "RTC READ_TIME via packed virtqueue",
                     VIRTIO_SPEC_V1_4, "5.23.6");
