/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0056: crypto_akcipher_sign_fuzz
 *
 * Fault injection. Spec 5.9.8.7 defines the akcipher sign opcode. A sign
 * request whose data descriptor is far shorter than the request header
 * must be rejected rather than read past its end. Post an RSA sign on
 * session zero with a 4 byte request descriptor and verify the host
 * survives. The device outcome is up to it: PASS, REJECT and WEDGED are
 * all acceptable as long as the next test can still run. Exercises the
 * akcipher sign dispatch arm. Most valuable under an ASan VMM with
 * akcipher enabled. Skips on Cloud Hypervisor, when the akcipher service
 * is not advertised, and when RSA is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_akcipher_sign_fuzz(struct virtio_dev *dev,
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
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_AKCIPHER_SIGN;
    dreq->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    dreq->header.session_id = 0;
    dreq->u.akcipher.para.src_data_len = 16;
    dreq->u.akcipher.para.dst_data_len = 16;
    memset(dst, 0, 16);
    inhdr->status = 0xFF;

    /* Request descriptor far shorter than the data request header. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a malformed akcipher sign");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0056, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_akcipher_sign_fuzz,
              "A malformed akcipher sign keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8.7");
