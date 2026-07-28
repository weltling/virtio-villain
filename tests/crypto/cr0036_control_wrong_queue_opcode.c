/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0036: crypto_control_wrong_queue_opcode
 *
 * Fault injection. Spec 5.9.7 lists the opcodes the control queue
 * accepts. Submitting a data queue opcode such as encrypt on the control
 * queue must be rejected rather than mis parsed as a session request.
 * Post a control request carrying the encrypt opcode and verify the host
 * survives. The device outcome is up to it: PASS, REJECT and WEDGED are
 * all acceptable as long as the next test can still run. Most valuable
 * under an ASan VMM. Skips on Cloud Hypervisor and when the cipher
 * service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_wrong_queue_opcode(struct virtio_dev *dev,
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
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    /* A data queue opcode submitted on the control queue. */
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a data opcode on the control queue");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0036, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_wrong_queue_opcode,
              "A data opcode on the control queue keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.7");
