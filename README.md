# ReactOS VirtIO Sound Driver

Standalone WDM/PortCls driver for ReactOS and NT 5.x-style systems.
This repository contains the driver, INF and development scripts but depends on reactos sources to compile.

## Motivation

I wanted to use the virtio-snd driver from redhat's virtio-win project and learned that it targets the WaveRT interface.
Reactos only implements the earliert PortCl interface. Likewise, virtio-win has dropped support for Windows Server 2003/Vista which are roughly contemporaries of ReactOS.

## Potentially outdated?


### Reactos

https://github.com/reactos/reactos/pull/8662 - Somebody has built WaveRT Streaming support, potentially reducing the need for this driver.

### Real legacy windowses

The virtio-win project does not target real legacy windowses with their current drivers and the older driver versions don't include virtio-snd which was not available as a standard back then. This driver may or may not do what you want on legacy Windows. The AC97 emulation is a battle tested basic audio driver. I'm glad if somebody tests my driver on legacy Windows and I will react to feedback but I have no strong desire to support that.

You know the vendor says you should not use legacy windows ;)

## Status

See [STATUS.md]

## Layout

- `src/driver.c`, `src/adapter.c`: WDM/PortCls entry and adapter startup.
- `src/wavecyclic.cpp`: WaveCyclic render stream and bridge pin.
- `src/topology.cpp`: Render topology with master volume and mute controls.
- `src/audio_controls.c`: Software master volume/mute state and PCM scaling.
- `include/virtio_snd_protocol.h`: Initial VirtIO Sound identifiers/layouts.
- `virtiosnd.inf`: Prototype PCI driver-install manifest.
- `scripts/run-reactos-virtio-snd.ps1`: QEMU launcher using a disposable disk
  overlay. Avoid polluting the UUT system.
- `CMakeLists.txt`: ReactOS driver-target fragment. It expects ReactOS's
  configured CMake context and includes its `virtio` target and driver macros.

## Dependencies and build

Expected sibling paths:

- ReactOS sources: `..\reactos`
- This repository: `..\reactos-virtio-snd-drv`
- TCP Console host to guest utility: `..\tcpconsole` (useful for testing)

The driver links the SDK's `virtio` static target.
The injector adds this repository as an external subdirectory during
configuration and defers target registration until ReactOS's driver macros
and VirtIO target are defined. 

Configure and build the x86 driver with RosBE:

```powershell
.\scripts\build-reactos.ps1
```

The script defaults to the sibling ReactOS checkout and
`build\reactos-i386`. Use `-ReactOSSourceDir`, `-BuildDir`, `-RosBEDir`, and
`-Architecture` to override those defaults. `-ConfigureOnly` skips the target
build. The first configure generates ReactOS build files and may take a while;
subsequent invocations reuse them.

The first target is i386, matching the available ReactOS 0.4.16 VM. The
development checkout is on `master`. Testing so far has been limited to the 0.4.16 release and the master branch.

## VM workflow

The development launcher creates `reactos-dev.qcow2` as a copy-on-write overlay over a base VM, then exposes QEMU's `virtio-sound-pci`.

```powershell
.\scripts\run-reactos-virtio-snd.ps1
```

Optionally pass an ISO containing the built `.sys` and INF:

```powershell
.\scripts\package-driver.ps1 -OutputPath .\build\virtiosnd-transport.iso
.\scripts\run-reactos-virtio-snd.ps1 -DriverIsoPath .\build\virtiosnd-transport.iso
```

`package-driver.ps1` defaults to the build made against the ReactOS 0.4.16
source tag and creates a data ISO using Windows IMAPI2. Override `-DriverPath`
to package a binary built against another ReactOS checkout.

`tests\waveout_control_capture.c` plays 440 Hz near-max, 660 Hz attenuated,
and 880 Hz muted segments. Run it in a WAV-capture VM and verify the recorded
levels with `tests\verify_control_capture.ps1 -WavPath <capture.wav>`.
Build the guest test from PowerShell with:

```powershell
& 'C:\RosBE\RosBE.ps1' i386
& 'C:\RosBE\i386\bin\gcc.exe' -Wall -Wextra -Werror `
    -o .\build\waveout_control_capture.exe `
    .\tests\waveout_control_capture.c -lwinmm
```

`tests\waveout_rate_smoke.c` defaults to ten individual second long writes per rate.
Use `--soak` to run 60 writes per rate with periodic feed gaps. 
Use `--s32` to verify 32-bit PCM playback at all three sample rates.

For synthetic XRUN recovery testing you can build with
`.\scripts\build-reactos.ps1 -InjectTestXrun`. 

It runs the driver's recovery path but does not validate XRUN delivery by a real device or its event queue.

## Next goals

- Implement VirtIO device mixer controls.
- Necessary bit for audio recording (microphone pin)
- Succcessively match the virtio host device's capabilities as far as the guest's sound API allows

