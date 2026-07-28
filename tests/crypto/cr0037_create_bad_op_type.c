/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0037: crypto_create_bad_op_type
 *
 * Fault injection. Spec 5.9.7 carries the symmetric op type in the
 * create session request, selecting cipher, hash chaining or algorithm
 * chaining. A create naming an unknown op type must be rejected rather
 * than routed into an unhandled path. Post a cipher create session whose
 * op type is garbage and verify the host survives. The device outcome is
 * up to it: PASS, REJECT and WEDGED are all acceptable as long as the
 * next test can still run. Most valuable under an ASan VMM. Skips on
 * Cloud Hypervisor and when the cipher service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define BOGUS_SYM_OP_TYPE 0xdeadu

static test_result_t test_crypto_bad_op_type(struct virtio_dev *dev,
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
    req->u.sym_create.op_type = BOGUS_SYM_OP_TYPE;
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an unknown op type");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0037, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_bad_op_type,
              "An unknown create session op type keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.7");
