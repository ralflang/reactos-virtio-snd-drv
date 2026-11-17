[CmdletBinding()]
param(
    [string]$VmDir = (Join-Path $HOME 'ReactOS-VM'),
    [string]$QemuDir = 'C:\Program Files\qemu',
    [ValidateSet('dsound', 'sdl', 'wav')]
    [string]$AudioBackend = 'dsound',
    [string]$DriverIsoPath
)

$ErrorActionPreference = 'Stop'

$BaseDisk = Join-Path $VmDir 'reactos.qcow2'
$DevDisk = Join-Path $VmDir 'reactos-dev.qcow2'
$OsIso = Join-Path $VmDir 'ReactOS.iso'
$QemuExe = Join-Path $QemuDir 'qemu-system-x86_64.exe'
$QemuImgExe = Join-Path $QemuDir 'qemu-img.exe'

foreach ($RequiredPath in @($BaseDisk, $QemuExe, $QemuImgExe)) {
    if (-not (Test-Path $RequiredPath)) {
        throw "Required file not found: $RequiredPath"
    }
}

if (-not (Test-Path $DevDisk)) {
    & $QemuImgExe create -f qcow2 -F qcow2 -b $BaseDisk $DevDisk
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create development overlay: $DevDisk"
    }
}

$AudioDevice = "$AudioBackend,id=audio1"
if ($AudioBackend -eq 'wav') {
    $WavPath = Join-Path $VmDir 'virtiosnd-output.wav'
    $AudioDevice += ",path=$WavPath"
}

$QemuArgs = @(
    '-accel', 'whpx,kernel-irqchip=off',
    '-m', '1G',
    '-smp', '1',
    '-drive', "if=ide,index=0,media=disk,file=$DevDisk",
    '-vga', 'std',
    '-audiodev', $AudioDevice,
    '-device', 'virtio-sound-pci,audiodev=audio1',
    '-net', 'nic,model=e1000',
    '-net', 'user',
    '-usb',
    '-device', 'usb-tablet',
    '-display', 'sdl',
    '-boot', 'order=c'
)

if (Test-Path $OsIso) {
    $QemuArgs += @('-drive', "if=ide,index=2,media=cdrom,file=$OsIso")
}

if ($DriverIsoPath) {
    if (-not (Test-Path $DriverIsoPath)) {
        throw "Driver ISO not found: $DriverIsoPath"
    }

    $QemuArgs += @('-drive', "if=ide,index=3,media=cdrom,file=$DriverIsoPath")
}

Write-Host "Starting ReactOS from development overlay: $DevDisk"
Write-Host "VirtIO sound backend: $AudioBackend"
& $QemuExe @QemuArgs
if ($LASTEXITCODE -ne 0) {
    throw "QEMU exited with code $LASTEXITCODE"
}
