/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0002: crypto_cipher_session_create_destroy
 *
 * Spec 5.9.7: on the control queue the driver creates a symmetric
 * cipher session by sending a create session request with the cipher
 * parameters and key, and the device replies with a session id and an
 * OK status. The driver then destroys the session by id. Create an
 * AES-CBC encrypt session, verify the device acks OK and returns a
 * session id, then destroy it and verify OK. Skips on Cloud
 * Hypervisor, which has no crypto model, when the device does not
 * offer the cipher service or advertise AES-CBC, and when the backend
 * cannot perform the algorithm (a QEMU built without a crypto
 * library). Passes under a QEMU with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_cipher_session(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;
    if (!(cfg->cipher_algo_l & (1u << VIRTIO_CRYPTO_CIPHER_AES_CBC)))
        return TEST_SKIP;

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);

    /* Create an AES-CBC encrypt session with a 16 byte key. */
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->header.queue_id = 0;
    req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.keylen = 16;
    req->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;  /* backend has no crypto library for this algo */
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("create session status %u, expected OK", sin->status);

    uint64_t session_id = sin->session_id;

    /* Destroy the session by id. */
    struct virtio_crypto_inhdr *inhdr =
        (struct virtio_crypto_inhdr *)key; /* reuse the page */
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
    req->u.destroy.session_id = session_id;
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("destroy session status %u, expected OK", inhdr->status);

    return TEST_PASS;
}

REGISTER_TEST_Q(CR0002, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_cipher_session,
              "Cipher session create then destroy acks OK",
              VIRTIO_SPEC_V1_2, "5.9.7", VV_QUEUE_LAST);
