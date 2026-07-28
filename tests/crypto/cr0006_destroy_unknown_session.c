/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0006: crypto_destroy_unknown_session
 *
 * Spec 5.9.7: destroying a session that was never created is invalid
 * input. The device must reject it gracefully with a non OK status and
 * stay healthy rather than crash or wedge. Send a cipher destroy for a
 * bogus session id and verify the device responds with a non OK status.
 * Skips on Cloud Hypervisor and when the cipher service is not offered;
 * passes under QEMU. This path needs no crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_destroy_unknown(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
    req->u.destroy.session_id = 0xdeadbeefcafef00dULL;
    inhdr->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    if (vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status == VIRTIO_CRYPTO_OK)
        TFAIL("device acked OK destroying an unknown session");

    return TEST_PASS;
}

REGISTER_TEST(CR0006, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_destroy_unknown,
              "Destroy of an unknown session is rejected gracefully",
              VIRTIO_SPEC_V1_2, "5.9.7");
