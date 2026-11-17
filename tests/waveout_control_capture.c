#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define SEGMENT_SECONDS 1
#define WAVEOUT_OPEN_ATTEMPTS 12

typedef struct _MIXER_TEST_STATE
{
    HMIXER Mixer;
    MIXERCONTROLW VolumeControl;
    MIXERCONTROLW MuteControl;
    DWORD VolumeChannels;
    DWORD MuteChannels;
    MIXERCONTROLDETAILS_UNSIGNED OriginalVolume[2];
    MIXERCONTROLDETAILS_BOOLEAN OriginalMute[2];
    BOOL VolumeSaved;
    BOOL MuteSaved;
} MIXER_TEST_STATE, *PMIXER_TEST_STATE;

static MMRESULT
GetControlDetails(
    HMIXER Mixer,
    DWORD ControlId,
    DWORD Channels,
    DWORD DetailSize,
    PVOID DetailsBuffer)
{
    MIXERCONTROLDETAILS Details;

    ZeroMemory(&Details, sizeof(Details));
    Details.cbStruct = sizeof(Details);
    Details.dwControlID = ControlId;
    Details.cChannels = Channels;
    Details.cbDetails = DetailSize;
    Details.paDetails = DetailsBuffer;
    return mixerGetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
}

static MMRESULT
SetControlDetails(
    HMIXER Mixer,
    DWORD ControlId,
    DWORD Channels,
    DWORD DetailSize,
    PVOID DetailsBuffer)
{
    MIXERCONTROLDETAILS Details;

    ZeroMemory(&Details, sizeof(Details));
    Details.cbStruct = sizeof(Details);
    Details.dwControlID = ControlId;
    Details.cChannels = Channels;
    Details.cbDetails = DetailSize;
    Details.paDetails = DetailsBuffer;
    return mixerSetControlDetails(
        (HMIXEROBJ)Mixer,
        &Details,
        MIXER_SETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HMIXER);
}

static int
OpenMixer(PMIXER_TEST_STATE State)
{
    MIXERLINEW Line;
    MIXERLINECONTROLSW LineControls;
    MMRESULT Result;

    ZeroMemory(State, sizeof(*State));
    Result = mixerOpen(&State->Mixer, 0, 0, 0, 0);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mixerOpen failed (%u)\n", Result);
        return 1;
    }

    ZeroMemory(&Line, sizeof(Line));
    Line.cbStruct = sizeof(Line);
    Line.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    Result = mixerGetLineInfoW(
        (HMIXEROBJ)State->Mixer,
        &Line,
        MIXER_GETLINEINFOF_COMPONENTTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR || Line.cChannels == 0 || Line.cChannels > 2)
    {
        fprintf(stderr, "speaker line query failed (%u)\n", Result);
        return 1;
    }

    ZeroMemory(&State->VolumeControl, sizeof(State->VolumeControl));
    State->VolumeControl.cbStruct = sizeof(State->VolumeControl);
    ZeroMemory(&LineControls, sizeof(LineControls));
    LineControls.cbStruct = sizeof(LineControls);
    LineControls.dwLineID = Line.dwLineID;
    LineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    LineControls.cControls = 1;
    LineControls.cbmxctrl = sizeof(State->VolumeControl);
    LineControls.pamxctrl = &State->VolumeControl;
    Result = mixerGetLineControlsW(
        (HMIXEROBJ)State->Mixer,
        &LineControls,
        MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "speaker volume query failed (%u)\n", Result);
        return 1;
    }
    State->VolumeChannels =
        (State->VolumeControl.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM) != 0
        ? 1
        : Line.cChannels;

    ZeroMemory(&State->MuteControl, sizeof(State->MuteControl));
    State->MuteControl.cbStruct = sizeof(State->MuteControl);
    LineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
    LineControls.pamxctrl = &State->MuteControl;
    Result = mixerGetLineControlsW(
        (HMIXEROBJ)State->Mixer,
        &LineControls,
        MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_HMIXER);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "speaker mute query failed (%u)\n", Result);
        return 1;
    }
    State->MuteChannels =
        (State->MuteControl.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM) != 0
        ? 1
        : Line.cChannels;

    Result = GetControlDetails(
        State->Mixer,
        State->VolumeControl.dwControlID,
        State->VolumeChannels,
        sizeof(State->OriginalVolume[0]),
        State->OriginalVolume);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "speaker volume read failed (%u)\n", Result);
        return 1;
    }
    State->VolumeSaved = TRUE;

    Result = GetControlDetails(
        State->Mixer,
        State->MuteControl.dwControlID,
        State->MuteChannels,
        sizeof(State->OriginalMute[0]),
        State->OriginalMute);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute read failed (%u)\n", Result);
        return 1;
    }
    State->MuteSaved = TRUE;
    return 0;
}

static int
SetMixerState(
    PMIXER_TEST_STATE State,
    DWORD Volume,
    BOOL Muted)
{
    MIXERCONTROLDETAILS_UNSIGNED VolumeDetails[2];
    MIXERCONTROLDETAILS_BOOLEAN MuteDetails[2];
    DWORD Channel;
    MMRESULT Result;

    for (Channel = 0; Channel < State->VolumeChannels; ++Channel)
        VolumeDetails[Channel].dwValue = Volume;
    Result = SetControlDetails(
        State->Mixer,
        State->VolumeControl.dwControlID,
        State->VolumeChannels,
        sizeof(VolumeDetails[0]),
        VolumeDetails);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume set failed (%u)\n", Result);
        return 1;
    }

    Result = GetControlDetails(
        State->Mixer,
        State->VolumeControl.dwControlID,
        State->VolumeChannels,
        sizeof(VolumeDetails[0]),
        VolumeDetails);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker volume readback failed (%u)\n", Result);
        return 1;
    }
    for (Channel = 0; Channel < State->VolumeChannels; ++Channel)
    {
        DWORD Difference = VolumeDetails[Channel].dwValue > Volume
            ? VolumeDetails[Channel].dwValue - Volume
            : Volume - VolumeDetails[Channel].dwValue;
        DWORD Tolerance = (State->VolumeControl.Bounds.dwMaximum -
            State->VolumeControl.Bounds.dwMinimum) / 32 + 1;
        if (Difference > Tolerance)
        {
            fprintf(stderr, "Speaker volume readback outside tolerance\n");
            return 1;
        }
    }

    for (Channel = 0; Channel < State->MuteChannels; ++Channel)
        MuteDetails[Channel].fValue = Muted;
    Result = SetControlDetails(
        State->Mixer,
        State->MuteControl.dwControlID,
        State->MuteChannels,
        sizeof(MuteDetails[0]),
        MuteDetails);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute set failed (%u)\n", Result);
        return 1;
    }

    Result = GetControlDetails(
        State->Mixer,
        State->MuteControl.dwControlID,
        State->MuteChannels,
        sizeof(MuteDetails[0]),
        MuteDetails);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Speaker mute readback failed (%u)\n", Result);
        return 1;
    }
    for (Channel = 0; Channel < State->MuteChannels; ++Channel)
    {
        if (MuteDetails[Channel].fValue != Muted)
        {
            fprintf(stderr, "Speaker mute readback mismatch\n");
            return 1;
        }
    }

    return 0;
}

static int
PlayTone(
    HWAVEOUT WaveOut,
    HANDLE CompletedEvent,
    WAVEHDR *Header,
    SHORT *Samples,
    UINT Frequency)
{
    const DWORD FrameCount = SAMPLE_RATE * SEGMENT_SECONDS;
    DWORD Frame;
    DWORD Phase = 0;
    MMRESULT Result;

    for (Frame = 0; Frame < FrameCount; ++Frame)
    {
        SHORT Value;

        Phase += Frequency;
        if (Phase >= SAMPLE_RATE)
            Phase -= SAMPLE_RATE;
        Value = Phase < SAMPLE_RATE / 2 ? 12000 : -12000;
        Samples[Frame * CHANNELS] = Value;
        Samples[Frame * CHANNELS + 1] = Value;
    }

    if (!ResetEvent(CompletedEvent))
    {
        fprintf(stderr, "event reset failed (%lu)\n", GetLastError());
        return 1;
    }

    Result = waveOutWrite(WaveOut, Header, sizeof(*Header));
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "waveOutWrite failed (%u)\n", Result);
        return 1;
    }
    if (WaitForSingleObject(CompletedEvent, 15000) != WAIT_OBJECT_0 ||
        (Header->dwFlags & WHDR_DONE) == 0)
    {
        fprintf(stderr, "waveOutWrite timed out (flags %08lx)\n",
            Header->dwFlags);
        return 1;
    }

    return 0;
}

static int
RestoreMixerState(PMIXER_TEST_STATE State)
{
    MMRESULT Result;
    int Failed = 0;

    if (State->Mixer == NULL)
        return 0;

    if (State->MuteSaved)
    {
        Result = SetControlDetails(
            State->Mixer,
            State->MuteControl.dwControlID,
            State->MuteChannels,
            sizeof(State->OriginalMute[0]),
            State->OriginalMute);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "failed to restore speaker mute (%u)\n", Result);
            Failed = 1;
        }
    }
    if (State->VolumeSaved)
    {
        Result = SetControlDetails(
            State->Mixer,
            State->VolumeControl.dwControlID,
            State->VolumeChannels,
            sizeof(State->OriginalVolume[0]),
            State->OriginalVolume);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "failed to restore speaker volume (%u)\n", Result);
            Failed = 1;
        }
    }

    Result = mixerClose(State->Mixer);
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "mixerClose failed (%u)\n", Result);
        Failed = 1;
    }

    return Failed;
}

int
main(void)
{
    static const struct
    {
        UINT Frequency;
        DWORD Volume;
        BOOL Muted;
        const char *Label;
    } Segments[] = {
        /* 0xFD56 maps to the top supported 1 dB step in ReactOS MMixer. */
        { 440, 0xFD56, FALSE, "near-max-volume" },
        { 660, 0x8000, FALSE, "attenuated" },
        { 880, 0xFD56, TRUE, "muted" }
    };
    MIXER_TEST_STATE MixerState;
    HANDLE CompletedEvent = NULL;
    HWAVEOUT WaveOut = NULL;
    WAVEFORMATEX Format;
    WAVEHDR Header;
    SHORT *Samples = NULL;
    DWORD FrameCount = SAMPLE_RATE * SEGMENT_SECONDS;
    DWORD BufferBytes = FrameCount * CHANNELS * sizeof(SHORT);
    DWORD Retry;
    MMRESULT Result;
    BOOL HeaderPrepared = FALSE;
    UINT Index;
    int ExitCode = 1;

    if (OpenMixer(&MixerState) != 0)
        goto Exit;

    Samples = (SHORT *)HeapAlloc(GetProcessHeap(), 0, BufferBytes);
    CompletedEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (Samples == NULL || CompletedEvent == NULL)
    {
        fprintf(stderr, "audio buffer or event allocation failed\n");
        goto Exit;
    }

    ZeroMemory(&Format, sizeof(Format));
    Format.wFormatTag = WAVE_FORMAT_PCM;
    Format.nChannels = CHANNELS;
    Format.nSamplesPerSec = SAMPLE_RATE;
    Format.wBitsPerSample = 16;
    Format.nBlockAlign = CHANNELS * sizeof(SHORT);
    Format.nAvgBytesPerSec = SAMPLE_RATE * Format.nBlockAlign;

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
            break;
        Sleep(500);
    }
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "waveOutOpen failed (%u)\n", Result);
        goto Exit;
    }

    ZeroMemory(&Header, sizeof(Header));
    Header.lpData = (LPSTR)Samples;
    Header.dwBufferLength = BufferBytes;
    Result = waveOutPrepareHeader(WaveOut, &Header, sizeof(Header));
    if (Result != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "waveOutPrepareHeader failed (%u)\n", Result);
        goto Exit;
    }
    HeaderPrepared = TRUE;

    for (Index = 0; Index < sizeof(Segments) / sizeof(Segments[0]); ++Index)
    {
        if (SetMixerState(
                &MixerState,
                Segments[Index].Volume,
                Segments[Index].Muted) != 0)
            goto Exit;

        printf("%s: playing %u Hz for %u seconds\n",
            Segments[Index].Label,
            Segments[Index].Frequency,
            SEGMENT_SECONDS);
        if (PlayTone(
                WaveOut,
                CompletedEvent,
                &Header,
                Samples,
                Segments[Index].Frequency) != 0)
            goto Exit;
        Sleep(500);
    }

    ExitCode = 0;

Exit:
    if (WaveOut != NULL)
    {
        waveOutReset(WaveOut);
        if (HeaderPrepared)
        {
            Result = waveOutUnprepareHeader(
                WaveOut,
                &Header,
                sizeof(Header));
            if (Result != MMSYSERR_NOERROR)
            {
                fprintf(stderr, "waveOutUnprepareHeader failed (%u)\n", Result);
                ExitCode = 1;
            }
        }
        Result = waveOutClose(WaveOut);
        if (Result != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "waveOutClose failed (%u)\n", Result);
            ExitCode = 1;
        }
    }
    if (CompletedEvent != NULL)
        CloseHandle(CompletedEvent);
    if (Samples != NULL)
        HeapFree(GetProcessHeap(), 0, Samples);
    if (RestoreMixerState(&MixerState) != 0)
        ExitCode = 1;
    if (ExitCode == 0)
        printf("control capture segments completed\n");
    return ExitCode;
}
