#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PLAYBACK_CYCLES 10
#define SOAK_PLAYBACK_CYCLES 60
#define PLAYBACK_GAP_INTERVAL 10
#define WAVEOUT_OPEN_ATTEMPTS 12

static int
TestMixerControls(void)
{
    HMIXER Mixer = NULL;
    MIXERLINEW Line;
    MIXERLINECONTROLSW LineControls;
    MIXERCONTROLW VolumeControl;
    MIXERCONTROLW MuteControl;
    MIXERCONTROLDETAILS Details;
    MIXERCONTROLDETAILS_UNSIGNED OriginalVolume[2];
    MIXERCONTROLDETAILS_UNSIGNED TestVolume[2];
    MIXERCONTROLDETAILS_BOOLEAN OriginalMute[2];
    MIXERCONTROLDETAILS_BOOLEAN TestMute[2];
    DWORD VolumeChannels = 0;
    DWORD MuteChannels = 0;
    DWORD Channel;
    DWORD TargetVolume;
    DWORD Tolerance;
    BOOL VolumeSaved = FALSE;
    BOOL VolumeChanged = FALSE;
    BOOL MuteSaved = FALSE;
    BOOL MuteChanged = FALSE;
    MMRESULT Result;
    int ExitCode = 1;

    Result = mixerOpen(&Mixer, 0, 0, 0, 0);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mixerOpen failed (%u)\n", Result);
        goto Exit;
    }

    ZeroMemory(&Line, sizeof(Line));
    Line.cbStruct = sizeof(Line);
    Line.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    Result = mixerGetLineInfoW(
        (HMIXEROBJ)Mixer,
        &Line,
        MIXER_GETLINEINFOF_COMPONENTTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mixer line query failed (%u)\n", Result);
        goto Exit;
    }
    if (Line.cChannels == 0 || Line.cChannels > 2)
    {
        fprintf(stderr, "Unsupported speaker channel count (%lu)\n",
            Line.cChannels);
        goto Exit;
    }

    ZeroMemory(&VolumeControl, sizeof(VolumeControl));
    VolumeControl.cbStruct = sizeof(VolumeControl);
    ZeroMemory(&LineControls, sizeof(LineControls));
    LineControls.cbStruct = sizeof(LineControls);
    LineControls.dwLineID = Line.dwLineID;
    LineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    LineControls.cControls = 1;
    LineControls.cbmxctrl = sizeof(VolumeControl);
    LineControls.pamxctrl = &VolumeControl;
    Result = mixerGetLineControlsW(
        (HMIXEROBJ)Mixer,
        &LineControls,
        MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume control query failed (%u)\n", Result);
        goto Exit;
    }

    VolumeChannels =
        (VolumeControl.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM) != 0
        ? 1
        : Line.cChannels;
    ZeroMemory(&Details, sizeof(Details));
    Details.cbStruct = sizeof(Details);
    Details.dwControlID = VolumeControl.dwControlID;
    Details.cChannels = VolumeChannels;
    Details.cbDetails = sizeof(OriginalVolume[0]);
    Details.paDetails = OriginalVolume;
    Result = mixerGetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume read failed (%u)\n", Result);
        goto Exit;
    }
    VolumeSaved = TRUE;

    TargetVolume = 0x8000;
    for (Channel = 0; Channel < VolumeChannels; ++Channel)
        TestVolume[Channel].dwValue = TargetVolume;
    Details.paDetails = TestVolume;
    VolumeChanged = TRUE;
    Result = mixerSetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_SETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume set failed (%u)\n", Result);
        goto Exit;
    }

    Details.paDetails = TestVolume;
    Result = mixerGetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume readback failed (%u)\n", Result);
        goto Exit;
    }
    Tolerance = (VolumeControl.Bounds.dwMaximum -
        VolumeControl.Bounds.dwMinimum) / 32 + 1;
    for (Channel = 0; Channel < VolumeChannels; ++Channel)
    {
        DWORD Difference = TestVolume[Channel].dwValue > TargetVolume
            ? TestVolume[Channel].dwValue - TargetVolume
            : TargetVolume - TestVolume[Channel].dwValue;
        if (Difference > Tolerance)
        {
            fprintf(stderr, "Speaker volume readback outside tolerance\n");
            goto Exit;
        }
    }

    ZeroMemory(&MuteControl, sizeof(MuteControl));
    MuteControl.cbStruct = sizeof(MuteControl);
    LineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
    LineControls.pamxctrl = &MuteControl;
    Result = mixerGetLineControlsW(
        (HMIXEROBJ)Mixer,
        &LineControls,
        MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute control query failed (%u)\n", Result);
        goto Exit;
    }

    MuteChannels =
        (MuteControl.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM) != 0
        ? 1
        : Line.cChannels;
    Details.dwControlID = MuteControl.dwControlID;
    Details.cChannels = MuteChannels;
    Details.cbDetails = sizeof(OriginalMute[0]);
    Details.paDetails = OriginalMute;
    Result = mixerGetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute read failed (%u)\n", Result);
        goto Exit;
    }
    MuteSaved = TRUE;

    for (Channel = 0; Channel < MuteChannels; ++Channel)
        TestMute[Channel].fValue = TRUE;
    Details.paDetails = TestMute;
    MuteChanged = TRUE;
    Result = mixerSetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_SETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute set failed (%u)\n", Result);
        goto Exit;
    }

    Result = mixerGetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute readback failed (%u)\n", Result);
        goto Exit;
    }
    for (Channel = 0; Channel < MuteChannels; ++Channel)
    {
        if (TestMute[Channel].fValue == FALSE)
        {
            fprintf(stderr, "Speaker mute readback was not muted\n");
            goto Exit;
        }
    }

    ExitCode = 0;
    printf("Speaker volume and mute controls: Set/get passed\n");

Exit:
    if (Mixer != NULL && MuteSaved && MuteChanged)
    {
        Details.dwControlID = MuteControl.dwControlID;
        Details.cChannels = MuteChannels;
        Details.cbDetails = sizeof(OriginalMute[0]);
        Details.paDetails = OriginalMute;
        Result = mixerSetControlDetails(
            (HMIXEROBJ)Mixer,
            &Details,
            MIXER_SETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "Failed to restore speaker mute (%u)\n", Result);
            ExitCode = 1;
        }
    }

    if (Mixer != NULL && VolumeSaved && VolumeChanged)
    {
        Details.dwControlID = VolumeControl.dwControlID;
        Details.cChannels = VolumeChannels;
        Details.cbDetails = sizeof(OriginalVolume[0]);
        Details.paDetails = OriginalVolume;
        Result = mixerSetControlDetails(
            (HMIXEROBJ)Mixer,
            &Details,
            MIXER_SETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "Failed to restore speaker volume (%u)\n", Result);
            ExitCode = 1;
        }
    }

    if (Mixer != NULL)
    {
        Result = mixerClose(Mixer);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "mixerClose failed (%u)\n", Result);
            ExitCode = 1;
        }
    }

    return ExitCode;
}

static int
CheckPlaybackPosition(HWAVEOUT WaveOut, UINT SampleRate)
{
    MMTIME Position;
    DWORD PreviousPosition = MAXDWORD;
    DWORD Attempt;
    MMRESULT Result;

    for (Attempt = 0; Attempt < 50; ++Attempt)
    {
        ZeroMemory(&Position, sizeof(Position));
        Position.wType = TIME_BYTES;
        Result = waveOutGetPosition(WaveOut, &Position, sizeof(Position));
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "%u Hz: waveOutGetPosition failed (%u)\n",
                SampleRate,
                Result);
            return 1;
        }
        if (Position.wType != TIME_BYTES)
        {
            fprintf(stderr, "%u Hz: device did not return byte positions\n",
                SampleRate);
            return 1;
        }
        if (Position.u.cb != 0 && Position.u.cb != PreviousPosition)
            return 0;

        PreviousPosition = Position.u.cb;
        Sleep(20);
    }

    fprintf(stderr, "%u Hz: Playback position did not advance\n", SampleRate);
    return 1;
}

static int
PlayRate(
    UINT SampleRate,
    DWORD PlaybackCycles,
    UINT BitsPerSample)
{
    const UINT Channels = 2;
    const DWORD DurationSeconds = 1;
    DWORD FrameCount = SampleRate * DurationSeconds;
    DWORD BufferBytes = FrameCount * Channels * BitsPerSample / 8;
    PVOID Samples = NULL;
    HANDLE CompletedEvent = NULL;
    HWAVEOUT WaveOut = NULL;
    WAVEFORMATEX Format;
    WAVEHDR Header;
    MMRESULT Result;
    DWORD Frame;
    DWORD Cycle;
    DWORD Retry;
    int ExitCode = 1;

    Samples = HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        BufferBytes);
    if (Samples == NULL)
    {
        fprintf(stderr, "%u Hz: audio buffer allocation failed\n", SampleRate);
        goto Exit;
    }

    for (Frame = 0; Frame < FrameCount; ++Frame)
    {
        DWORD TonePeriod = SampleRate / 440;
        if (BitsPerSample == 16)
        {
            SHORT Value =
                Frame % TonePeriod < TonePeriod / 2 ? 512 : -512;
            PSHORT Samples16 = (PSHORT)Samples;

            Samples16[Frame * Channels] = Value;
            Samples16[Frame * Channels + 1] = Value;
        }
        else
        {
            LONG Value =
                Frame % TonePeriod < TonePeriod / 2
                ? 0x01000000
                : -0x01000000;
            PLONG Samples32 = (PLONG)Samples;

            Samples32[Frame * Channels] = Value;
            Samples32[Frame * Channels + 1] = Value;
        }
    }

    CompletedEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (CompletedEvent == NULL)
    {
        fprintf(stderr, "%u Hz: event creation failed (%lu)\n",
            SampleRate,
            GetLastError());
        goto Exit;
    }

    ZeroMemory(&Format, sizeof(Format));
    Format.wFormatTag = WAVE_FORMAT_PCM;
    Format.nChannels = Channels;
    Format.nSamplesPerSec = SampleRate;
    Format.wBitsPerSample = BitsPerSample;
    Format.nBlockAlign = Channels * BitsPerSample / 8;
    Format.nAvgBytesPerSec = SampleRate * Format.nBlockAlign;

    for (Retry = 0; Retry < WAVEOUT_OPEN_ATTEMPTS; ++Retry)
    {
        Result = waveOutOpen(
            &WaveOut,
            WAVE_MAPPER,
            &Format,
            (DWORD_PTR)CompletedEvent,
            0,
            CALLBACK_EVENT);
        if (Result == MMSYSERR_NOERROR ||
            (Result != MMSYSERR_ERROR && Result != MMSYSERR_ALLOCATED))
        {
            break;
        }
        Sleep(500);
    }
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "%u Hz: waveOutOpen failed after retries (%u)\n",
            SampleRate,
            Result);
        goto Exit;
    }

    ZeroMemory(&Header, sizeof(Header));
    Header.lpData = (LPSTR)Samples;
    Header.dwBufferLength = BufferBytes;

    Result = waveOutPrepareHeader(WaveOut, &Header, sizeof(Header));
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "%u Hz: waveOutPrepareHeader failed (%u)\n",
            SampleRate,
            Result);
        goto Reset;
    }

    for (Cycle = 0; Cycle < PlaybackCycles; ++Cycle)
    {
        if (!ResetEvent(CompletedEvent))
        {
            fprintf(stderr, "%u Hz: event reset failed (%lu)\n",
                SampleRate,
                GetLastError());
            goto Reset;
        }

        Result = waveOutWrite(WaveOut, &Header, sizeof(Header));
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "%u Hz: waveOutWrite cycle %lu failed (%u)\n",
                SampleRate,
                Cycle + 1,
                Result);
            goto Reset;
        }

        if (Cycle == 0 && CheckPlaybackPosition(WaveOut, SampleRate) != 0)
            goto Reset;

        if (WaitForSingleObject(CompletedEvent, 15000) != WAIT_OBJECT_0)
        {
            fprintf(stderr, "%u Hz: Playback cycle %lu timed out\n",
                SampleRate,
                Cycle + 1);
            goto Reset;
        }

        if ((Header.dwFlags & WHDR_DONE) == 0)
        {
            fprintf(stderr,
                "%u Hz: Completion event without WHDR_DONE on cycle %lu\n",
                SampleRate,
                Cycle + 1);
            goto Reset;
        }

        if (Cycle % PLAYBACK_GAP_INTERVAL == 2 ||
            Cycle % PLAYBACK_GAP_INTERVAL == 5)
        {
            Sleep(500);
        }

        if (SampleRate == 44100 && Cycle == 2 &&
            TestMixerControls() != 0)
        {
            goto Reset;
        }
    }

    ExitCode = 0;
    printf("%u Hz stereo S%u: %lu one-second writes completed\n",
        SampleRate,
        BitsPerSample,
        PlaybackCycles);

    Result = waveOutUnprepareHeader(WaveOut, &Header, sizeof(Header));
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "%u Hz: waveOutUnprepareHeader failed (%u)\n",
            SampleRate,
            Result);
        ExitCode = 1;
    }
    goto Close;

Reset:
    waveOutReset(WaveOut);
    if ((Header.dwFlags & WHDR_PREPARED) != 0)
        waveOutUnprepareHeader(WaveOut, &Header, sizeof(Header));

Close:
    Result = waveOutClose(WaveOut);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "%u Hz: waveOutClose failed (%u)\n",
            SampleRate,
            Result);
        ExitCode = 1;
    }

Exit:
    if (CompletedEvent != NULL)
        CloseHandle(CompletedEvent);
    if (Samples != NULL)
        HeapFree(GetProcessHeap(), 0, Samples);
    return ExitCode;
}

int
main(int argc, char **argv)
{
    static const UINT SampleRates[] = { 44100, 48000, 96000 };
    UINT DeviceCount = waveOutGetNumDevs();
    DWORD PlaybackCycles = DEFAULT_PLAYBACK_CYCLES;
    UINT BitsPerSample = 16;
    UINT Index;

    if (argc == 2 && strcmp(argv[1], "--soak") == 0)
    {
        PlaybackCycles = SOAK_PLAYBACK_CYCLES;
    }
    else if (argc == 2 && strcmp(argv[1], "--s32") == 0)
    {
        BitsPerSample = 32;
    }
    else if (argc != 1)
    {
        fprintf(stderr,
            "usage: waveout-rate-smoke.exe [--soak | --s32]\n");
        return 2;
    }

    printf("waveOut devices: %u\n", DeviceCount);
    if (PlaybackCycles != DEFAULT_PLAYBACK_CYCLES)
        printf("Running %lu playback cycles per rate\n", PlaybackCycles);
    if (DeviceCount == 0)
        return 1;

    for (Index = 0; Index < sizeof(SampleRates) / sizeof(SampleRates[0]); ++Index)
    {
        if (PlayRate(SampleRates[Index], PlaybackCycles, BitsPerSample) != 0)
            return 1;
    }

    return 0;
}
