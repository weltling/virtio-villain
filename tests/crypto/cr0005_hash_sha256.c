/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0005: crypto_hash_sha256
 *
 * Spec 5.9.9: with the hash service the driver creates a hash session
 * and submits data on a data queue to receive a digest. Create a
 * SHA-256 session, hash a 16 byte input, verify the device acks OK and
 * writes a non zero 32 byte digest, then destroy the session. Skips on
 * Cloud Hypervisor, when SHA-256 is not advertised, and when the
 * backend cannot perform it; passes under a QEMU with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_hash_sha256(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_HASH)))
        return TEST_SKIP;
    if (!(cfg->hash_algo & (1u << VIRTIO_CRYPTO_HASH_SHA_256)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    /* Create a SHA-256 hash session on the control queue. */
    struct virtio_crypto_op_ctrl_req *creq = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_HASH_CREATE_SESSION;
    creq->header.algo = VIRTIO_CRYPTO_HASH_SHA_256;
    creq->u.hash_create.para.algo = VIRTIO_CRYPTO_HASH_SHA_256;
    creq->u.hash_create.para.hash_result_len = 32;
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    if (vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("hash session create status %u, expected OK", sin->status);
    uint64_t session_id = sin->session_id;

    /* Hash 16 bytes on the data queue. */
    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *digest = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_HASH;
    dreq->header.algo = VIRTIO_CRYPTO_HASH_SHA_256;
    dreq->header.session_id = session_id;
    dreq->u.hash.para.src_data_len = 16;
    dreq->u.hash.para.hash_result_len = 32;
    memset(src, 0x61, 16);
    memset(digest, 0, 32);
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(digest), 32,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("hash op status %u, expected OK", inhdr->status);

    int nonzero = 0;
    for (int i = 0; i < 32; i++)
        if (digest[i] != 0)
            nonzero = 1;
    if (!nonzero)
        TFAIL("digest is all zero, no hash produced");

    /* Destroy the session. */
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_HASH_DESTROY_SESSION;
    creq->u.destroy.session_id = session_id;
    inhdr->status = 0xFF;
    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 1, 0);
    vring_raw_set_avail_idx(&cvr, 2);
    (void)vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);

    return TEST_PASS;
}

REGISTER_TEST(CR0005, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_hash_sha256,
              "SHA-256 hash op produces a non zero digest",
              VIRTIO_SPEC_V1_2, "5.9.9");
