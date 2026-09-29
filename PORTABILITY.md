# AArch64 development status

This branch repairs CPU-side portability defects in the experimental VC4
implementation. It is **not a conformant Vulkan implementation** and is not
yet demonstrated to render on a Raspberry Pi Zero 2 W running a 64-bit OS.
It does not currently meet yserver's Vulkan requirements.

## Building with the distribution's Vulkan loader

Requires CMake 3.18+, a C/C++ compiler, Python, Vulkan headers/loader, libdrm,
expat, zlib and POSIX threads. Use a native Linux build environment.

```sh
cmake -S . -B build -DUSE_SYSTEM_VULKAN=ON -DBUILD_TESTING=ON
cmake --build build --parallel --target rpi-vk-driver cpu-regressions no-vc4 triangle clear
ctest --test-dir build -R 'cpu-regressions|no-vc4' --output-on-failure
```

System mode neither downloads nor installs a replacement Vulkan loader.
The upstream bundled-loader build remains available with the option OFF;
that older dependency path was not revalidated in this phase.
Do not run the complete CTest suite in QEMU: its other tests require VC4 and
may modeset the display. `no-vc4` skips if `/dev/dri/card0` exists.

CPU sanitizers, when supplied by the compiler installation:

```sh
cmake -S . -B build-asan -DUSE_SYSTEM_VULKAN=ON -DBUILD_TESTING=OFF -DCPU_TEST_SANITIZERS=ON
cmake --build build-asan --target cpu-regressions
ctest --test-dir build-asan -R '^cpu-regressions$' --output-on-failure
```

## Changes

- Pool links retain native pointers; unaligned strides use memcpy. Consecutive
  allocation maintains ordered free runs and correct reallocation accounting,
  handles shrinking and unchanged sizes, and retains old data on failure.
- Map keys retain pointer-width profiler addresses. Hashing no longer shifts
  by the table capacity; full-table searches terminate and deleting a colliding
  key preserves lookup of subsequent keys.
- The private assembly shader transport now preserves 64-bit pointers.
  The driver, internal shaders and demos use the same encoding helper.
  **Rebuild AArch64 clients**; the previous truncated-pointer encoding cannot
  work. The six-word 32-bit encoding is preserved. This is not a SPIR-V compiler.
- DRM driver identification replaces the unsafe `/proc/cpuinfo` parser.
  Absent/non-VC4 devices fail before VC4-specific ioctls; descriptor references
  are locked, descriptor zero is valid, and failures clean up allocations.
  Device selection is still limited to `/dev/dri/card0`.
- Modern CMake/GCC fixes include declarations, kernel ioctl pointer conversion,
  opaque handles and correctly selecting nested fields in Vulkan `*2` structs.
  Image-copy regions are translated to blit regions instead of reinterpreting
  incompatible struct layouts; rendering correctness still needs hardware tests.
- Unsupported API versions and ordinary SPIR-V shaders fail explicitly.
  The compute-pipeline stub returns an error and null handles instead of success.

## Evidence (2026-09-29)

- AArch64 Bootstrapo chroot in QEMU: shared driver and triangle/clear demos build
  with GCC 16.2.0 and installed Vulkan 1.4.360 headers/loader. No `-fcommon`,
  `-fpermissive` or diagnostic suppression is needed for this build. Existing
  unrelated warnings remain; this is not a warning-free whole-driver build.
- CPU regressions pass: 20,000 randomized allocation/reallocation operations,
  data preservation, pool exhaustion/coalescing, unaligned blocks, full maps,
  collision deletion, and native-pointer assembly encoding.
- Same CPU tests pass on x86-64 macOS under Rosetta, built with Clang.
- AArch64 Linux GCC 13 AddressSanitizer/UBSan CPU tests pass outside the
  Bootstrapo chroot. Its GCC package lacks sanitizer runtimes, so the sanitized
  test uses the VM host compiler and pinned Vulkan headers instead.
- No-VC4 test passes in QEMU: repeated failed instance creations free all
  callback allocations, output handles are null, unsupported API/shaders fail.
- No physical GPU rendering or Vulkan CTS result is claimed.

## Work towards hardware rendering and conformance

1. On physical Zero 2 W: verify the VC4 kernel driver and firmware, identify
   the GPU, render the private-assembly clear/triangle demos and verify output,
   readback and repeated teardown. Record the exact kernel, firmware and source.
2. Implement a real SPIR-V-to-QPU compiler path. Evaluate reuse of Mesa's VC4
   compiler infrastructure; binding its shader outputs, uniforms, descriptors,
   coordinate shaders and relocations into this driver requires engineering,
   not just linking a library. Prove ordinary compiled vertex/fragment shaders
   with pixel comparisons before expanding coverage.
3. Audit every advertised feature, limit, format and entry point against a
   specific Vulkan version. Transfer operations and compute are incomplete;
   success-returning stubs are not implementation. Determine hardware limits
   and required emulation before committing to any full-conformance target.
4. Establish Vulkan CTS/validation baselines on the physical GPU and fix the
   failures by feature area. Keep CPU-only regression testing in QEMU.
5. yserver additionally needs dynamic rendering, synchronization2, normal
   SPIR-V pipelines, external-memory/semaphore FDs, and compatible KMS scanout.
   Build and test those paths; increasing the advertised API version is not
   a substitute. Passing an assembly demo does not establish yserver support.

Khronos conformance involves CTS and the conformance submission process:
https://github.khronos.org/Vulkan-Site/guide/latest/vulkan_cts.html
The possibility, performance and scope of GPU-backed conformance on VC4 remain
unproven. No CPU fallback is silently substituted for hardware testing.
