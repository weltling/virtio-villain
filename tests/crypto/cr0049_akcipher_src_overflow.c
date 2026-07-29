/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0049: crypto_akcipher_src_overflow
 *
 * Fault injection. Spec 5.9.8.7 carries the source length in the
 * akcipher data request. A request whose source length is enormous while
 * the source descriptor is small must not drive the device to read past
 * the buffer or overflow its length arithmetic. Post an RSA encrypt on
 * the data queue with src_data_len 0xffffffff over a 16 byte source and
 * verify the host survives. The device outcome is up to it: PASS, REJECT
 * and WEDGED are all acceptable as long as the next test can still run.
 * Exercises the akcipher data dispatch. Most valuable under an ASan VMM
 * with akcipher enabled. Skips on Cloud Hypervisor, when the akcipher
 * service is not advertised, and when RSA is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_akcipher_src_overflow(struct virtio_dev *dev,
                                                       struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_AKCIPHER)))
        return TEST_SKIP;
    if (!(cfg->akcipher_algo & (1u << VIRTIO_CRYPTO_AKCIPHER_RSA)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_AKCIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    dreq->header.session_id = 0;
    dreq->u.akcipher.para.src_data_len = 0xffffffffu;
    dreq->u.akcipher.para.dst_data_len = 16;
    memset(src, 0x41, 16);
    memset(dst, 0, 16);
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an oversized akcipher source");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0049, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_akcipher_src_overflow,
              "An oversized akcipher source length keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8.7");
