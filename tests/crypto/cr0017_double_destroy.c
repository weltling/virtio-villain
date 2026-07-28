/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0017: crypto_double_destroy
 *
 * Spec 5.9.7: a session id is valid only until it is destroyed, so a
 * second destroy of the same id must be rejected with a non OK status
 * and the device must stay healthy. Create an AES-CBC session, destroy
 * it once and expect OK, then destroy the same id again and verify the
 * device responds with a non OK status. Skips on Cloud Hypervisor, when
 * the cipher service or AES-CBC is not advertised, and when the backend
 * cannot create the session; passes under a QEMU with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t destroy_session(struct virtio_dev *dev, struct vring *cvr,
                                     uint16_t slot, uint64_t session_id,
                                     uint8_t *status_out)
{
    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
    req->u.destroy.session_id = session_id;
    inhdr->status = 0xFF;

    uint16_t b = (uint16_t)(slot * 2);
    vring_raw_set_desc(cvr, b, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, (uint16_t)(b + 1));
    vring_raw_set_desc(cvr, (uint16_t)(b + 1), vv_virt_to_phys(inhdr),
                       sizeof(*inhdr), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(cvr, slot, b);
    vring_raw_set_avail_idx(cvr, (uint16_t)(slot + 1));

    test_result_t r = vv_kick_and_wait(dev, cvr, 0, VV_TIMEOUT_MS);
    *status_out = inhdr->status;
    return r;
}

static test_result_t test_crypto_double_destroy(struct virtio_dev *dev,
                                                struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;
    if (!(cfg->cipher_algo_l & (1u << VIRTIO_CRYPTO_CIPHER_AES_CBC)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    struct virtio_crypto_op_ctrl_req *creq = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    creq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    creq->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    creq->u.sym_create.para.keylen = 16;
    creq->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    creq->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("create session status %u, expected OK", sin->status);

    uint64_t session_id = sin->session_id;

    /* First destroy uses control slots 2 and 3 (create used 0..2). */
    uint8_t status = 0xFF;
    r = destroy_session(dev, &cvr, 1, session_id, &status);
    if (r != TEST_PASS)
        return r;
    if (status != VIRTIO_CRYPTO_OK)
        TFAIL("first destroy status %u, expected OK", status);

    /* Second destroy of the same id must be rejected. */
    status = 0xFF;
    r = destroy_session(dev, &cvr, 2, session_id, &status);
    if (r != TEST_PASS)
        return r == TEST_REJECT ? TEST_PASS : r;
    if (status == VIRTIO_CRYPTO_OK)
        TFAIL("device acked OK on a second destroy of the same session");

    return TEST_PASS;
}

REGISTER_TEST(CR0017, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_double_destroy,
              "A second destroy of the same session is rejected",
              VIRTIO_SPEC_V1_2, "5.9.7");
