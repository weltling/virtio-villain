/* SPDX-License-Identifier: Apache-2.0 */
/*
 * virtio_spec.h - Frozen, authoritative virtio device constants.
 *
 * This is the single source of truth for virtio device request types,
 * feature bits, status codes and the wire structs shared by tests.
 * Tests MUST include this header instead of redefining constants
 * locally; local redefinition is what caused opcode drift in the past.
 *
 * Values are anchored to the Linux UAPI headers
 * (include/uapi/linux/virtio_*.h), which track the ratified OASIS
 * virtio specification. Spec section numbers below refer to virtio
 * v1.2 unless a newer version is noted. Do not change a value to make
 * a single test pass: the whole point of this header is that the
 * numbers are frozen and shared.
 *
 * Families covered here: blk, net, vsock, mem, console, balloon, pmem.
 * Transport neutral feature bits (VIRTIO_F_*) live here too. PCI
 * specific constants (status bits, PCI caps, device IDs) live in
 * virtio_pci.h / virtio_mmio.h. The iommu device lives in
 * virtio_iommu.h. The admin and rtc families are partially folded in
 * (the wire structs that are shared verbatim across tests).
 */
#ifndef VV_VIRTIO_SPEC_H
#define VV_VIRTIO_SPEC_H

#include <stdint.h>

/* Transport feature bits (virtio spec 6.3) */
#define VIRTIO_F_NOTIFY_ON_EMPTY     24
#define VIRTIO_F_ANY_LAYOUT          27
#define VIRTIO_F_INDIRECT_DESC       28
#define VIRTIO_F_EVENT_IDX           29
#define VIRTIO_F_VERSION_1           32
#define VIRTIO_F_ACCESS_PLATFORM     33
#define VIRTIO_F_RING_PACKED         34
#define VIRTIO_F_IN_ORDER            35
#define VIRTIO_F_ORDER_PLATFORM      36
#define VIRTIO_F_SR_IOV              37
#define VIRTIO_F_NOTIFICATION_DATA   38
#define VIRTIO_F_NOTIF_CONFIG_DATA   39
#define VIRTIO_F_RING_RESET          40
#define VIRTIO_F_ADMIN_VQ            41
#define VIRTIO_F_SUSPEND             43   /* proposal, not in a released spec */

/* Block device (virtio spec 5.2) */

/* Request types (spec 5.2.6). ZONE_* require VIRTIO_BLK_F_ZONED. */
#define VIRTIO_BLK_T_IN              0
#define VIRTIO_BLK_T_OUT             1
#define VIRTIO_BLK_T_SCSI_CMD        2   /* legacy, removed in v1.0+ */
#define VIRTIO_BLK_T_FLUSH           4
#define VIRTIO_BLK_T_GET_ID          8
#define VIRTIO_BLK_T_GET_LIFETIME    10
#define VIRTIO_BLK_T_DISCARD         11
#define VIRTIO_BLK_T_WRITE_ZEROES    13
#define VIRTIO_BLK_T_SECURE_ERASE    14
#define VIRTIO_BLK_T_ZONE_APPEND     15
#define VIRTIO_BLK_T_ZONE_REPORT     16
#define VIRTIO_BLK_T_ZONE_OPEN       18
#define VIRTIO_BLK_T_ZONE_CLOSE      20
#define VIRTIO_BLK_T_ZONE_FINISH     22
#define VIRTIO_BLK_T_ZONE_RESET      24
#define VIRTIO_BLK_T_ZONE_RESET_ALL  26
#define VIRTIO_BLK_T_BARRIER         0x80000000u  /* legacy OR-flag */

/* Feature bits (spec 5.2.3). */
#define VIRTIO_BLK_F_SIZE_MAX        1
#define VIRTIO_BLK_F_SEG_MAX         2
#define VIRTIO_BLK_F_GEOMETRY        4
#define VIRTIO_BLK_F_RO              5
#define VIRTIO_BLK_F_BLK_SIZE        6
#define VIRTIO_BLK_F_FLUSH           9
#define VIRTIO_BLK_F_TOPOLOGY        10
#define VIRTIO_BLK_F_CONFIG_WCE      11
#define VIRTIO_BLK_F_MQ              12
#define VIRTIO_BLK_F_DISCARD         13
#define VIRTIO_BLK_F_WRITE_ZEROES    14
#define VIRTIO_BLK_F_LIFETIME        15
#define VIRTIO_BLK_F_SECURE_ERASE    16
#define VIRTIO_BLK_F_ZONED           17
#define VIRTIO_BLK_F_REQ_FLAGS       18
#define VIRTIO_BLK_F_REQ_FLAGS_OUT_FUA 19

/* Request flags bitfield (spec 5.2.6, valid when REQ_FLAGS negotiated). */
#define VIRTIO_BLK_REQ_FLAG_OUT_FUA  0  /* bit index, VIRTIO_BLK_T_OUT only */

/* Status codes (spec 5.2.6). */
#define VIRTIO_BLK_S_OK              0
#define VIRTIO_BLK_S_IOERR           1
#define VIRTIO_BLK_S_UNSUPP          2

/* Zoned block status codes (spec 5.2.6.6). */
#define VIRTIO_BLK_S_ZONE_INVALID_CMD     3
#define VIRTIO_BLK_S_ZONE_UNALIGNED_WP    4
#define VIRTIO_BLK_S_ZONE_OPEN_RESOURCE   5
#define VIRTIO_BLK_S_ZONE_ACTIVE_RESOURCE 6

/* Write-zeroes flags (spec 5.2.6.2). */
#define VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP 0x1

/* GET_ID returns a NUL-padded ASCII string of this length (spec 5.2.6). */
#define VIRTIO_BLK_ID_BYTES         20

/* Config space byte offsets (spec 5.2.4). */
#define VIRTIO_BLK_CFG_WCE_OFFSET   32  /* writeback control byte */

/* Request header, prepended to every request (spec 5.2.6). */
struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

/* Request header under its alternate name used by many tests. The
 * middle word is the reserved field rather than ioprio; same layout. */
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

/* Zone report header (spec 5.2.6.2.1). Tests that need a wider
 * nr_zones field keep their own local copy. */
struct virtio_blk_zone_report_hdr {
    uint32_t nr_zones;
    uint8_t  reserved[60];
} __attribute__((packed));

/* Discard / write-zeroes segment descriptor (spec 5.2.6.2). */
struct virtio_blk_discard_write_zeroes {
    uint64_t sector;
    uint32_t num_sectors;
    uint32_t flags;
} __attribute__((packed));

/* Config view exposing only the leading capacity field. */
struct virtio_blk_config {
    uint64_t capacity;
} __attribute__((packed));

/* Alternate name for the leading capacity field of the config. */
struct virtio_blk_config_head {
    uint64_t capacity;
} __attribute__((packed));

/* Device lifetime data returned by GET_LIFETIME (spec 5.2.6). */
struct virtio_blk_lifetime {
    uint32_t pre_eol_info;
    uint32_t device_lifetime_est_typ_a;
    uint32_t device_lifetime_est_typ_b;
} __attribute__((packed));

/* Zone report header with a wider 64-bit zone count. */
struct virtio_blk_zone_report_hdr_wide {
    uint64_t nr_zones;
} __attribute__((packed));

/* Network device (virtio spec 5.1) */

/* Feature bits (spec 5.1.3). */
#define VIRTIO_NET_F_CSUM                  0
#define VIRTIO_NET_F_GUEST_CSUM            1
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS   2
#define VIRTIO_NET_F_MTU                   3
#define VIRTIO_NET_F_MAC                   5
#define VIRTIO_NET_F_GSO                   6   /* legacy */
#define VIRTIO_NET_F_GUEST_TSO4            7
#define VIRTIO_NET_F_GUEST_TSO6            8
#define VIRTIO_NET_F_GUEST_ECN            9
#define VIRTIO_NET_F_GUEST_UFO            10
#define VIRTIO_NET_F_HOST_TSO4           11
#define VIRTIO_NET_F_HOST_TSO6           12
#define VIRTIO_NET_F_HOST_ECN            13
#define VIRTIO_NET_F_HOST_UFO            14
#define VIRTIO_NET_F_MRG_RXBUF           15
#define VIRTIO_NET_F_STATUS              16
#define VIRTIO_NET_F_CTRL_VQ             17
#define VIRTIO_NET_F_CTRL_RX             18
#define VIRTIO_NET_F_CTRL_VLAN           19
#define VIRTIO_NET_F_CTRL_RX_EXTRA       20
#define VIRTIO_NET_F_GUEST_ANNOUNCE      21
#define VIRTIO_NET_F_MQ                  22
#define VIRTIO_NET_F_CTRL_MAC_ADDR       23
#define VIRTIO_NET_F_DEVICE_STATS        50
#define VIRTIO_NET_F_VQ_NOTF_COAL        52
#define VIRTIO_NET_F_NOTF_COAL           53
#define VIRTIO_NET_F_GUEST_USO4          54
#define VIRTIO_NET_F_GUEST_USO6          55
#define VIRTIO_NET_F_HOST_USO            56
#define VIRTIO_NET_F_HASH_REPORT         57
#define VIRTIO_NET_F_GUEST_HDRLEN        59
#define VIRTIO_NET_F_RSS                 60
#define VIRTIO_NET_F_RSC_EXT             61
#define VIRTIO_NET_F_STANDBY             62
#define VIRTIO_NET_F_SPEED_DUPLEX        63
#define VIRTIO_NET_F_RSS_CONTEXT         64
#define VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO      65
#define VIRTIO_NET_F_GUEST_UDP_TUNNEL_GSO_CSUM 66
#define VIRTIO_NET_F_HOST_UDP_TUNNEL_GSO       67
#define VIRTIO_NET_F_HOST_UDP_TUNNEL_GSO_CSUM  68
#define VIRTIO_NET_F_OUT_NET_HEADER            69
#define VIRTIO_NET_F_IPSEC                     70

/* Control virtqueue command classes (spec 5.1.6.5). */
#define VIRTIO_NET_CTRL_RX               0
#define VIRTIO_NET_CTRL_MAC              1
#define VIRTIO_NET_CTRL_VLAN             2
#define VIRTIO_NET_CTRL_ANNOUNCE         3
#define VIRTIO_NET_CTRL_MQ               4
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS   5
#define VIRTIO_NET_CTRL_NOTF_COAL        6
#define VIRTIO_NET_CTRL_HASH_TUNNEL      7   /* proposal, not in a released spec */
#define VIRTIO_NET_CTRL_STATS            8
#define VIRTIO_NET_CTRL_RSS_CTX          9   /* spec VIRTNET_RSS_CTX_CTRL */

/* CTRL_RX commands. */
#define VIRTIO_NET_CTRL_RX_PROMISC       0
#define VIRTIO_NET_CTRL_RX_ALLMULTI      1
#define VIRTIO_NET_CTRL_RX_ALLUNI        2
#define VIRTIO_NET_CTRL_RX_NOMULTI       3
#define VIRTIO_NET_CTRL_RX_NOUNI         4
#define VIRTIO_NET_CTRL_RX_NOBCAST       5

/* CTRL_MAC commands. */
#define VIRTIO_NET_CTRL_MAC_TABLE_SET    0
#define VIRTIO_NET_CTRL_MAC_ADDR_SET     1

/* CTRL_VLAN commands. */
#define VIRTIO_NET_CTRL_VLAN_ADD         0
#define VIRTIO_NET_CTRL_VLAN_DEL         1

/* CTRL_ANNOUNCE commands. */
#define VIRTIO_NET_CTRL_ANNOUNCE_ACK     0

/* CTRL_MQ commands. */
#define VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET  0
#define VIRTIO_NET_CTRL_MQ_RSS_CONFIG    1
#define VIRTIO_NET_CTRL_MQ_HASH_CONFIG   2

/* CTRL_GUEST_OFFLOADS commands. */
#define VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET 0

/* CTRL_NOTF_COAL commands. */
#define VIRTIO_NET_CTRL_NOTF_COAL_TX_SET 0
#define VIRTIO_NET_CTRL_NOTF_COAL_RX_SET 1
#define VIRTIO_NET_CTRL_NOTF_COAL_VQ_SET 2
#define VIRTIO_NET_CTRL_NOTF_COAL_VQ_GET 3

/* CTRL_STATS commands. */
#define VIRTIO_NET_CTRL_STATS_QUERY      0
#define VIRTIO_NET_CTRL_STATS_GET        1

/* Device statistics types (spec 5.1.6.5.9). Bits in the capabilities
 * bitmap returned by STATS_QUERY and requested in STATS_GET. */
#define VIRTIO_NET_STATS_TYPE_RX_BASIC  (1ULL << 0)
#define VIRTIO_NET_STATS_TYPE_RX_CSUM   (1ULL << 1)
#define VIRTIO_NET_STATS_TYPE_RX_GSO    (1ULL << 2)
#define VIRTIO_NET_STATS_TYPE_RX_SPEED  (1ULL << 3)
#define VIRTIO_NET_STATS_TYPE_TX_BASIC  (1ULL << 16)
#define VIRTIO_NET_STATS_TYPE_TX_CSUM   (1ULL << 17)
#define VIRTIO_NET_STATS_TYPE_TX_GSO    (1ULL << 18)
#define VIRTIO_NET_STATS_TYPE_TX_SPEED  (1ULL << 19)
#define VIRTIO_NET_STATS_TYPE_CVQ       (1ULL << 32)

/* STATS_QUERY reply. */
struct virtio_net_stats_capabilities {
    uint64_t supported_stats_types;
} __attribute__((packed));

/* CTRL_RSS_CTX commands (spec VIRTNET_RSS_CTX_CTRL). */
#define VIRTIO_NET_CTRL_RSS_CTX_CAP_GET  0
#define VIRTIO_NET_CTRL_RSS_CTX_ADD      1
#define VIRTIO_NET_CTRL_RSS_CTX_MOD      2
#define VIRTIO_NET_CTRL_RSS_CTX_DEL      3

/* CTRL_HASH_TUNNEL commands (proposal, not in a released spec). */
#define VIRTIO_NET_CTRL_HASH_TUNNEL_SET  0

/* Control ack values. */
#define VIRTIO_NET_OK                    0
#define VIRTIO_NET_ERR                   1

/* Header flags (spec 5.1.6). */
#define VIRTIO_NET_HDR_F_NEEDS_CSUM      1
#define VIRTIO_NET_HDR_F_DATA_VALID      2
#define VIRTIO_NET_HDR_F_RSC_INFO        4

/* Header GSO types (spec 5.1.6). */
#define VIRTIO_NET_HDR_GSO_NONE          0
#define VIRTIO_NET_HDR_GSO_TCPV4         1
#define VIRTIO_NET_HDR_GSO_UDP           3
#define VIRTIO_NET_HDR_GSO_TCPV6         4
#define VIRTIO_NET_HDR_GSO_UDP_L4        5
#define VIRTIO_NET_HDR_GSO_ECN           0x80

/* Config status bits (spec 5.1.4). */
#define VIRTIO_NET_S_LINK_UP             1
#define VIRTIO_NET_S_ANNOUNCE            2

/* Config space byte offsets (spec 5.1.4). */
#define VIRTIO_NET_CFG_MAC_OFFSET        0
#define VIRTIO_NET_CFG_STATUS_OFFSET     6

/* Packet header, prepended to every buffer (spec 5.1.6). The basic
 * layout; the mergeable-buffers variant adds num_buffers below. */
struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

/* Device configuration layout (spec 5.1.4). Tests reading only the
 * leading fields use this same struct; trailing fields exist only
 * when the matching feature is negotiated. */
struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t  duplex;
} __attribute__((packed));

/* Mergeable-buffers packet header (VIRTIO_NET_F_MRG_RXBUF). */
struct virtio_net_hdr_mrg {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

/* Control queue command header (spec 5.1.6.5). */
struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

/* CTRL_MQ virtqueue-pairs payload. */
struct virtio_net_ctrl_mq {
    uint16_t virtqueue_pairs;
} __attribute__((packed));

/* CTRL_NOTF_COAL payload. */
struct virtio_net_ctrl_coal {
    uint32_t max_packets;
    uint32_t max_usecs;
} __attribute__((packed));

/* CTRL_NOTF_COAL per-virtqueue payload. */
struct virtio_net_ctrl_coal_vq {
    uint16_t vq_index;
    uint16_t reserved;
    uint32_t max_packets;
    uint32_t max_usecs;
} __attribute__((packed));

/* CTRL_MQ hash/RSS config payload. */
struct virtio_net_hash_config {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint8_t hash_key_length;
    uint8_t hash_key[40];
} __attribute__((packed));

/* CTRL_MQ RSS config payload with a single indirection entry. Tests
 * that need a wider indirection table keep their own local copy. */
struct virtio_net_rss_config {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint16_t indirection_table[1];
    uint16_t max_tx_vq;
    uint8_t  hash_key_length;
    uint8_t  hash_key_data[40];
} __attribute__((packed));

/* RSS config payload with a 16-entry indirection table. */
struct virtio_net_rss_config_full {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint16_t indirection_table[16];
    uint16_t max_tx_vq;
    uint8_t  hash_key_length;
    uint8_t  hash_key[40];
} __attribute__((packed));

/* Packet header with the hash report trailer (VIRTIO_NET_F_HASH_REPORT). */
struct virtio_net_hdr_hash {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
    uint32_t hash_value;
    uint16_t hash_report_type;
    uint16_t padding;
} __attribute__((packed));

/* CTRL_NOTF_COAL_VQ_GET request and response payloads. */
struct virtio_net_ctrl_coal_vq_get_req {
    uint16_t vq_index;
    uint16_t reserved;
} __attribute__((packed));

struct virtio_net_ctrl_coal_vq_get_resp {
    uint32_t max_packets;
    uint32_t max_usecs;
} __attribute__((packed));

/* Hash config payload truncated to the header, with no key bytes. */
struct virtio_net_hash_config_short {
    uint32_t hash_types;
    uint16_t indirection_table_mask;
    uint16_t unclassified_queue;
    uint8_t  hash_key_length;
} __attribute__((packed));

/* CTRL_MAC table set payload with a flexible array of MAC entries. */
struct virtio_net_ctrl_mac {
    uint32_t entries;
    uint8_t  macs[][6];
} __attribute__((packed));

/* CTRL_MAC table header with a zero entry count. */
struct mac_table_zero {
    uint32_t entries;
} __attribute__((packed));

/* CTRL_MQ virtqueue-pairs payload under an alternate name. */
struct virtio_net_ctrl_mq_pairs {
    uint16_t virtqueue_pairs;
} __attribute__((packed));

/* Socket device / vsock (virtio spec 5.10) */

/* Feature bits. */
#define VIRTIO_VSOCK_F_SEQPACKET         1
#define VIRTIO_VSOCK_F_DGRAM             3   /* proposal, not in a released spec */

/* Operations (spec 5.10.6). */
#define VIRTIO_VSOCK_OP_INVALID          0
#define VIRTIO_VSOCK_OP_REQUEST          1
#define VIRTIO_VSOCK_OP_RESPONSE         2
#define VIRTIO_VSOCK_OP_RST              3
#define VIRTIO_VSOCK_OP_SHUTDOWN         4
#define VIRTIO_VSOCK_OP_RW               5
#define VIRTIO_VSOCK_OP_CREDIT_UPDATE    6
#define VIRTIO_VSOCK_OP_CREDIT_REQUEST   7

/* Socket types. */
#define VIRTIO_VSOCK_TYPE_STREAM         1
#define VIRTIO_VSOCK_TYPE_SEQPACKET      2
#define VIRTIO_VSOCK_TYPE_DGRAM          3   /* proposal, not in a released spec */

/* SHUTDOWN flags. */
#define VIRTIO_VSOCK_SHUTDOWN_RCV        1
#define VIRTIO_VSOCK_SHUTDOWN_SEND       2
#define VIRTIO_VSOCK_SHUTDOWN_BOTH       3   /* RCV | SEND */

/* RW (seqpacket) flags. */
#define VIRTIO_VSOCK_SEQ_EOM             1
#define VIRTIO_VSOCK_SEQ_EOR             2

/* Event ids. */
#define VIRTIO_VSOCK_EVENT_TRANSPORT_RESET 0

/* Packet header, prepended to every vsock buffer (spec 5.10.6). */
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

/* Event queue entry (spec 5.10.6). */
struct virtio_vsock_event {
    uint32_t id;
} __attribute__((packed));

/* Memory device / virtio-mem (virtio spec 5.15) */

/* Feature bits. */
#define VIRTIO_MEM_F_ACPI_PXM               0
#define VIRTIO_MEM_F_UNPLUGGED_INACCESSIBLE 1
#define VIRTIO_MEM_F_PERSISTENT_SUSPEND     2

/* Guest -> host request types. */
#define VIRTIO_MEM_REQ_PLUG                 0
#define VIRTIO_MEM_REQ_UNPLUG               1
#define VIRTIO_MEM_REQ_UNPLUG_ALL           2
#define VIRTIO_MEM_REQ_STATE                3

/* Host -> guest response codes. */
#define VIRTIO_MEM_RESP_ACK                 0
#define VIRTIO_MEM_RESP_NACK                1
#define VIRTIO_MEM_RESP_BUSY                2
#define VIRTIO_MEM_RESP_ERROR               3

/* Block state values. */
#define VIRTIO_MEM_STATE_PLUGGED            0
#define VIRTIO_MEM_STATE_UNPLUGGED          1
#define VIRTIO_MEM_STATE_MIXED              2

/* Guest -> host request, written to the request queue (spec 5.15.6). */
struct virtio_mem_req {
    uint16_t type;
    uint16_t padding[3];
    uint64_t addr;
    uint16_t nb_blocks;
    uint16_t padding1[3];
} __attribute__((packed));

/* Host -> guest response, written back by the device (spec 5.15.6). */
struct virtio_mem_resp {
    uint16_t type;
    uint16_t padding[3];
    uint16_t state;
} __attribute__((packed));

/* Response header without the trailing state field, used by tests
 * that only inspect the type word. */
struct virtio_mem_resp_short {
    uint16_t type;
    uint16_t padding[3];
} __attribute__((packed));

/* Response header with a wider 64-bit state field. */
struct virtio_mem_resp_wide {
    uint16_t type;
    uint16_t padding[3];
    uint64_t state;
} __attribute__((packed));

/* Device configuration layout (spec 5.15.4). */
struct virtio_mem_config {
    uint64_t block_size;
    uint16_t node_id;
    uint8_t  padding[6];
    uint64_t addr;
    uint64_t region_size;
    uint64_t usable_region_size;
    uint64_t plugged_size;
    uint64_t requested_size;
} __attribute__((packed));

/* Console device (virtio spec 5.3) */

/* Feature bits. */
#define VIRTIO_CONSOLE_F_SIZE            0
#define VIRTIO_CONSOLE_F_MULTIPORT       1
#define VIRTIO_CONSOLE_F_EMERG_WRITE     2

/* Control events. */
#define VIRTIO_CONSOLE_DEVICE_READY      0
#define VIRTIO_CONSOLE_PORT_ADD          1
#define VIRTIO_CONSOLE_PORT_REMOVE       2
#define VIRTIO_CONSOLE_PORT_READY        3
#define VIRTIO_CONSOLE_CONSOLE_PORT      4
#define VIRTIO_CONSOLE_RESIZE            5
#define VIRTIO_CONSOLE_PORT_OPEN         6
#define VIRTIO_CONSOLE_PORT_NAME         7

/* Control message, exchanged on the control queue (spec 5.3.6.3). */
struct virtio_console_control {
    uint32_t id;
    uint16_t event;
    uint16_t value;
} __attribute__((packed));

/* Device configuration layout (spec 5.3.4). */
struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute__((packed));

/* Resize control payload (spec 5.3.6.3). */
struct virtio_console_resize {
    uint16_t rows;
    uint16_t cols;
} __attribute__((packed));

/* Memory balloon device (virtio spec 5.5) */

/* Feature bits. */
#define VIRTIO_BALLOON_F_MUST_TELL_HOST  0
#define VIRTIO_BALLOON_F_STATS_VQ        1
#define VIRTIO_BALLOON_F_DEFLATE_ON_OOM  2
#define VIRTIO_BALLOON_F_FREE_PAGE_HINT  3
#define VIRTIO_BALLOON_F_PAGE_POISON     4
#define VIRTIO_BALLOON_F_REPORTING       5

/* PFN shift used by the inflate/deflate queues. */
#define VIRTIO_BALLOON_PFN_SHIFT         12

/* Free-page-hint command ids. */
#define VIRTIO_BALLOON_CMD_ID_STOP       0
#define VIRTIO_BALLOON_CMD_ID_DONE       1

/* Memory statistics tags. */
#define VIRTIO_BALLOON_S_SWAP_IN         0
#define VIRTIO_BALLOON_S_SWAP_OUT        1
#define VIRTIO_BALLOON_S_MAJFLT          2
#define VIRTIO_BALLOON_S_MINFLT          3
#define VIRTIO_BALLOON_S_MEMFREE         4
#define VIRTIO_BALLOON_S_MEMTOT          5
#define VIRTIO_BALLOON_S_AVAIL           6
#define VIRTIO_BALLOON_S_CACHES          7
#define VIRTIO_BALLOON_S_HTLB_PGALLOC    8
#define VIRTIO_BALLOON_S_HTLB_PGFAIL     9

/* Statistics queue entry (spec 5.5.6). */
struct virtio_balloon_stat {
    uint16_t tag;
    uint64_t val;
} __attribute__((packed));

/* Leading config fields: balloon target and actual page counts. */
struct virtio_balloon_config_head {
    uint32_t num_pages;
    uint32_t actual;
} __attribute__((packed));

/* Persistent memory device / virtio-pmem (virtio spec 5.16) */

/* Feature bits. */
#define VIRTIO_PMEM_F_SHMEM_REGION       0
#define VIRTIO_PMEM_F_DISCARD            1   /* proposal, not in a released spec */

/* Request types. */
#define VIRTIO_PMEM_REQ_TYPE_FLUSH       0
#define VIRTIO_PMEM_REQ_TYPE_DISCARD     1   /* proposal, not in a released spec */

/* Request, written to the request queue (spec 5.16.6). */
struct virtio_pmem_req {
    uint32_t type;
} __attribute__((packed));

/* Response, written back by the device (spec 5.16.6). */
struct virtio_pmem_resp {
    uint32_t ret;
} __attribute__((packed));

/* Shared memory region config exposed by VIRTIO_PMEM_F_SHMEM_REGION. */
struct pmem_config {
    uint64_t start;
    uint64_t size;
} __attribute__((packed));

/* Indirect descriptor table entry used by the flush test. */
struct ind_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

/* Crypto device / virtio-crypto (virtio spec 5.9) */

/* Config status bit: the accelerator hardware is ready. */
#define VIRTIO_CRYPTO_S_HW_READY   (1u << 0)

/* Service masks in crypto_services. */
#define VIRTIO_CRYPTO_SERVICE_CIPHER   0
#define VIRTIO_CRYPTO_SERVICE_HASH     1
#define VIRTIO_CRYPTO_SERVICE_MAC      2
#define VIRTIO_CRYPTO_SERVICE_AEAD     3
#define VIRTIO_CRYPTO_SERVICE_AKCIPHER 4

/* Per request status codes returned by the device. */
#define VIRTIO_CRYPTO_OK           0
#define VIRTIO_CRYPTO_ERR          1
#define VIRTIO_CRYPTO_BADMSG       2
#define VIRTIO_CRYPTO_NOTSUPP      3
#define VIRTIO_CRYPTO_INVSESS      4
#define VIRTIO_CRYPTO_NOSPC        5
#define VIRTIO_CRYPTO_KEY_REJECTED 6

/* Device configuration layout (spec 5.9.4). */
struct virtio_crypto_config {
    uint32_t status;
    uint32_t max_dataqueues;
    uint32_t crypto_services;
    uint32_t cipher_algo_l;
    uint32_t cipher_algo_h;
    uint32_t hash_algo;
    uint32_t mac_algo_l;
    uint32_t mac_algo_h;
    uint32_t aead_algo;
    uint32_t max_cipher_key_len;
    uint32_t max_auth_key_len;
    uint32_t akcipher_algo;
    uint64_t max_size;
} __attribute__((packed));

/* Control request opcodes are (service << 8) | op (spec 5.9.7). */
#define VIRTIO_CRYPTO_CIPHER_CREATE_SESSION  0x0002
#define VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION 0x0003

/* Cipher algorithm ids (subset, spec 5.9.7.3). */
#define VIRTIO_CRYPTO_CIPHER_AES_CBC   3

/* Cipher direction. */
#define VIRTIO_CRYPTO_OP_ENCRYPT  1
#define VIRTIO_CRYPTO_OP_DECRYPT  2

/* Symmetric op type. */
#define VIRTIO_CRYPTO_SYM_OP_CIPHER  1

/* Control queue request header (spec 5.9.7). */
struct virtio_crypto_ctrl_header {
    uint32_t opcode;
    uint32_t algo;
    uint32_t flag;
    uint32_t queue_id;
} __attribute__((packed));

/* Cipher session parameters (spec 5.9.7.3.1). */
struct virtio_crypto_cipher_session_para {
    uint32_t algo;
    uint32_t keylen;
    uint32_t op;         /* VIRTIO_CRYPTO_OP_* */
    uint32_t padding;
} __attribute__((packed));

/* Hash control opcodes and algorithm ids (spec 5.9.9). */
#define VIRTIO_CRYPTO_HASH_CREATE_SESSION   0x0102
#define VIRTIO_CRYPTO_HASH_DESTROY_SESSION  0x0103
#define VIRTIO_CRYPTO_HASH_SHA1             2
#define VIRTIO_CRYPTO_HASH_SHA_256          4

/* Hash session parameters (spec 5.9.9). */
struct virtio_crypto_hash_session_para {
    uint32_t algo;
    uint32_t hash_result_len;
    uint8_t padding[8];
} __attribute__((packed));

/* MAC control opcodes and algorithm ids (spec 5.9.9.2). */
#define VIRTIO_CRYPTO_MAC_CREATE_SESSION    0x0202
#define VIRTIO_CRYPTO_MAC_DESTROY_SESSION   0x0203
#define VIRTIO_CRYPTO_MAC_HMAC_SHA1         2

/* MAC session parameters (spec 5.9.9.2). */
struct virtio_crypto_mac_session_para {
    uint32_t algo;
    uint32_t hash_result_len;
    uint32_t auth_key_len;
    uint32_t padding;
} __attribute__((packed));

/* Akcipher control opcodes, algorithm ids, key types (spec 5.9.10). */
#define VIRTIO_CRYPTO_AKCIPHER_CREATE_SESSION   0x0404
#define VIRTIO_CRYPTO_AKCIPHER_DESTROY_SESSION  0x0405
#define VIRTIO_CRYPTO_AKCIPHER_RSA              1
#define VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC  1
#define VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PRIVATE 2
#define VIRTIO_CRYPTO_RSA_RAW_PADDING           0
#define VIRTIO_CRYPTO_RSA_NO_HASH               0

/* RSA specific akcipher session parameters (spec 5.9.10). */
struct virtio_crypto_rsa_session_para {
    uint32_t padding_algo;
    uint32_t hash_algo;
} __attribute__((packed));

/* Akcipher session parameters (spec 5.9.10). */
struct virtio_crypto_akcipher_session_para {
    uint32_t algo;
    uint32_t keytype;
    uint32_t keylen;
    struct virtio_crypto_rsa_session_para rsa;
} __attribute__((packed));

/* Device-writable session create reply (spec 5.9.7). */
struct virtio_crypto_session_input {
    uint64_t session_id;
    uint32_t status;
    uint32_t padding;
} __attribute__((packed));

/* Control request. The header is followed by an opcode specific body
 * padded to a fixed 56 bytes. A symmetric cipher create session fills
 * the cipher parameters then the symmetric op type at offset 48; a
 * destroy fills the session id at the start. */
struct virtio_crypto_op_ctrl_req {
    struct virtio_crypto_ctrl_header header;
    union {
        struct {
            struct virtio_crypto_cipher_session_para para;
            uint8_t cipher_padding[32];
            uint32_t op_type;   /* VIRTIO_CRYPTO_SYM_OP_* */
            uint32_t padding;
        } sym_create;
        struct {
            struct virtio_crypto_hash_session_para para;
            uint8_t padding[40];
        } hash_create;
        struct {
            struct virtio_crypto_mac_session_para para;
            uint8_t padding[40];
        } mac_create;
        struct {
            struct virtio_crypto_akcipher_session_para para;
            uint8_t padding[36];
        } akcipher_create;
        struct {
            uint64_t session_id;
            uint8_t padding[48];
        } destroy;
        uint8_t raw[56];
    } u;
} __attribute__((packed));

/* Device-writable one byte status for a destroy request. */
struct virtio_crypto_inhdr {
    uint8_t status;
} __attribute__((packed));

/* Data queue request opcodes (spec 5.9.8). */
#define VIRTIO_CRYPTO_CIPHER_ENCRYPT  0x0000
#define VIRTIO_CRYPTO_CIPHER_DECRYPT  0x0001
#define VIRTIO_CRYPTO_HASH            0x0100

/* Data queue request header (spec 5.9.8). */
struct virtio_crypto_op_header {
    uint32_t opcode;
    uint32_t algo;
    uint64_t session_id;
    uint32_t flag;
    uint32_t padding;
} __attribute__((packed));

/* Cipher operation parameters (spec 5.9.8.4). */
struct virtio_crypto_cipher_para {
    uint32_t iv_len;
    uint32_t src_data_len;
    uint32_t dst_data_len;
    uint32_t padding;
} __attribute__((packed));

/* Hash operation parameters (spec 5.9.8.6). */
struct virtio_crypto_hash_para {
    uint32_t src_data_len;
    uint32_t hash_result_len;
} __attribute__((packed));

/* Data queue request. The header is followed by an op specific body
 * padded to a fixed 48 bytes. A symmetric cipher op fills the cipher
 * parameters then the symmetric op type at offset 40. */
struct virtio_crypto_op_data_req {
    struct virtio_crypto_op_header header;
    union {
        struct {
            struct virtio_crypto_cipher_para para;
            uint8_t cipher_padding[24];
            uint32_t op_type;   /* VIRTIO_CRYPTO_SYM_OP_* */
            uint32_t padding;
        } sym_cipher;
        struct {
            struct virtio_crypto_hash_para para;
            uint8_t padding[40];
        } hash;
        uint8_t raw[48];
    } u;
} __attribute__((packed));

/* Admin command family (virtio spec 1.3 section 2.12). */

/* Group administration command opcodes (spec 2.12.1). Verified
 * against OASIS virtio v1.4. The legacy interface opcodes are a
 * contiguous block: write before read, common before device. */
#define VIRTIO_ADMIN_CMD_LIST_QUERY              0x0000
#define VIRTIO_ADMIN_CMD_LIST_USE                0x0001
#define VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_WRITE 0x0002
#define VIRTIO_ADMIN_CMD_LEGACY_COMMON_CFG_READ  0x0003
#define VIRTIO_ADMIN_CMD_LEGACY_DEV_CFG_WRITE    0x0004
#define VIRTIO_ADMIN_CMD_LEGACY_DEV_CFG_READ     0x0005
#define VIRTIO_ADMIN_CMD_LEGACY_NOTIFY_INFO      0x0006

/* Simplified admin command header used by the list management tests
 * (opcode, group selector, member id only). */
struct virtio_admin_cmd {
    uint16_t opcode;
    uint16_t group_type;
    uint64_t group_member_id;
} __attribute__((packed));

/* Command header prepended to every admin command (spec 2.12.1). */
struct virtio_admin_cmd_hdr {
    uint16_t opcode;
    uint16_t group_type;
    uint8_t  reserved1[12];
    uint64_t group_member_id;
} __attribute__((packed));

/* Compact admin command header variant with an inline group field and
 * a command specific data length, used by a subset of tests. */
struct virtio_admin_cmd_hdr_short {
    uint16_t opcode;
    uint16_t group_type;
    uint16_t group;
    uint16_t command_specific_data_len;
} __attribute__((packed));

/* Status block written back by the device (spec 2.12.1). */
struct virtio_admin_cmd_status {
    uint16_t status;
    uint16_t status_qualifier;
} __attribute__((packed));

/* Legacy common config access command data (spec 2.12.2). */
struct virtio_admin_cmd_legacy_cfg {
    uint8_t offset;
    uint8_t reserved[3];
} __attribute__((packed));

/* Legacy common config write command data (spec 2.12.2). */
struct virtio_admin_cmd_legacy_wr {
    uint8_t offset;
    uint8_t reserved[3];
    uint8_t data[4];
} __attribute__((packed));

/* Full admin command layout with an explicit data length field. */
struct virtio_admin_cmd_full {
    uint16_t opcode;
    uint16_t group_type;
    uint16_t group_member_id;
    uint16_t reserved1;
    uint64_t group_member_size;
    uint64_t data_length;
    uint8_t  reserved2[16];
} __attribute__((packed));

/* Compact admin header with a one byte group type and reserved byte. */
struct admin_hdr {
    uint16_t opcode;
    uint8_t  group_type;
    uint8_t  reserved;
    uint64_t group_member_id;
} __attribute__((packed));

/* Admin command response status block. */
struct admin_resp {
    uint16_t status;
    uint16_t status_qualifier;
} __attribute__((packed));

/* Real time clock device / virtio-rtc. Only the structs that are
 * byte identical across tests live here; tests with deliberately
 * divergent request or response layouts keep their own local copies. */

/* Request and notification message types, feature bits, status
 * codes, flags and counter ids. Verified against OASIS virtio v1.4
 * (RTC Device section). */
#define VIRTIO_RTC_REQ_READ              0x0001
#define VIRTIO_RTC_REQ_READ_CROSS        0x0002
#define VIRTIO_RTC_REQ_CFG               0x1000
#define VIRTIO_RTC_REQ_CLOCK_CAP         0x1001
#define VIRTIO_RTC_REQ_CROSS_CAP         0x1002
#define VIRTIO_RTC_REQ_READ_ALARM        0x1003
#define VIRTIO_RTC_REQ_SET_ALARM         0x1004
#define VIRTIO_RTC_REQ_SET_ALARM_ENABLED 0x1005
#define VIRTIO_RTC_NOTIF_ALARM           0x2000

#define VIRTIO_RTC_F_ALARM               0

#define VIRTIO_RTC_S_OK                  0
#define VIRTIO_RTC_S_EOPNOTSUPP          2
#define VIRTIO_RTC_S_ENODEV              3
#define VIRTIO_RTC_S_EINVAL              4
#define VIRTIO_RTC_S_EIO                 5

#define VIRTIO_RTC_FLAG_ALARM_ENABLED    (1 << 0)
#define VIRTIO_RTC_FLAG_ALARM_CAP        (1 << 0)
#define VIRTIO_RTC_FLAG_CROSS_CAP        (1 << 0)

#define VIRTIO_RTC_COUNTER_ARM_VCT       0
#define VIRTIO_RTC_COUNTER_X86_TSC       1
#define VIRTIO_RTC_COUNTER_INVALID       0xFF

/* Common request header prefix. */
struct rtc_req_head {
    uint16_t msg_type;
    uint8_t  reserved[6];
} __attribute__((packed));

/* Response to the device configuration request. */
struct rtc_resp_cfg {
    uint8_t  status;
    uint8_t  r0[7];
    uint16_t num_clocks;
    uint8_t  r1[6];
} __attribute__((packed));

/* Cross timestamp read request. */
struct rtc_req_read_cross {
    uint16_t msg_type;
    uint8_t  r0[6];
    uint16_t clock_id;
    uint8_t  hw_counter;
    uint8_t  r1[5];
} __attribute__((packed));

/* Time read request. The trailing reserved fields are padding only
 * and are never accessed by name. */
struct rtc_req_read {
    uint16_t msg_type;
    uint8_t  reserved[6];
    uint16_t clock_id;
    uint8_t  reserved2[6];
};

/* Clock capability request and response. Tests with the older
 * unpacked layout keep their own local copy. */
struct rtc_req_clock_cap {
    uint16_t msg_type;
    uint16_t clock_id;
    uint32_t reserved;
} __attribute__((packed));

struct rtc_resp_clock_cap {
    uint8_t  status;
    uint8_t  reserved[7];
    uint64_t resolution;
    uint64_t flags;
} __attribute__((packed));

/* Time read response. The 64-bit reading is in nanoseconds. Tests
 * with a divergent packed request layout keep their own copy. */
struct rtc_resp_read {
    uint8_t  status;
    uint8_t  reserved[7];
    uint64_t time_ns;
};

/* Cross timestamp read response. */
struct rtc_resp_read_cross {
    uint8_t status; uint8_t r0[7];
    uint64_t clock_reading;
    uint64_t counter_cycles;
};

/* Cross capability request and response. */
struct rtc_req_cross_cap {
    uint16_t msg_type; uint8_t r0[6];
    uint16_t clock_id; uint8_t hw_counter; uint8_t r1[5];
};

struct rtc_resp_cross_cap {
    uint8_t status; uint8_t r0[7];
    uint8_t flags; uint8_t r1[7];
};

/* Generic response status block. */
struct rtc_resp {
    uint8_t  status;
    uint8_t  reserved[7];
};

/* Response header prefix used by the alarm tests. */
struct rtc_resp_head { uint8_t status; uint8_t reserved[7]; };

/* Alarm notification message. */
struct rtc_notif_alarm {
    uint16_t msg_type;
    uint8_t reserved0[6];
    uint16_t clock_id;
    uint8_t reserved1[6];
} __attribute__((packed));

/* Older unpacked clock capability request and response layout. */
struct rtc_req_clock_cap_legacy {
    uint16_t msg_type; uint8_t r0[6];
    uint16_t clock_id; uint8_t r1[6];
};

struct rtc_resp_clock_cap_legacy {
    uint8_t status; uint8_t r0[7];
    uint8_t type; uint8_t leap_second_smearing; uint8_t flags;
    uint8_t r1[5];
};

/* Set alarm request with an embedded request header. */
struct rtc_req_set_alarm_nested {
    struct rtc_req_head head;
    uint64_t alarm_time;
    uint16_t clock_id;
    uint8_t flags;
    uint8_t reserved[5];
} __attribute__((packed));

/* Set alarm request with a flat field layout. */
struct rtc_req_set_alarm_flat {
    uint16_t msg_type;
    uint8_t  reserved[6];
    uint16_t alarm_id;
    uint8_t  reserved2[6];
    uint64_t time_ns;
};

/* Packed time read request and response variant. */
struct rtc_req_read_packed {
    uint16_t msg_type;
    uint16_t clock_id;
    uint32_t reserved;
} __attribute__((packed));

struct rtc_resp_read_packed {
    uint8_t  status;
    uint8_t  pad[7];
    uint64_t time_ns;
} __attribute__((packed));

#endif /* VV_VIRTIO_SPEC_H */
