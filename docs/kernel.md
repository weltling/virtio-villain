# Optional Linux builds

The builder supports the latest stable Linux release, pinned releases,
local checkouts, Git repositories, HTTP archives and preloaded tarballs.
It builds x86_64 and aarch64 images. Source selection and kernel
configuration are independent.

Run [build-kernel](../build-kernel) from the repository root. Use
`./build-kernel --help` to list its options. The `guest-kernel` Make
target forwards `KERNEL_ARGS` to the same command.

Normal appliance builds do not build a kernel. The virtio runner
continues to download its existing default kernel. Select a custom image
explicitly with `--kernel`.

## Prerequisites

Install the dependencies required by the selected Linux tree. Typical
dependencies are GCC, make, binutils, bc, bison, flex, libelf headers
and OpenSSL headers. Python 3 drives the build. Archive extraction
requires a Python build with `tarfile.data_filter` support. Git sources
need Git. ARM builds on x86_64 need an aarch64 cross compiler.

Only build trusted sources. Kernel makefiles execute commands on the
build machine. No elevated permissions are needed for the builder.

Set `RUSTC` in the environment to select the Rust compiler for configuration
and compilation. When unset, Linux uses its normal compiler default.

## Latest stable and pinned releases

With no source selector, the builder follows the `default` policy in
[manifest.json](../kernel/manifest.json). It currently resolves the
kernel.org `stable` release on every invocation. It does not select the
separate mainline entry or a release candidate. The exact version,
source URL, archive SHA256 and release timestamp are recorded with the
build.

```sh
make guest-kernel KERNEL_ARGS="--arch x86_64 --jobs 4"
./build-kernel --latest-stable --arch x86_64 --jobs 4
```

The bundled manifest also retains an exact pin. Select it explicitly for
a repeatable source selection without querying release metadata.

```sh
./build-kernel --pinned --arch x86_64 --jobs 4
./build-kernel --version 7.2.12 --arch x86_64 --jobs 4
```

`--version` selects an exact kernel.org release. Its checksum is
retrieved from the official checksum listing unless supplied with
`--sha256` or covered by the bundled pin. `--pinned` uses the version,
checksum and timestamp from the manifest.

Official release builds use the bundled [common
config](../kernel/common.config) and architecture config unless
`--config` is supplied. The bundled configs enable builtin virtio and
VFIO support for the existing appliance. A new kernel release may change
Kconfig dependencies. If requested values cannot be retained, the
builder fails rather than silently dropping them.

### Custom configs through make

`make guest-kernel` accepts `KERNEL_CONFIGS` as a space separated list of
config files and fragments. Later files override earlier values. Leaving
the list empty preserves the bundled profile unless `KERNEL_ARGS` supplies
explicit configs.

```sh
make guest-kernel \
  KERNEL_CONFIGS="/path/base.config /path/extra.config" \
  KERNEL_ARGS="--source /path/linux --output /path/kernel-build --offline"
```

`KERNEL_ARGS` supplies additional builder options after `KERNEL_CONFIGS`.
Any `--config` options in those arguments are applied last. Custom configs
require an explicit output directory. Use a new directory when changing
the source, configuration or toolchain.

Use `make help` for Make variables and `./build-kernel --help` for all
source and build options.

## Preloaded archives and offline pipelines

A pipeline can preload the source archive and configuration itself. Pass
their paths directly.

```sh
./build-kernel \
  --offline \
  --archive /inputs/linux.tar.xz \
  --config /inputs/kernel.config \
  --sha256 "$EXPECTED_SHA256" \
  --cache-dir /work/kernel-cache \
  --output /work/kernel-build \
  --arch x86_64 --jobs 4
```

The builder reads the supplied files without modifying them. It verifies
the expected checksum before extraction. If `--sha256` is omitted, it
still records the computed archive digest but does not claim independent
checksum verification.

Archives can be uncompressed tar files or compressed with gzip, bzip2 or
xz. They can contain a single top level source directory or the source
files directly. The source root must contain a Linux Makefile.
Extraction rejects paths and links that escape the temporary extraction
directory.

`--offline` prevents builder managed release lookups, Git fetches and
archive downloads. It never falls back to a different source. Missing
archives, configs and checkouts fail explicitly.

Supported offline inputs are a local checkout, a local archive or an
already cached HTTP archive or exact kernel.org release. `--pinned
--offline` checks for `linux-VERSION.tar.xz` in the cache and verifies
it against the manifest. `--version VERSION --sha256 DIGEST --offline`
uses the same cache naming. If the requested version matches the bundled
pin, the checksum argument can be omitted.

Latest stable resolution is unavailable offline because the builder
cannot establish what is currently latest. Git source selection also
fails offline. Use `--source` with a prepared local checkout instead.

Offline mode is not a network sandbox for kernel makefiles, compilers or
other build tools. The pipeline must enforce its no network policy
externally. Any required toolchains, certificates, firmware and other
build dependencies must already be available.

## Local checkout

```sh
./build-kernel \
  --offline \
  --source /path/to/linux \
  --config /path/to/kernel.config \
  --output /path/to/build \
  --arch x86_64 --jobs 4
```

The tree must be prepared for an out of tree Linux build. The builder
does not clean, check out or patch it. Do not modify it while building.
Use a new output directory when changing its revision or toolchain.

## Git repository

```sh
./build-kernel \
  --git-url \
    https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
  --ref v7.2.12 \
  --config /path/to/kernel.config \
  --output /path/to/build \
  --arch x86_64 --jobs 4
```

Branches, tags and commit IDs are accepted. Each invocation fetches the
requested ref and records the resolved commit. Use a commit ID for
repeatable source selection. A moving branch requires a new output
directory when it resolves to a different commit. Failed fetches do not
fall back to cached revisions.

Cached checkouts must remain unmodified. Git URLs can also identify
local repositories. Authentication uses the normal Git configuration. Do
not embed credentials in URLs. The builder does not initialize
submodules. Use a prepared local checkout if the kernel requires them.

## HTTP or HTTPS archive

```sh
./build-kernel \
  --url https://example.org/linux-custom.tar.gz \
  --sha256 "$EXPECTED_SHA256" \
  --config /path/to/kernel.config \
  --output /path/to/build \
  --arch x86_64 --jobs 4
```

Downloads are cached by URL and reused if present. Use an immutable URL,
an expected checksum or a fresh cache when the remote content changes. A
checksum mismatch fails without replacing the cached artifact.
Interrupted downloads are not installed in the cache.

Without `--sha256`, the builder warns that it is only recording a
computed digest. Build metadata distinguishes an expected checksum
verification from a recorded digest. Official release checksums are
retrieved over HTTPS. The builder does not verify PGP signatures.

## Configuration

Local checkouts, Git sources and custom archives require at least one
`--config` and an explicit `--output`. Supply a full Linux config or a
fragment. Repeat `--config` to layer fragments in order. The last
explicit value for each option wins. Supplied configs replace the
bundled config rather than inheriting it. This also applies to configs
supplied with latest stable or pinned release selection.

```sh
./build-kernel \
  --source /path/to/linux \
  --config /path/to/base.config \
  --config /path/to/overrides.config \
  --output /path/to/arm-build \
  --arch aarch64 --cross-compile aarch64-linux-gnu- --jobs 4
```

The builder uses `allnoconfig` with `KCONFIG_ALLCONFIG`. Kconfig
resolves dependencies and disables unspecified options where possible.
Every explicitly requested value is checked against the effective config
before compilation. If an enabled option is unavailable or dependencies
change its value, the build fails with the requested option in the
diagnostic. An absent disabled option is treated as disabled.

Use `--base-config PATH` for a full config from another source revision or
toolchain. Kconfig adapts its values and reports each adjustment with the
requested and effective values. This base replaces the bundled configs.
Additional `--config` files override the base and remain strictly checked.
Use them to require essential boot options or features. A base config also
requires an explicit `--output`.

```sh
./build-kernel \
  --source /path/to/linux \
  --base-config /path/to/vendor.config \
  --config /path/to/required.config \
  --output /path/to/vendor-build
```

The supplied config controls boot prerequisites such as the initramfs
decoder, console and device drivers. The builder does not automatically
add them. It builds kernel images only. It does not build or package
loadable modules, firmware, device trees or bootloaders.

## Outputs and cache

The default cache is `target/guest-kernel/`. Override it with
`--cache-dir`. Preloaded input archives can remain on a read only mount.
The cache and output directories must be writable.

Extracted archive trees are keyed by archive content digest. A changed
tarball cannot reuse an older extracted tree. Do not modify cached
source trees. Outputs created by the earlier builder with a different
source cache layout require a new `--output` directory.

Official release outputs default to `VERSION-REVISION/` under the cache.
An explicit `--output` overrides that location. Custom sources and
custom configs require `--output`.

Each output directory contains architecture specific build trees and an
`images/` directory. The x86 images are `vmlinux-x86_64` and
`bzImage-x86_64`. The ARM image is `Image-arm64`. Each build also
exports the effective config, build metadata and SHA256 checksums.

Use `--result PATH` to write a JSON build result after a successful build.
It reports the selected architecture, produced image paths and their SHA256
digests. AMD64 uses the architecture identifier `x86_64`. AArch64 uses
`aarch64`. Consumers can use the reported boot image instead of predicting
an architecture specific filename. Failed builds do not replace the result.

Source preparation and architecture builds use file locks. Changed
source paths, config contents or cross compiler prefixes require a
separate output directory. Local checkouts record their source path but
may contain uncommitted changes and are not claimed to be reproducible.
Archive builds record the content digest. Git builds record the
repository, ref and resolved commit.

Build timestamps default to the Git commit timestamp or the kernel.org
release timestamp when available. Local checkouts, arbitrary archives
and explicit versions without a known release timestamp use epoch zero.
Use `--source-date-epoch SECONDS` to override it. Timestamps alone do
not guarantee identical binaries across different toolchains.

```sh
make initramfs
./run -m /path/to/cloud-hypervisor \
  --kernel /path/to/build/images/vmlinux-x86_64 PCI0127
./run -m /path/to/qemu-system-x86_64 \
  --kernel /path/to/build/images/bzImage-x86_64 PCI0127
```

A successful build does not prove the image boots or supports a
particular test. Boot validation must match the selected kernel
configuration and execution environment.

This local build interface adds no CI publishing workflow. When
distributing custom kernels, retain and provide the corresponding
source, configuration and build scripts as required by their licenses.
