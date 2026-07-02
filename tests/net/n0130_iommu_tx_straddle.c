/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0130: net_iommu_tx_straddle
 *
 * Reproduce the virtio-net TX wedge that happens when the net device
 * sits behind the virtio-iommu and a single TX buffer spans two
 * adjacent IOMMU mappings.
 *
 * The TX worker translates each descriptor with one translate_gva call
 * over the whole descriptor length (net_util/src/queue_pair.rs). The
 * IommuMapping requires the entire [addr, addr+len) span to fall inside
 * a single mapping (virtio-devices/src/iommu.rs, documented in
 * device.rs). A guest may legitimately back a contiguous IOVA range
 * with several adjacent page granule mappings, so a TX buffer that
 * crosses a mapping boundary fails translation. The worker returns Err,
 * thread_helper turns that into mark_device_needs_reset, and every net
 * queue halts until the device is reset.
 *
 * Setup here mirrors the observed evidence: a 0x4d4 byte buffer at page
 * offset 0xb50 that crosses a 4 KiB boundary.
 *
 * This test only makes sense when the net device is attached to the
 * virtio-iommu. The runner enables that topology just for N0130.
 *
 * Verdict:
 *   PASS   - device handled the straddling buffer without wedging
 *            (translation split per granule, or a graceful drop).
 *   WEDGED - device set NEEDS_RESET, the bug is present.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* Domain used to hold the two adjacent mappings for the net endpoint. */
#define TX_DOMAIN      0x30
/* Page aligned IOVA base, known to sit inside the CH input range. */
#define TX_IOVA_BASE   0xA0000ULL
/* Match the observed report: offset 0xb50, length 0x4d4 (1236 bytes). */
#define TX_BUF_OFFSET  0xB50ULL
#define TX_BUF_LEN     0x4D4U

/*
 * Compute the virtio-iommu endpoint id for a PCI device the same way
 * Cloud Hypervisor does: the device BDF packed as
 * (segment << 16) | (bus << 8) | (device << 3) | function.
 * dev->slot is "/sys/bus/pci/devices/SSSS:BB:DD.F".
 */
static int endpoint_from_slot(const char *slot, uint32_t *out)
{
    const char *base = strrchr(slot, '/');
    base = base ? base + 1 : slot;
    unsigned seg, bus, dev, func;
    if (sscanf(base, "%x:%x:%x.%x", &seg, &bus, &dev, &func) != 4)
        return -1;
    *out = ((uint32_t)(seg & 0xffff) << 16)
         | ((uint32_t)(bus & 0xff) << 8)
         | ((uint32_t)(dev & 0x1f) << 3)
         | ((uint32_t)(func & 0x7));
    return 0;
}

/*
 * Bring up the virtio-iommu far enough to serve ATTACH and MAP
 * requests. Negotiate VIRTIO_F_VERSION_1 and enable both the request
 * queue (0) and the event queue (1); CH refuses to activate the device
 * unless both are ready.
 *
 * The global bypass is left at its default (on). An endpoint that is
 * not attached to any domain then translates as identity, which lets
 * net keep its identity ring addresses while it is brought up, before
 * the ATTACH request binds it to a domain. Once the net endpoint is
 * attached, the global bypass no longer applies to it and the domain
 * mappings govern its translation.
 */
#define VIRTIO_F_VERSION_1_BIT       32
#define VIRTIO_F_ACCESS_PLATFORM_BIT 33

static int iommu_bringup(struct virtio_dev *iommu, struct vring *reqvr,
                         struct vring *evtvr)
{
    if (virtio_pci_find(VIRTIO_PCI_DEVICE_IOMMU, iommu) < 0)
        return -1;

    volatile struct virtio_pci_common_cfg *cfg = iommu->common;
    cfg->device_status = 0;
    __sync_synchronize();
    for (int i = 0; i < 1000 && cfg->device_status != 0; i++)
        usleep(1000);
    if (cfg->device_status != 0)
        return -1;

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat_lo = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t feat_hi = cfg->device_feature;
    uint64_t offered = ((uint64_t)feat_hi << 32) | feat_lo;

    uint64_t want = 0;
    if (offered & (1ULL << VIRTIO_F_VERSION_1_BIT))
        want |= (1ULL << VIRTIO_F_VERSION_1_BIT);

    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = (uint32_t)want;
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = (uint32_t)(want >> 32);
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    int ok = 0;
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status & VIRTIO_STATUS_FEATURES_OK) {
            ok = 1;
            break;
        }
        usleep(1000);
    }
    if (!ok)
        return -1;

    vring_alloc(reqvr, 16);
    vring_attach(iommu, reqvr, 0);
    vring_alloc(evtvr, 16);
    vring_attach(iommu, evtvr, 1);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    return 0;
}

/*
 * Reset net and renegotiate with VIRTIO_F_ACCESS_PLATFORM (bit 33),
 * which also requires VIRTIO_F_VERSION_1 (bit 32). CH only routes a
 * virtio device's DMA through the vIOMMU when the driver acks bit 33;
 * the harness brings net up with zero features, so its buffers would
 * otherwise bypass the vIOMMU as identity and never be translated.
 *
 * Enable exactly the rx/tx pair (queue 0 and queue 1), which satisfies
 * net's activation (min two queues, no control queue for an even
 * count). The pair is enabled here, before the endpoint is attached to
 * a domain: at queue enable CH translates the ring base addresses, and
 * while net is still unattached the global bypass returns them as
 * identity, so the rings keep resolving to the physical pages the
 * harness allocated.
 *
 * Returns 0 on success, 1 if net does not offer access platform (not
 * behind a vIOMMU on this backing), or -1 on a bringup error.
 */
static int net_reactivate_access_platform(struct virtio_dev *dev,
                                          struct vring *rxvr,
                                          struct vring *txvr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_status = 0;
    __sync_synchronize();
    for (int i = 0; i < 1000 && cfg->device_status != 0; i++)
        usleep(1000);
    if (cfg->device_status != 0)
        return -1;

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat_lo = cfg->device_feature;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t feat_hi = cfg->device_feature;
    uint64_t offered = ((uint64_t)feat_hi << 32) | feat_lo;

    if (!(offered & (1ULL << VIRTIO_F_ACCESS_PLATFORM_BIT)))
        return 1; /* net not behind a vIOMMU on this backing */

    uint64_t want = (1ULL << VIRTIO_F_ACCESS_PLATFORM_BIT);
    if (offered & (1ULL << VIRTIO_F_VERSION_1_BIT))
        want |= (1ULL << VIRTIO_F_VERSION_1_BIT);

    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = (uint32_t)want;
    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = (uint32_t)(want >> 32);
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    int ok = 0;
    for (int i = 0; i < 1000; i++) {
        __sync_synchronize();
        if (cfg->device_status & VIRTIO_STATUS_FEATURES_OK) {
            ok = 1;
            break;
        }
        usleep(1000);
    }
    if (!ok)
        return -1;

    vring_alloc(rxvr, 16);
    vring_attach(dev, rxvr, 0);
    vring_attach(dev, txvr, 1);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    return 0;
}

/*
 * Outcome of submitting one readable TX descriptor and waiting.
 */
enum tx_outcome { TX_WEDGED, TX_RETURNED, TX_NOPROGRESS };

/*
 * Place one readable TX descriptor at slot, point avail_slot at it, kick
 * the queue, and wait. A wedge (NEEDS_RESET) is terminal, so it is
 * reported as soon as it appears. When the head returns, allow a brief
 * moment for a late NEEDS_RESET before declaring a clean return.
 */
static enum tx_outcome tx_submit_wait(struct virtio_dev *dev, struct vring *vr,
                                      uint16_t slot, uint64_t iova,
                                      uint32_t len, uint16_t avail_slot,
                                      uint16_t new_avail_idx)
{
    vring_raw_set_desc(vr, slot, iova, len, 0, 0);
    vring_raw_set_avail(vr, avail_slot, slot);
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    vring_raw_set_avail_idx(vr, new_avail_idx);
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    int step = 10000; /* 10 ms */
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(step);
        __sync_synchronize();
        if (dev->common->device_status & VIRTIO_STATUS_NEEDS_RESET)
            return TX_WEDGED;
        if ((uint16_t)(vr->used->idx - before) >= 1) {
            usleep(50000);
            __sync_synchronize();
            if (dev->common->device_status & VIRTIO_STATUS_NEEDS_RESET)
                return TX_WEDGED;
            return TX_RETURNED;
        }
        elapsed += step;
    }
    if (dev->common->device_status & VIRTIO_STATUS_NEEDS_RESET)
        return TX_WEDGED;
    return TX_NOPROGRESS;
}

static test_result_t test_net_iommu_tx_straddle(struct virtio_dev *dev,
                                                struct vring *vr)
{
    uint32_t endpoint;
    if (endpoint_from_slot(dev->slot, &endpoint) < 0)
        TFAIL("cannot parse net BDF from slot '%s'", dev->slot);

    struct virtio_dev iommu;
    struct vring ivr, ievt;
    if (iommu_bringup(&iommu, &ivr, &ievt) < 0)
        return TEST_SKIP; /* net not behind a vIOMMU on this backing */

    /*
     * Route net DMA through the vIOMMU by renegotiating with
     * VIRTIO_F_ACCESS_PLATFORM. The rx/tx pair is enabled here while net
     * is still unattached, so the ring addresses translate as identity
     * through the global bypass.
     */
    struct vring rxvr;
    int nr = net_reactivate_access_platform(dev, &rxvr, vr);
    if (nr > 0)
        return TEST_SKIP; /* net does not offer access platform */
    if (nr < 0)
        TFAIL("could not renegotiate net with VIRTIO_F_ACCESS_PLATFORM");

    /*
     * Two adjacent physical pages back a contiguous IOVA range through
     * two separate MAP requests, so CH stores them as two independent
     * mappings rather than one wide mapping.
     */
    struct virtio_iommu_req_attach *a = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m1 = vv_alloc_pages(1);
    struct virtio_iommu_req_map    *m2 = vv_alloc_pages(1);
    uint8_t *page_a = vv_alloc_pages(1);
    uint8_t *page_b = vv_alloc_pages(1);
    memset(a, 0, sizeof(*a));
    memset(m1, 0, sizeof(*m1));
    memset(m2, 0, sizeof(*m2));
    memset(page_a, 0xAA, 4096);
    memset(page_b, 0xBB, 4096);

    a->head.type = VIRTIO_IOMMU_T_ATTACH;
    a->domain = TX_DOMAIN;
    a->endpoint = endpoint;
    a->tail.status = 0xFF;

    m1->head.type = VIRTIO_IOMMU_T_MAP;
    m1->domain = TX_DOMAIN;
    m1->virt_start = TX_IOVA_BASE;
    m1->virt_end = TX_IOVA_BASE + 0xFFF;
    m1->phys_start = vv_virt_to_phys(page_a);
    m1->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m1->tail.status = 0xFF;

    m2->head.type = VIRTIO_IOMMU_T_MAP;
    m2->domain = TX_DOMAIN;
    m2->virt_start = TX_IOVA_BASE + 0x1000;
    m2->virt_end = TX_IOVA_BASE + 0x1FFF;
    m2->phys_start = vv_virt_to_phys(page_b);
    m2->flags = VIRTIO_IOMMU_MAP_F_READ | VIRTIO_IOMMU_MAP_F_WRITE;
    m2->tail.status = 0xFF;

    uint64_t a_phys = vv_virt_to_phys(a);
    uint64_t m1_phys = vv_virt_to_phys(m1);
    uint64_t m2_phys = vv_virt_to_phys(m2);
    size_t a_in = (size_t)((uint8_t *)&a->tail - (uint8_t *)a);
    size_t m_in = (size_t)((uint8_t *)&m1->tail - (uint8_t *)m1);

    vring_raw_set_desc(&ivr, 0, a_phys, a_in, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ivr, 1, a_phys + a_in, sizeof(a->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(&ivr, 2, m1_phys, m_in, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(&ivr, 3, m1_phys + m_in, sizeof(m1->tail),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(&ivr, 4, m2_phys, m_in, VRING_DESC_F_NEXT, 5);
    vring_raw_set_desc(&ivr, 5, m2_phys + m_in, sizeof(m2->tail),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&ivr, 0, 0);
    vring_raw_set_avail(&ivr, 1, 2);
    vring_raw_set_avail(&ivr, 2, 4);
    vring_raw_set_avail_idx(&ivr, 3);

    test_result_t ir = vv_kick_and_wait_n(&iommu, &ivr, 0, 3, VV_TIMEOUT_MS);
    if (ir != TEST_PASS)
        TFAIL("iommu attach/map did not complete (r=%d)", ir);
    if (a->tail.status != 0 || m1->tail.status != 0 || m2->tail.status != 0)
        TFAIL("iommu setup rejected: attach=%u map1=%u map2=%u",
              a->tail.status, m1->tail.status, m2->tail.status);

    /*
     * Straddling TX buffer, submitted first so a wedge is attributable
     * only to this buffer. The whole span is a valid, contiguous IOVA
     * range, but no single mapping covers it, so the pre patch worker
     * fails the one shot translate_gva and marks the device NEEDS_RESET.
     */
    uint64_t tx_iova = TX_IOVA_BASE + TX_BUF_OFFSET;
    enum tx_outcome so = tx_submit_wait(dev, vr, 0, tx_iova, TX_BUF_LEN, 0, 1);
    if (so == TX_WEDGED)
        TWEDGED("net set NEEDS_RESET on a TX buffer straddling two IOMMU "
                "mappings (iova=0x%llx len=0x%x)",
                (unsigned long long)tx_iova, TX_BUF_LEN);
    if (so == TX_NOPROGRESS)
        TREJECT("net made no progress on the straddling TX buffer");

    /*
     * The straddling buffer returned without a wedge. That is only the
     * correct outcome if the device actually translated it (the per
     * granule split). First prove that normal TX still works: a mapped,
     * non straddling buffer fully inside the first mapping must
     * translate and return. This is a positive control, independent of
     * how the device treats an unmapped address.
     */
    enum tx_outcome go = tx_submit_wait(dev, vr, 0, TX_IOVA_BASE + 0x100,
                                        0x100, 1, 2);
    if (go == TX_WEDGED)
        TWEDGED("net set NEEDS_RESET on a mapped, non straddling TX buffer "
                "fully inside one IOMMU mapping");
    if (go != TX_RETURNED)
        TREJECT("net made no progress on a mapped, non straddling TX buffer");

    /*
     * Now confirm the net endpoint is genuinely behind the vIOMMU by
     * submitting an unmapped IOVA: a translating device wedges, a device
     * in identity bypass does not.
     */
    enum tx_outcome po = tx_submit_wait(dev, vr, 0, 0xC0000ULL, 0x100, 2, 3);
    if (po == TX_WEDGED)
        return TEST_PASS; /* endpoint attached: the split handled the span */

    /*
     * The unmapped IOVA did not wedge, so the net endpoint is not behind
     * the vIOMMU and the straddle path was never exercised.
     */
    printf("N0130: net endpoint not behind vIOMMU (identity translation); "
           "topology not exercised\n");
    fflush(stdout);
    return TEST_SKIP;
}

REGISTER_TEST_Q(N0130, VIRTIO_PCI_DEVICE_NET, test_net_iommu_tx_straddle,
                "TX buffer straddling two IOMMU mappings wedges the device",
                VIRTIO_SPEC_V1_2, "5.1.6", 1);
