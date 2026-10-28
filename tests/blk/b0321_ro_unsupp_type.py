# SPDX-License-Identifier: Apache-2.0
"""Launch B0321 with a read only block device."""


def vmm_config():
    return {"opts": {"blk_read_only": True}}
