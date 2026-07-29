/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0053: crypto_short_session_input
 *
 * Fault injection. Spec 5.9.7 has the device write a session id and
 * status into a device-writable reply descriptor. A reply descriptor
 * shorter than that structure must not let the device write past its
 * end. Post a cipher create session whose reply descriptor is 4 bytes,
 * less than the session input structure, and verify the host survives.
 * The device outcome is up to it: PASS, REJECT and WEDGED are all
 * acceptable as long as the next test can still run. Most valuable under
 * an ASan VMM. Skips on Cloud Hypervisor and when the cipher service is
 * not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_short_session_input(struct virtio_dev *dev,
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
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.keylen = 16;
    req->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    /* Reply descriptor deliberately shorter than session_input. */
    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), 4,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a short session reply buffer");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0053, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_short_session_input,
              "A short session reply buffer keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.7");
