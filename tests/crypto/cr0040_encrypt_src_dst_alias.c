/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0040: crypto_encrypt_src_dst_alias
 *
 * Fault injection. Spec 5.9.8 supplies separate source and destination
 * buffers for a cipher op. Pointing both at the same guest memory asks
 * the device to encrypt in place. It must either handle the aliasing or
 * reject the request rather than corrupt its own bookkeeping or crash
 * the host. Post an encrypt whose source and destination descriptors
 * address the same buffer and verify the host survives. The device
 * outcome is up to it: PASS, REJECT and WEDGED are all acceptable as
 * long as the next test can still run. Most valuable under an ASan VMM.
 * Skips on Cloud Hypervisor and when the cipher service is not
 * advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_src_dst_alias(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(iv, 0, 16);
    memset(buf, 0x41, 16);
    inhdr->status = 0xFF;

    /* Source and destination point at the same buffer. */
    uint64_t buf_phys = vv_virt_to_phys(buf);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, buf_phys, 16,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, buf_phys, 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an aliased source and destination");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0040, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_src_dst_alias,
              "An aliased source and destination keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8");
