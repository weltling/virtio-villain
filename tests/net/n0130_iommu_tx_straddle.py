# SPDX-License-Identifier: Apache-2.0
"""
Launch requirements for N0130 (net TX buffer straddling two IOMMU
mappings).

The test drives the virtio-net device and the virtio-iommu directly
from the guest, so it needs two things from the launch:

  * the net device placed behind the vIOMMU (net_iommu opt), and
  * the guest kernel to leave the net and iommu devices alone, so the
    test fully owns them. If the kernel iommu driver stays active it
    turns off the global bypass and the harness ring setup then fails
    translation before the test runs.

This sidecar only declares configuration; it has no run(ctx) host
actor, so the runner does not start a sidecar thread for it.
"""


def vmm_config():
    return {
        "opts": {"net_iommu": True},
        "skip_initcalls": [
            "virtio_net_driver_init",
            "virtio_iommu_drv_init",
        ],
    }
