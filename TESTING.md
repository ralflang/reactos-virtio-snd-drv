# Testing

These tests target an i386 ReactOS 0.4.16 guest running under QEMU with a
`virtio-sound-pci` device. 


I used the TCP Console service installed and reachable through the host forward on port `14141`.
https://github.com/ralflang/tcpconsole - you can also just run the tests interactively on the host and guest without this tool.

## Build the guest test tools

Build the test tools with RosBE and the
ReactOS-compatible MinGW compiler:

```powershell
& 'C:\RosBE\RosBE.ps1' i386
New-Item -ItemType Directory -Force .\build\guest-tests | Out-Null
& 'C:\RosBE\i386\bin\gcc.exe' -Wall -Wextra -Werror `
    -o .\build\guest-tests\waveout_control_capture.exe `
    .\tests\waveout_control_capture.c -lwinmm
& 'C:\RosBE\i386\bin\gcc.exe' -Wall -Wextra -Werror `
    -o .\build\guest-tests\waveout_rate_smoke.exe `
    .\tests\waveout_rate_smoke.c -lwinmm
```

The resulting executables are Windows/ReactOS guest programs.

## Stage the driver and tools

Copy the driver, INF, and test tools to the directory exposed by QEMU as the
guest's `D:\` drive:

```powershell
$Share = 'C:\Users\$user\ros'
Copy-Item .\build\reactos-0.4.16-i386\virtiosnd.sys "$Share\virtiosnd.sys" -Force
Copy-Item .\virtiosnd.inf "$Share\virtiosnd.inf" -Force
Copy-Item .\build\guest-tests\waveout_control_capture.exe "$Share\" -Force
Copy-Item .\build\guest-tests\waveout_rate_smoke.exe "$Share\" -Force
```

If the driver was built through the monolithic ReactOS checkout, use its
outputs instead:

```powershell
Copy-Item `
    ..\reactos-feature-build\drivers\wdm\audio\drivers\virtiosnd\virtiosnd.sys `
    "$Share\virtiosnd.sys" -Force
Copy-Item `
    ..\reactos-feature-build\drivers\wdm\audio\drivers\virtiosnd\virtiosnd.inf `
    "$Share\virtiosnd.inf" -Force
```

Restart QEMU after staging files. ReactOS does not notice refreshed contents
of the host-backed FAT disk until the VM is restarted.

## Install or replace the driver


### First Time

Use the graphical driver install dialog. If built from the monolithic reactos, the driver CD should already contain the built driver and detect a virtio pci sound device on first boot.

### Upgrade/replace

After the guest has restarted use the TCP Console client from the host:

```powershell
$Client = '..\tcpconsole\build\client-i386\tcpconsole-client.exe'
& $Client --port 14141 -- cmd.exe /c `
    'copy /y C:\ReactOS\System32\drivers\virtiosnd.sys C:\virtiosnd-before-test.sys'
& $Client --port 14141 -- cmd.exe /c `
    'copy /y C:\ReactOS\System32\drivers\virtiosnd.sys C:\virtiosnd-before-test.sys'
```

Or use a terminal inside the guest:
```powershell
    'copy /y C:\ReactOS\System32\drivers\virtiosnd.sys C:\virtiosnd-before-test.sys'
    'copy /y C:\ReactOS\System32\drivers\virtiosnd.sys C:\virtiosnd-before-test.sys'
```

Reboot the guest after replacing the `.sys` so the kernel reloads the driver.

## Run playback tests

Run the rate smoke test through TCP Console:

```powershell
& $Client --port 14141 -- cmd.exe /c 'D:\waveout_rate_smoke.exe'
```

It checks the mixer volume/mute controls and emits ten one-second stereo
buffers at 44.1, 48, and 96 kHz. Optional modes are:

```powershell
& $Client --port 14141 -- cmd.exe /c 'D:\waveout_rate_smoke.exe --soak'
& $Client --port 14141 -- cmd.exe /c 'D:\waveout_rate_smoke.exe --s32'
```

Run the control test to emit a near-max 440 Hz tone, an attenuated 660 Hz
tone and a muted 880 Hz segment:

```powershell
& $Client --port 14141 -- cmd.exe /c 'D:\waveout_control_capture.exe'
```

Success is reported as `control capture segments completed`.

## Verify a WAV capture

When the VM or host audio setup records the control-test output, verify the
capture on the host:

```powershell
.\tests\verify_control_capture.ps1 -WavPath C:\path\to\capture.wav
```

The verifier checks for the 440 Hz near-max segment, attenuated 660 Hz
segment and silent 880 Hz segment.
