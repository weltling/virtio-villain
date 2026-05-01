/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0006: vsock_shutdown_then_send
 *
 * Send a SHUTDOWN followed by more data on the same connection.
 * The device must reject post-shutdown data rather than processing it.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} __attribute__((packed));

#define VIRTIO_VSOCK_TYPE_STREAM 1
#define VIRTIO_VSOCK_OP_SHUTDOWN 4
#define VIRTIO_VSOCK_OP_RW       5

static test_result_t test_vsock_shutdown_then_send(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_vsock_hdr *shutdown_pkt = vv_alloc_pages(1);
    struct virtio_vsock_hdr *data_pkt = vv_alloc_pages(1);
    uint8_t *payload = vv_alloc_pages(1);

    /* SHUTDOWN packet */
    shutdown_pkt->src_cid = 3;
    shutdown_pkt->dst_cid = 2;
    shutdown_pkt->src_port = 1234;
    shutdown_pkt->dst_port = 5678;
    shutdown_pkt->len = 0;
    shutdown_pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    shutdown_pkt->op = VIRTIO_VSOCK_OP_SHUTDOWN;
    shutdown_pkt->flags = 3; /* SHUTDOWN_SEND | SHUTDOWN_RECV */
    shutdown_pkt->buf_alloc = 4096;
    shutdown_pkt->fwd_cnt = 0;

    /* Data packet on same connection after shutdown */
    data_pkt->src_cid = 3;
    data_pkt->dst_cid = 2;
    data_pkt->src_port = 1234;
    data_pkt->dst_port = 5678;
    data_pkt->len = 64;
    data_pkt->type = VIRTIO_VSOCK_TYPE_STREAM;
    data_pkt->op = VIRTIO_VSOCK_OP_RW;
    data_pkt->flags = 0;
    data_pkt->buf_alloc = 4096;
    data_pkt->fwd_cnt = 0;

    memset(payload, 0xDD, 64);

    uint64_t shutdown_phys = vv_virt_to_phys(shutdown_pkt);
    uint64_t data_phys = vv_virt_to_phys(data_pkt);
    uint64_t payload_phys = vv_virt_to_phys(payload);

    /* Desc 0: shutdown packet (single desc) */
    vring_raw_set_desc(vr, 0, shutdown_phys, sizeof(*shutdown_pkt), 0, 0);
    /* Desc 1: data header + payload chain */
    vring_raw_set_desc(vr, 1, data_phys, sizeof(*data_pkt),
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, payload_phys, 64, 0, 0);

    /* Submit both in order */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0006, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_shutdown_then_send,
              "Data sent after SHUTDOWN on same connection",
              VIRTIO_SPEC_V1_2, "5.10.6");
