[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$WavPath
)

$ErrorActionPreference = 'Stop'
$Bytes = [System.IO.File]::ReadAllBytes((Resolve-Path $WavPath))
if ([System.Text.Encoding]::ASCII.GetString($Bytes, 0, 4) -ne 'RIFF' -or
    [System.Text.Encoding]::ASCII.GetString($Bytes, 8, 4) -ne 'WAVE') {
    throw 'The capture is not a RIFF/WAVE file.'
}

$Offset = 12
$Format = $null
$DataOffset = -1
$DataLength = 0
while ($Offset + 8 -le $Bytes.Length) {
    $ChunkId = [System.Text.Encoding]::ASCII.GetString($Bytes, $Offset, 4)
    $ChunkLength = [BitConverter]::ToUInt32($Bytes, $Offset + 4)
    if ($ChunkId -eq 'fmt ') {
        $Format = @{
            Code = [BitConverter]::ToUInt16($Bytes, $Offset + 8)
            Channels = [BitConverter]::ToUInt16($Bytes, $Offset + 10)
            Rate = [BitConverter]::ToUInt32($Bytes, $Offset + 12)
            Bits = [BitConverter]::ToUInt16($Bytes, $Offset + 22)
        }
    }
    elseif ($ChunkId -eq 'data') {
        $DataOffset = $Offset + 8
        $DataLength = $ChunkLength
        break
    }

    $Offset += 8 + $ChunkLength + ($ChunkLength % 2)
}

if ($null -eq $Format -or $DataOffset -lt 0 -or
    $Format.Code -ne 1 -or $Format.Bits -ne 16) {
    throw 'Only PCM 16-bit WAV captures are supported.'
}

if ($DataLength -eq 0) {
    $DataLength = $Bytes.Length - $DataOffset
}
$BytesPerFrame = $Format.Channels * 2
$FrameCount = [int][Math]::Floor(
    [Math]::Min($DataLength, $Bytes.Length - $DataOffset) / $BytesPerFrame)
$WindowFrames = [int][Math]::Floor($Format.Rate / 10)
$Frequencies = @(440, 660, 880)
$Windows = [System.Collections.Generic.List[object]]::new()

for ($StartFrame = 0; $StartFrame + $WindowFrames -le $FrameCount;
     $StartFrame += $WindowFrames) {
    $Samples = [double[]]::new($WindowFrames)
    $SumSquares = 0.0
    for ($Frame = 0; $Frame -lt $WindowFrames; ++$Frame) {
        $Value = 0.0
        for ($Channel = 0; $Channel -lt $Format.Channels; ++$Channel) {
            $SampleOffset = $DataOffset +
                (($StartFrame + $Frame) * $BytesPerFrame) + ($Channel * 2)
            $Value += [BitConverter]::ToInt16($Bytes, $SampleOffset)
        }
        $Value /= $Format.Channels
        $Samples[$Frame] = $Value
        $SumSquares += $Value * $Value
    }

    $Powers = [double[]]::new($Frequencies.Count)
    for ($Index = 0; $Index -lt $Frequencies.Count; ++$Index) {
        $Coefficient = 2.0 * [Math]::Cos(
            2.0 * [Math]::PI * $Frequencies[$Index] / $Format.Rate)
        $Previous = 0.0
        $PreviousPrevious = 0.0
        foreach ($Sample in $Samples) {
            $Current = $Sample + $Coefficient * $Previous -
                $PreviousPrevious
            $PreviousPrevious = $Previous
            $Previous = $Current
        }
        $Powers[$Index] = $Previous * $Previous +
            $PreviousPrevious * $PreviousPrevious -
            $Coefficient * $Previous * $PreviousPrevious
    }

    $PeakIndex = 0
    for ($Index = 1; $Index -lt $Powers.Length; ++$Index) {
        if ($Powers[$Index] -gt $Powers[$PeakIndex]) {
            $PeakIndex = $Index
        }
    }

    $Windows.Add([pscustomobject]@{
        Start = $StartFrame / $Format.Rate
        Rms = [Math]::Sqrt($SumSquares / $WindowFrames)
        Peak = $Frequencies[$PeakIndex]
        Tone = [Math]::Sqrt($Powers[$PeakIndex]) / $WindowFrames
    })
}

$NearMax = $Windows |
    Where-Object { $_.Peak -eq 440 -and $_.Rms -gt 1000 -and $_.Tone -gt 100 } |
    Sort-Object Rms -Descending |
    Select-Object -First 1
if ($null -eq $NearMax) {
    throw 'No full-level 440 Hz test segment was found.'
}

$Attenuated = $Windows |
    Where-Object {
        $_.Start -gt $NearMax.Start -and $_.Peak -eq 660 -and
        $_.Rms -gt 5 -and $_.Rms -lt ($NearMax.Rms * 0.1) -and $_.Tone -gt 5
    } |
    Sort-Object Start |
    Select-Object -First 1
if ($null -eq $Attenuated) {
    throw 'No attenuated 660 Hz test segment was found.'
}

$MutedStart = $Attenuated.Start + 1.2
$MutedWindows = @($Windows | Where-Object {
    $_.Start -ge $MutedStart -and $_.Start -lt ($MutedStart + 0.5)
})
if ($MutedWindows.Count -lt 3) {
    throw 'The capture ended before the muted segment could be checked.'
}
$MutedRms = ($MutedWindows | Measure-Object Rms -Maximum).Maximum
if ($MutedRms -ge ($NearMax.Rms * 0.01) -or
    ($MutedWindows | Where-Object { $_.Peak -eq 880 -and $_.Tone -gt 5 })) {
    throw ('Mute was not silent: measured RMS {0:N1}' -f $MutedRms)
}

$AttenuationRatio = $Attenuated.Rms / $NearMax.Rms
Write-Output ('Near-max 440 Hz RMS: {0:N1}' -f $NearMax.Rms)
Write-Output ('Attenuated 660 Hz RMS: {0:N1} ({1:P2} of near-max)' -f
    $Attenuated.Rms, $AttenuationRatio)
Write-Output ('Muted 880 Hz RMS upper bound: {0:N1}' -f $MutedRms)
Write-Output 'Volume attenuation and mute capture checks passed.'
