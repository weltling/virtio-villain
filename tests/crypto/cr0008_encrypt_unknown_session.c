/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0008: crypto_encrypt_unknown_session
 *
 * Spec 5.9.8: a data queue operation that names a session id the device
 * never issued must be rejected with a non OK status rather than
 * processed or crashed on. Submit a cipher encrypt on a data queue with
 * a bogus session id and verify the device responds with a non OK
 * status. Skips on Cloud Hypervisor and when the cipher service is not
 * offered; passes under QEMU. This path needs no crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_encrypt_unknown(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *req = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->header.session_id = 0xdeadbeefcafef00dULL;
    req->u.sym_cipher.para.iv_len = 16;
    req->u.sym_cipher.para.src_data_len = 16;
    req->u.sym_cipher.para.dst_data_len = 16;
    req->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(iv, 0, 16);
    memset(src, 0x41, 16);
    memset(dst, 0, 16);
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status == VIRTIO_CRYPTO_OK)
        TFAIL("device acked OK for an op on an unknown session");

    return TEST_PASS;
}

REGISTER_TEST(CR0008, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_encrypt_unknown,
              "Data op on an unknown session is rejected gracefully",
              VIRTIO_SPEC_V1_2, "5.9.8");
