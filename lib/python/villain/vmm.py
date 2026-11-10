# SPDX-License-Identifier: Apache-2.0
"""Shared VMM command builders and bundled guest kernel selection."""

from dataclasses import dataclass
import os
from pathlib import Path
import platform
import re
import sys
import threading
from typing import Optional, Union
from villain import artifacts

_SCRIPT_DIR = str(Path(__file__).resolve().parents[3])
DEFAULT_VMM_TRACE_FILE = "/tmp/vmm-trace.log"
KERNEL_REPOSITORY = "cloud-hypervisor/linux"
KERNEL_NAMES = {"x86_64": "vmlinux-x86_64", "aarch64": "Image-arm64"}


def _bump_maxmem(mem_str):
    """Return a maxmem string strictly larger than *mem_str* (e.g. '1G' -> '2G')."""
    m = re.fullmatch(r"(\d+)\s*([KMGkmg])?", mem_str)
    if not m:
        return "2G"
    val, suffix = int(m.group(1)), (m.group(2) or "M").upper()
    sizes = {"K": 1, "M": 1024, "G": 1024 * 1024}
    kb = val * sizes.get(suffix, 1)
    # At least double, minimum 2G
    target = max(kb * 2, 2 * 1024 * 1024)
    if target % (1024 * 1024) == 0:
        return f"{target // (1024 * 1024)}G"
    if target % 1024 == 0:
        return f"{target // 1024}M"
    return f"{target}K"


# ---------------------------------------------------------------------------
# VMM backends
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class LaunchConfig:
    """Portable boot and resource settings without device fixtures."""

    cpus: int
    memory: str
    kernel: Optional[Union[str, os.PathLike[str]]] = None
    initramfs: Optional[Union[str, os.PathLike[str]]] = None
    cmdline: Optional[str] = None
    firmware: Optional[Union[str, os.PathLike[str]]] = None

    def __post_init__(self):
        if isinstance(self.cpus, bool) or not isinstance(self.cpus, int) or self.cpus < 1:
            raise ValueError("CPU count must be a positive integer")
        if not isinstance(self.memory, str) or not self.memory.strip():
            raise ValueError("Memory size must be a nonempty string")


class VmmBackend:
    """Base class for VMM-specific command building and settings."""

    name = None
    console_device = "hvc0"
    mmio_transport = False
    # Whether this backend can emit a tracepoint log via --vmm-trace.
    supports_trace = False

    def __init__(self, binary):
        self.binary = binary

    def build_launch_cmd(self, config: LaunchConfig) -> list[str]:
        """Build launch arguments without attaching test device fixtures."""
        raise NotImplementedError(f"{self.name} does not implement launch command building")

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        raise NotImplementedError

    def _boot_args(self, config: LaunchConfig, *, kernel: str, initramfs: str,
                   cmdline: str, firmware: Optional[str] = None) -> list[str]:
        command = []
        for field, flag in (("kernel", kernel), ("initramfs", initramfs),
                            ("cmdline", cmdline), ("firmware", firmware)):
            value = getattr(config, field)
            if value is not None:
                if flag is None:
                    raise ValueError(
                        f"{self.name} {field} boot is not supported by this launch builder")
                command.extend((flag, str(value)))
        return command

    def _qemu_launch_cmd(self, config: LaunchConfig, *, machine: Optional[str] = None,
                         cpu_model: Optional[str] = None, modern_mmio: bool = False,
                         kvm: bool = False, accel: Optional[str] = None,
                         nographic: bool = False, no_reboot: bool = False,
                         memory_max: Optional[str] = None,
                         memory_slots: Optional[int] = None) -> list[str]:
        command = [self.binary]
        if machine is not None:
            command.extend(("-M", machine))
        use_kvm = accel == "kvm" if accel is not None else kvm
        if cpu_model is None and use_kvm and platform.machine() == "aarch64":
            cpu_model = "host"
        if cpu_model is not None:
            command.extend(("-cpu", cpu_model))
        if modern_mmio:
            command.extend(("-global", "virtio-mmio.force-legacy=false"))
        if accel is not None:
            command.extend(("-accel", accel))
        elif kvm:
            command.append("-enable-kvm")
        command.extend(self._boot_args(
            config, kernel="-kernel", initramfs="-initrd", cmdline="-append", firmware="-bios"))
        if nographic:
            command.append("-nographic")
        if no_reboot:
            command.append("-no-reboot")
        memory = config.memory
        if memory_max is not None:
            memory += f",maxmem={memory_max}"
        if memory_slots is not None:
            memory += f",slots={memory_slots}"
        command.extend(("-m", memory, "-smp", str(config.cpus)))
        return command

    def trace_args(self, opts):
        """Return VMM-specific tracing flags for opts, or [] if none.

        opts['vmm_trace'] is a list of event names or globs and
        opts['vmm_trace_file'] is the log path. Backends that set
        supports_trace override this; the base returns nothing.
        """
        return []

    @staticmethod
    def _qemu_trace_flags(opts):
        """Build QEMU -D/--trace flags from the generic trace opts."""
        events = opts.get("vmm_trace") or []
        if not events:
            return []
        args = ["-D", opts.get("vmm_trace_file") or DEFAULT_VMM_TRACE_FILE]
        for ev in events:
            args += ["--trace", ev]
        return args

    @staticmethod
    def _qemu_drive_arg(disk_path, qemu_fmt, opts):
        """Build the QEMU -drive value from the generic IO opts.

        Maps the cross backend --io-engine to QEMU aio modes and adds
        O_DIRECT when requested. QEMU native AIO requires direct IO, so
        the aio engine turns it on regardless of --direct.
        """
        aio_map = {"default": None, "io_uring": "io_uring",
                   "aio": "native", "sync": "threads"}
        engine = opts.get("io_engine", "default")
        aio = aio_map.get(engine)
        direct = bool(opts.get("direct"))
        if engine == "aio":
            direct = True
        parts = [f"file={disk_path}", f"format={qemu_fmt}",
                 "if=none", "id=vd0"]
        if aio:
            parts.append(f"aio={aio}")
        if direct:
            parts.append("cache.direct=on")
        if opts.get("blk_read_only"):
            parts.append("readonly=on")
        return ",".join(parts)


class CloudHypervisor(VmmBackend):
    name = "ch"
    # Runner output rides on the legacy serial port so the
    # virtio-console device stays available for the console tests to
    # claim without hijacking init's stdout. The virtio-console device
    # is still instantiated via `--console null`. CH wires a 16550 at
    # ttyS0 on x86_64 and a PL011 at ttyAMA0 on aarch64.
    console_device = "ttyAMA0" if platform.machine() == "aarch64" else "ttyS0"
    # CH uses MMIO transport on aarch64, PCI on x86_64
    mmio_transport = (platform.machine() == "aarch64")
    _cid_counter = 2
    _cid_lock = threading.Lock()

    @classmethod
    def _next_cid(cls):
        with cls._cid_lock:
            cls._cid_counter += 1
            return cls._cid_counter

    def build_launch_cmd(self, config: LaunchConfig, *, serial: Optional[str] = None,
                         console: Optional[str] = None, seccomp: Optional[bool] = None,
                         debug: bool = False, nested: bool = False, memory_shared: bool = False,
                         memory_hotplug_size: Optional[str] = None,
                         memory_hotplug_method: Optional[str] = None) -> list[str]:
        command = [self.binary]
        if debug:
            command.append("-vv")
        command.extend(self._boot_args(
            config, kernel="--kernel", initramfs="--initramfs",
            cmdline="--cmdline", firmware="--firmware"))
        cpus = f"boot={config.cpus}" + (",nested=on" if nested else "")
        memory = f"size={config.memory}" + (",shared=on" if memory_shared else "")
        if memory_hotplug_size is not None:
            memory += f",hotplug_size={memory_hotplug_size}"
        if memory_hotplug_method is not None:
            memory += f",hotplug_method={memory_hotplug_method}"
        command.extend(("--cpus", cpus, "--memory", memory))
        if serial is not None:
            command.extend(("--serial", serial))
        if console is not None:
            command.extend(("--console", console))
        if seccomp is not None:
            command.extend(("--seccomp", "true" if seccomp else "false"))
        return command

    @staticmethod
    def _ch_disk_arg(disk_path, disk_fmt, blk_queues, opts):
        """Build the CH --disk value from the generic IO opts.

        Maps --io-engine to CH's _disable_io_uring / _disable_aio
        toggles. CH prefers io_uring, then Linux AIO, then sync, so aio
        disables io_uring and sync disables both. direct=on requests
        O_DIRECT.
        """
        engine = opts.get("io_engine", "default")
        arg = (f"path={disk_path},image_type={disk_fmt}"
               f",num_queues={blk_queues}")
        if opts.get("blk_iommu", True):
            arg += ",iommu=virtio"
        if engine == "aio":
            arg += ",_disable_io_uring=on"
        elif engine == "sync":
            arg += ",_disable_io_uring=on,_disable_aio=on"
        if opts.get("direct"):
            arg += ",direct=on"
        if opts.get("blk_read_only"):
            arg += ",readonly=on"
        return arg

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        opts = opts or {}
        if platform.machine() == "aarch64":
            # DT boot cannot deliver ACPI PCI hotplug notifications.
            arguments = cmdline.split() if cmdline else []
            if not any(argument.startswith("acpi=") for argument in arguments):
                cmdline = f"{cmdline} acpi=on" if cmdline else "acpi=on"
        cpus = opts.get("cpus", 4)
        mem = opts.get("memory", "512M")
        blk_queues = opts.get("blk_queues", 4)
        net_queues = opts.get("net_queues", 4)
        pmem_path = opts.get("pmem_path")
        fs_socket = opts.get("fs_socket")
        api_socket = opts.get("api_socket")
        only = opts.get("fuzz_device")

        def want(dev):
            return only is None or only == dev

        # CH validation constraints (vmm/src/config.rs):
        #   blk:  num_queues       <= boot_vcpus
        #   net:  num_queues / 2   <= boot_vcpus  (rx+tx pairs)
        #   fs:   num_queues       <= boot_vcpus  (fs uses 1)
        # vsock has no such check. Earlier code summed all queues and
        # added +3, inflating cpus far above what CH requires. With
        # parallel jobs this caused severe host oversubscription and
        # guest rcu_sched stalls. Use the real minimum.
        min_cpus = max(blk_queues if want("blk") else 1,
                       (net_queues + 1) // 2 if want("net") else 1)
        cpus = max(cpus, min_cpus)
        cid = self._next_cid()
        vsock_sock = disk_path.rsplit(".", 1)[0] + ".vsock"
        ext = disk_path.rsplit(".", 1)[-1]
        fmt_map = {"raw": "raw", "qcow2": "qcow2", "vhd": "vhd", "vhdx": "vhdx"}
        disk_fmt = fmt_map.get(ext, "raw")
        # Virtio tests disable seccomp to avoid VMM filter failures.
        cmd = self.build_launch_cmd(
            LaunchConfig(cpus=cpus, memory=mem, kernel=kernel,
                         initramfs=initramfs, cmdline=cmdline),
            serial="tty", console="null", memory_shared=bool(fs_socket and want("fs")),
            memory_hotplug_size="256M", memory_hotplug_method="virtio-mem",
            seccomp=False, debug=bool(os.environ.get("VV_CH_DEBUG")))
        if want("blk"):
            cmd += ["--disk", self._ch_disk_arg(
                disk_path, disk_fmt, blk_queues, opts)]
        if want("net"):
            net_rx_tap = opts.get("net_rx_tap", "")
            net_rx_ip = opts.get("net_rx_ip")
            endpoint = f"tap={net_rx_tap}"
            if net_rx_ip:
                endpoint += f",ip={net_rx_ip},mask=255.255.255.0"
            cmd += ["--net", f"{endpoint},num_queues={net_queues}"
                    f"{',iommu=virtio' if opts.get('net_iommu') else ''}"]
        if want("vsock") and opts.get("vsock_enabled", True):
            cmd += ["--vsock", f"cid={cid},socket={vsock_sock}"]
        if want("balloon"):
            cmd += ["--balloon", "size=0"]
        if want("watchdog"):
            cmd += ["--watchdog"]
        if want("rng"):
            cmd += ["--rng", "src=/dev/urandom"]
        if pmem_path and want("pmem"):
            cmd += ["--pmem", f"file={pmem_path},size=128M,discard_writes=on"]
        if fs_socket and want("fs"):
            cmd += ["--fs", f"tag=vvfs,socket={fs_socket},num_queues=1,queue_size=128"]
        if api_socket:
            cmd += ["--api-socket", f"path={api_socket}"]
        return cmd


class Qemu(VmmBackend):
    name = "qemu"
    console_device = "ttyAMA0" if platform.machine() == "aarch64" else "ttyS0"
    supports_trace = True
    _cid_counter = 2  # start above reserved CIDs (0=hypervisor, 1=local, 2=host)
    _cid_lock = threading.Lock()

    def trace_args(self, opts):
        return self._qemu_trace_flags(opts)

    @classmethod
    def _next_cid(cls):
        with cls._cid_lock:
            cls._cid_counter += 1
            return cls._cid_counter

    def build_launch_cmd(self, config: LaunchConfig, *, cpu_model: Optional[str] = None,
                         kvm: bool = False, accel: Optional[str] = None,
                         nographic: bool = False, no_reboot: bool = False,
                         memory_max: Optional[str] = None,
                         memory_slots: Optional[int] = None) -> list[str]:
        machine = "virt" if platform.machine() == "aarch64" else None
        return self._qemu_launch_cmd(
            config, machine=machine, cpu_model=cpu_model, kvm=kvm, accel=accel,
            nographic=nographic, no_reboot=no_reboot,
            memory_max=memory_max, memory_slots=memory_slots)

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        opts = opts or {}
        arch = platform.machine()
        accel = opts.get("accel", "kvm")
        cpus = opts.get("cpus", 1)
        mem = opts.get("memory", "512M")
        blk_queues = opts.get("blk_queues", 4)
        packed = opts.get("packed", True)
        pmem_path = opts.get("pmem_path")
        fs_socket = opts.get("fs_socket")
        p9_sharedir = opts.get("p9_sharedir")
        api_socket = opts.get("api_socket")
        # When fuzzing one device, attach only that device so the guest
        # boots faster. None (the conformance runs) attaches the full set.
        only = opts.get("fuzz_device")

        def want(dev):
            return only is None or only == dev

        cid = self._next_cid()
        ext = disk_path.rsplit(".", 1)[-1]
        fmt_map = {"raw": "raw", "qcow2": "qcow2", "vhd": "vpc", "vhdx": "vhdx"}
        qemu_fmt = fmt_map.get(ext, "raw")
        packed_flag = "on" if packed else "off"
        blk_dev = (f"virtio-blk-pci-non-transitional,drive=vd0"
                   f",packed={packed_flag},num-queues={blk_queues}")
        # Multiqueue net requires tap netdev which needs /dev/net/tun
        # (CAP_NET_ADMIN). Since QEMU is typically a distro binary that
        # cannot be given extra capabilities, we use the unprivileged
        # user netdev which only supports a single queue.
        net_dev = (f"virtio-net-pci-non-transitional,netdev=net0"
                   f",packed={packed_flag},romfile=")
        net_rx_port = opts.get("net_rx_port")
        net_rx_source_port = opts.get("net_rx_source_port")
        if net_rx_port:
            netdev = (f"socket,id=net0,udp=127.0.0.1:{net_rx_source_port},"
                      f"localaddr=127.0.0.1:{net_rx_port}")
        else:
            netdev = "user,id=net0"
        cmd = self.build_launch_cmd(
            LaunchConfig(cpus=cpus, memory=mem, kernel=kernel,
                         initramfs=initramfs, cmdline=cmdline),
            cpu_model=(opts.get("cpu_model") or None) if arch != "aarch64" else None,
            accel=accel, nographic=True, no_reboot=True,
            memory_max=_bump_maxmem(mem), memory_slots=2)
        if want("mem") and opts.get("mem_region_size"):
            cmd += [
                "-object", f"memory-backend-ram,id=mem0,size={opts['mem_region_size']}",
                "-device", "virtio-mem-pci,memdev=mem0,requested-size=0,"
                "block-size=2M,unplugged-inaccessible=off",
            ]
        if want("blk"):
            cmd += ["-drive", self._qemu_drive_arg(disk_path, qemu_fmt, opts),
                    "-device", blk_dev]
        if want("net"):
            cmd += ["-netdev", netdev, "-device", net_dev]
        if want("vsock") and opts.get("vsock_enabled", True):
            cmd += ["-device",
                    f"vhost-vsock-pci,guest-cid={cid},packed={packed_flag}"]
        if want("balloon"):
            cmd += ["-device",
                    f"virtio-balloon-pci-non-transitional,packed={packed_flag}"]
        if want("iommu"):
            cmd += ["-device", "virtio-iommu-pci"]
        if want("console"):
            cmd += ["-device",
                    f"virtio-serial-pci-non-transitional,packed={packed_flag}"]
        if want("rng"):
            cmd += ["-object", "rng-random,id=rng0,filename=/dev/urandom",
                    "-device",
                    f"virtio-rng-pci-non-transitional,rng=rng0"
                    f",packed={packed_flag}"]
        if want("rtc"):
            cmd += ["-device", f"virtio-rtc-pci,packed={packed_flag}"]
        if want("sound"):
            cmd += ["-device", "virtio-sound-pci,audiodev=snd0",
                    "-audiodev", "none,id=snd0"]
        if not want("net"):
            # Suppress QEMU's default NIC so a non-net focus does not pay
            # to emulate an unused virtio-net and its option ROM.
            cmd += ["-net", "none"]
        if arch != "aarch64":
            cmd += ["-device", "isa-debug-exit,iobase=0x501,iosize=1"]
        if want("pmem") and pmem_path and arch == "x86_64":
            cmd += [
                "-object", f"memory-backend-file,id=pmem0,mem-path={pmem_path},size=128M",
                "-device", f"virtio-pmem-pci,memdev=pmem0,packed={packed_flag}",
            ]
        if fs_socket and want("fs"):
            cmd += [
                "-chardev", f"socket,id=fs0,path={fs_socket}",
                "-device", f"vhost-user-fs-pci,chardev=fs0,tag=vvfs,packed={packed_flag}",
                "-object", f"memory-backend-file,id=mem-fs,size={mem},mem-path=/dev/shm,share=on",
                "-numa", "node,memdev=mem-fs",
            ]
        if p9_sharedir and want("p9"):
            cmd += [
                "-fsdev",
                f"local,id=fsdev9p,path={p9_sharedir},security_model=none",
                "-device",
                f"virtio-9p-pci,disable-legacy=on,packed={packed_flag}"
                ",fsdev=fsdev9p,mount_tag=vv9p",
            ]
        # virtio-crypto with a builtin software cryptodev backend. CH has
        # no crypto device model, so crypto tests only run under QEMU.
        if want("crypto") and not opts.get("vm_snapshot"):
            cmd += [
                "-object", "cryptodev-backend-builtin,id=cryptodev0",
                "-device",
                f"virtio-crypto-pci,id=crypto0,cryptodev=cryptodev0,packed={packed_flag}",
            ]
        # virtio-scsi with backing scsi-hd LUNs. CH has no virtio-scsi
        # device model, so scsi tests only run under QEMU. A real LUN is
        # needed so a SCSI READ reaches the block backend. A second LUN
        # with a 4K logical block exercises multi LUN addressing and non
        # 512 geometry, and multiple request queues are advertised.
        if opts.get("scsi_path") and want("scsi"):
            cmd += self._scsi_args(
                opts, f"virtio-scsi-pci-non-transitional,id=scsi0"
                f",packed={packed_flag},num_queues=4")
        if api_socket:
            cmd += ["-qmp", f"unix:{api_socket},server=on,wait=off"]
        cmd[1:1] = self.trace_args(opts)
        return cmd

    @staticmethod
    def _scsi_args(opts, controller):
        return [
            "-drive", f"file={opts['scsi_path']},if=none,id=scsidisk,format=raw",
            "-drive", f"file={opts['scsi_path2']},if=none,id=scsidisk2,format=raw",
            "-device", controller,
            "-device",
            "scsi-hd,drive=scsidisk,bus=scsi0.0,channel=0"
            ",scsi-id=0,lun=0,serial=vvscsi0",
            "-device",
            "scsi-hd,drive=scsidisk2,bus=scsi0.0,channel=0"
            ",scsi-id=0,lun=1,logical_block_size=4096"
            ",physical_block_size=4096,serial=vvscsi1",
        ]


class QemuMicrovm(VmmBackend):
    """QEMU with virtio-mmio transport (microvm on x86_64, virt on aarch64)."""
    name = "qemu-mmio"
    console_device = "ttyS0"
    mmio_transport = True
    supports_trace = True

    def trace_args(self, opts):
        return self._qemu_trace_flags(opts)

    def build_launch_cmd(self, config: LaunchConfig, *, kvm: bool = False,
                         accel: Optional[str] = None,
                         nographic: bool = False, no_reboot: bool = False) -> list[str]:
        machine = "virt" if platform.machine() == "aarch64" else "microvm,rtc=off"
        return self._qemu_launch_cmd(
            config, machine=machine, modern_mmio=True, kvm=kvm, accel=accel,
            nographic=nographic, no_reboot=no_reboot)

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        opts = opts or {}
        mem = opts.get("memory", "512M")
        ext = disk_path.rsplit(".", 1)[-1]
        fmt_map = {"raw": "raw", "qcow2": "qcow2", "vhd": "vpc", "vhdx": "vhdx"}
        qemu_fmt = fmt_map.get(ext, "raw")
        arch = platform.machine()
        accel = opts.get("accel", "kvm")
        if arch == "aarch64":
            self.console_device = "ttyAMA0"
        cmd = self.build_launch_cmd(
            LaunchConfig(cpus=1, memory=mem, kernel=kernel,
                         initramfs=initramfs, cmdline=cmdline),
            accel=accel, nographic=True, no_reboot=True)
        cmd += ["-drive", self._qemu_drive_arg(disk_path, qemu_fmt, opts)]
        device = opts.get("mmio_device", "blk")
        if device == "blk":
            cmd += ["-device", "virtio-blk-device,drive=vd0"]
        elif device == "rng":
            cmd += ["-object", "rng-random,id=rng0,filename=/dev/urandom",
                    "-device", "virtio-rng-device,rng=rng0"]
        elif device in ("balloon", "rtc"):
            cmd += ["-device", f"virtio-{device}-device"]
        elif device == "vsock":
            cmd += ["-device",
                    f"vhost-vsock-device,guest-cid={Qemu._next_cid()}"]
        elif device == "console":
            cmd += ["-device", "virtio-serial-device,id=console0",
                    "-chardev", "null,id=console-output",
                    "-device",
                    "virtconsole,bus=console0.0,chardev=console-output"]
        elif device == "scsi":
            cmd += Qemu._scsi_args(
                opts, "virtio-scsi-device,id=scsi0,num_queues=4")
        else:
            raise ValueError(f"unsupported MMIO device: {device}")
        if arch != "aarch64":
            cmd += ["-device", "isa-debug-exit,iobase=0x501,iosize=1"]
        if opts.get("api_socket"):
            cmd += ["-qmp", f"unix:{opts['api_socket']},server=on,wait=off"]
        cmd[1:1] = self.trace_args(opts)
        return cmd


class OpenVmm(VmmBackend):
    """Microsoft OpenVMM, booted via Linux direct boot.

    OpenVMM direct boot has no legacy PCI bus, so every virtio device is
    placed on a named port of an emulated native PCIe root complex. A
    device left off a PCIe port would fall back to the legacy PCI bus and
    fail VM startup, so all devices below carry an explicit pcie_port.

    Runner output rides on COM1 (a legacy 16550 at ttyS0 on x86, a PL011
    at ttyAMA0 on aarch64) via `--com1 console`, leaving the virtio-console
    device free for the console tests to claim without hijacking the
    runner stream.

    Virtio devices use named PCIe ports so direct boot does not depend on
    the legacy PCI interrupt routing provided by PCAT firmware. OpenVMM has
    no virtio-balloon, virtio-mem, or virtio-watchdog device, so those
    families are unavailable here.
    """
    name = "openvmm"
    console_device = "ttyAMA0" if platform.machine() == "aarch64" else "ttyS0"
    mmio_transport = False

    _ROOT_COMPLEX = "rc0"

    def build_launch_cmd(self, config: LaunchConfig, *, hypervisor: Optional[str] = None,
                         hyperv: bool = False, single_process: bool = False,
                         serial: Optional[str] = None) -> list[str]:
        command = [self.binary]
        if hypervisor is not None:
            command.extend(("--hypervisor", hypervisor))
        if hyperv:
            command.append("--hv")
        if single_process:
            command.append("--single-process")
        command.extend(self._boot_args(config, kernel="-k", initramfs="-r", cmdline="-c"))
        command.extend(("-m", config.memory, "-p", str(config.cpus)))
        if serial is not None:
            command.extend(("--com1", serial))
        return command

    @staticmethod
    def _blk_arg(disk_path, port, opts, read_only=False):
        """Build a --virtio-blk value for a disk on the given PCIe port.

        Maps --direct to the OpenVMM `;direct` file modifier.
        """
        engine = opts.get("io_engine", "default")
        if engine not in ("default", "io_uring"):
            raise ValueError(
                f"OpenVMM does not support selecting the {engine} IO engine")
        direct = bool(opts.get("direct"))
        spec = f"file:{disk_path}"
        if direct:
            spec += ";direct"
        parts = [spec]
        if read_only:
            parts.append("ro")
        parts.append(f"pcie_port={port}")
        return ",".join(parts)

    def build_cmd(self, kernel, initramfs, disk_path, cmdline, opts=None):
        opts = opts or {}
        cpus = max(opts.get("cpus", 4), 1)
        mem = opts.get("memory", "512M")
        pmem_path = opts.get("pmem_path")
        fs_socket = opts.get("fs_socket")
        p9_sharedir = opts.get("p9_sharedir")
        vsock_path = disk_path.rsplit(".", 1)[0] + ".vsock"
        # When fuzzing one device, attach only that device so the guest
        # boots faster. None (the conformance runs) attaches the full set.
        only = opts.get("fuzz_device")
        exit_on_end = bool(opts.get("exit_on_guest_end"))

        def want(dev):
            if opts.get("vm_snapshot") and dev in ("console", "vsock"):
                return False
            return only is None or only == dev

        rc = self._ROOT_COMPLEX
        # Declare a PCIe root port per virtio device that is present.
        ports = []
        if want("blk"):
            ports.append("disk")
        if want("net"):
            ports.append("net")
        if want("console"):
            ports.append("console")
        if want("rng"):
            ports.append("rng")
        if want("vsock"):
            ports.append("vsock")
        if want("pmem") and pmem_path:
            ports.append("pmem")
        if want("fs") and fs_socket:
            ports.append("fs")
        if want("p9") and p9_sharedir:
            ports.append("p9")

        # KVM/aarch64 does not provide SynIC, so enabling HV#1 would make
        # OpenVMM create a VMBus instance that cannot start.
        cmd = self.build_launch_cmd(
            LaunchConfig(cpus=cpus, memory=mem, kernel=kernel,
                         initramfs=initramfs, cmdline=cmdline),
            hypervisor="kvm", hyperv=platform.machine() != "aarch64",
            single_process=True, serial="console")
        cmd += ["--pcie-root-complex", rc]
        for port in ports:
            cmd += ["--pcie-root-port", f"{rc}:{port}"]

        if want("blk"):
            cmd += ["--virtio-blk", self._blk_arg(
                disk_path, "disk", opts,
                read_only=opts.get("blk_read_only", False))]
        if want("net"):
            net_rx_port = opts.get("net_rx_port")
            endpoint = "pcie_port=net:consomme"
            if net_rx_port:
                endpoint += (f":hostfwd=udp:127.0.0.1:{net_rx_port}"
                             f"-:{net_rx_port}")
            cmd += ["--virtio-net", endpoint]
        if want("console"):
            # Present but idle, sunk to /dev/null so the console tests can
            # claim it while the runner keeps COM1.
            cmd += ["--virtio-console", "file=/dev/null",
                    "--virtio-console-pcie-port", "console"]
        if want("rng"):
            cmd += ["--virtio-rng", "--virtio-rng-pcie-port", "rng"]
        if want("vsock"):
            cmd += ["--virtio-vsock-path", vsock_path,
                    "--virtio-vsock-bus", "pcie"]
        if want("pmem") and pmem_path:
            cmd += ["--virtio-pmem", f"pcie_port=pmem:{pmem_path}"]
        if want("fs") and fs_socket:
            cmd += ["--vhost-user",
                    f"{fs_socket},type=fs,tag=vvfs,pcie_port=fs"]
        if want("p9") and p9_sharedir:
            cmd += ["--virtio-9p",
                    f"pcie_port=p9:vv9p,{p9_sharedir}"]
        if exit_on_end:
            # Fuzzing: the guest ends every run by powering off or
            # rebooting. The defaults halt or reset the VM, which would
            # hang the VMM until the timeout; make it exit promptly.
            cmd += ["--guest-shutdown-action", "exit",
                    "--guest-reset-action", "exit"]
        return cmd
        return cmd


_VMM_BACKENDS = [CloudHypervisor, Qemu, QemuMicrovm, OpenVmm]


def detect_vmm(vmm_path):
    """Detect VMM backend from binary name. Returns a VmmBackend instance."""
    base = os.path.basename(vmm_path)
    if "cloud-hypervisor" in base or base == "ch":
        return CloudHypervisor(vmm_path)
    if base.startswith("qemu-system"):
        return Qemu(vmm_path)
    if "openvmm" in base:
        return OpenVmm(vmm_path)
    names = ", ".join(cls.name for cls in _VMM_BACKENDS)
    sys.exit(f"Cannot detect VMM type from '{base}'. "
             f"Supported: {names}")


def fetch_kernel(mmio=False):
    """Select a local guest image or acquire a verified cached release."""
    arch = platform.machine()
    name = KERNEL_NAMES.get(arch)
    if mmio and arch == "x86_64":
        name = "bzImage-x86_64"
    if not name:
        raise RuntimeError(f"No bundled guest kernel available for {arch}. Use --kernel.")
    target = Path(_SCRIPT_DIR) / "target"
    local = target / name
    if local.is_file():
        print(f"Using existing guest kernel {local} without a recorded release checksum.",
              file=sys.stderr)
        return str(local)
    try:
        return str(artifacts.github_release_asset(
            KERNEL_REPOSITORY, name, target / "kernel-cache", prefer_cached=True))
    except (OSError, RuntimeError) as error:
        raise RuntimeError(f"Cannot acquire guest kernel {name}: {error}") from error
