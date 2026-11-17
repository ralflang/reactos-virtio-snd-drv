[CmdletBinding()]
param(
    [string]$DriverPath = (Join-Path $PSScriptRoot '..\build\reactos-0.4.16-i386\virtiosnd.sys'),
    [string]$InfPath = (Join-Path $PSScriptRoot '..\virtiosnd.inf'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\build\virtiosnd-test.iso')
)

$ErrorActionPreference = 'Stop'

$DriverPath = (Resolve-Path $DriverPath).Path
$InfPath = (Resolve-Path $InfPath).Path
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

if (Test-Path $OutputPath) {
    throw "Refusing to overwrite existing ISO: $OutputPath"
}

$BuildDirectory = Join-Path (Split-Path $PSScriptRoot -Parent) 'build'
$StagingDirectory = Join-Path $BuildDirectory ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $StagingDirectory | Out-Null

try {
    Copy-Item $DriverPath (Join-Path $StagingDirectory 'virtiosnd.sys')
    Copy-Item $InfPath (Join-Path $StagingDirectory 'virtiosnd.inf')

    $Image = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
    $Image.FileSystemsToCreate = 3
    $Image.VolumeName = 'VIRTIOSND'
    $Image.Root.AddTree($StagingDirectory, $false)
    $Result = $Image.CreateResultImage()

    if (-not ('VirtioSndIsoStreamWriter' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class VirtioSndIsoStreamWriter
{
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int SeekFunction(IntPtr self, long offset, int origin, IntPtr newPosition);

    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int ReadFunction(
        IntPtr self,
        [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 1)] byte[] buffer,
        int count,
        IntPtr bytesRead);

    public static void Save(object sourceObject, string outputPath)
    {
        IntPtr unknown = Marshal.GetIUnknownForObject(sourceObject);
        Guid streamIid = new Guid("0000000c-0000-0000-C000-000000000046");
        IntPtr stream = IntPtr.Zero;
        int result = Marshal.QueryInterface(unknown, in streamIid, out stream);
        Marshal.Release(unknown);
        Marshal.ThrowExceptionForHR(result);

        string temporaryPath = outputPath + ".tmp";
        try
        {
            IntPtr vtable = Marshal.ReadIntPtr(stream);
            var seek = (SeekFunction)Marshal.GetDelegateForFunctionPointer(
                Marshal.ReadIntPtr(vtable, 5 * IntPtr.Size),
                typeof(SeekFunction));
            var read = (ReadFunction)Marshal.GetDelegateForFunctionPointer(
                Marshal.ReadIntPtr(vtable, 3 * IntPtr.Size),
                typeof(ReadFunction));

            Marshal.ThrowExceptionForHR(seek(stream, 0, 0, IntPtr.Zero));
            IntPtr readCount = Marshal.AllocHGlobal(sizeof(int));
            try
            {
                using (var output = new FileStream(
                    temporaryPath,
                    FileMode.CreateNew,
                    FileAccess.Write,
                    FileShare.None))
                {
                    var buffer = new byte[65536];
                    while (true)
                    {
                        Marshal.WriteInt32(readCount, 0);
                        Marshal.ThrowExceptionForHR(read(stream, buffer, buffer.Length, readCount));
                        int count = Marshal.ReadInt32(readCount);
                        if (count <= 0)
                            break;

                        output.Write(buffer, 0, count);
                    }
                }

                File.Move(temporaryPath, outputPath);
            }
            finally
            {
                Marshal.FreeHGlobal(readCount);
            }
        }
        catch
        {
            if (File.Exists(temporaryPath))
                File.Delete(temporaryPath);
            throw;
        }
        finally
        {
            Marshal.Release(stream);
        }
    }
}
'@
    }

    [VirtioSndIsoStreamWriter]::Save($Result.ImageStream, $OutputPath)
    Write-Output "Driver media created: $OutputPath"
}
finally {
    Remove-Item -LiteralPath $StagingDirectory -Recurse -Force
}
