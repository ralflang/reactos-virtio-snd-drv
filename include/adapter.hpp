/*
 * PROJECT:     ReactOS VirtIO Sound Driver
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     VirtIO sound adapter class
 * COPYRIGHT:   Copyright 2026 Ralf Lang <ralf.lang@ralf-lang.de>
 */

#ifndef _VIRTIOSND_ADAPTER_HPP_
#define _VIRTIOSND_ADAPTER_HPP_

#include "kcom.hpp"
#include "virtio_snd_protocol.h"
#include "virtiosnd.h"

#ifdef FORCEINLINE
#undef FORCEINLINE
#endif
#if defined(__GNUC__)
#define FORCEINLINE __attribute__((unused))
#else
#define FORCEINLINE
#endif
/* clang-format off */
/* Include order is significant:
 * virtio_pci.h defines VirtIODevice and struct virtqueue */
extern "C" {
#include <virtio_pci.h>
#include <VirtIO.h>
}
/* clang-format on */
#undef FORCEINLINE

#define VIRTIO_SND_POOL_TAG 'dSnV'
#define VIRTIO_SND_TX_SLOT_COUNT 4
#define VIRTIO_SND_EVENT_SLOT_COUNT 16

/** @brief One in-flight PCM transfer slot. */
typedef struct _VIRTIO_SND_TX_SLOT
{
    VIRTIO_SND_PCM_XFER Request;              /**< Device request for this period. */
    UCHAR Audio[VIRTIO_SND_PCM_PERIOD_BYTES]; /**< Period audio buffer. */
    VIRTIO_SND_PCM_STATUS Response;           /**< Device response for this period. */
    BOOLEAN InFlight;                         /**< Whether the device owns this slot. */
} VIRTIO_SND_TX_SLOT, *PVIRTIO_SND_TX_SLOT;

/** @brief One event-queue slot. */
typedef struct _VIRTIO_SND_EVENT_SLOT
{
    VIRTIO_SND_EVENT Event; /**< Event buffer posted to the device. */
} VIRTIO_SND_EVENT_SLOT, *PVIRTIO_SND_EVENT_SLOT;

/** @brief A mapped or I/O-space PCI base address register. */
typedef struct _VIRTIO_SND_BAR
{
    PHYSICAL_ADDRESS BaseAddress; /**< PCI resource base address. */
    SIZE_T Length;                /**< Length of the BAR resource. */
    PVOID MappedAddress;          /**< Kernel mapping for a memory BAR. */
    BOOLEAN IsPort;               /**< Whether the BAR describes port I/O. */
    BOOLEAN Assigned;             /**< Whether the BAR was assigned by PnP. */
} VIRTIO_SND_BAR, *PVIRTIO_SND_BAR;

/** @brief Captured PCI transport resources for the device. */
typedef struct _VIRTIO_SND_TRANSPORT
{
    ULONG BusNumber;                          /**< PCI bus containing the device. */
    PCI_SLOT_NUMBER SlotNumber;               /**< PCI device and function number. */
    VIRTIO_SND_BAR Bars[PCI_TYPE0_ADDRESSES]; /**< Captured BAR resources. */
} VIRTIO_SND_TRANSPORT, *PVIRTIO_SND_TRANSPORT;

class CVirtioSndAdapter;

typedef struct _VIRTIO_SND_PLAYBACK VIRTIO_SND_PLAYBACK, *PVIRTIO_SND_PLAYBACK;

/**
 * @brief Per-device storage carried in the PortCls device extension.
 *
 * Holds the heap-allocated adapter so the PnP dispatch can reach it from the
 * DeviceObject on STOP/REMOVE without a global, mirroring the hdaudio
 * PHDAUDIO_DEVICE_EXTENSION pattern.
 */
typedef struct _VIRTIO_SND_DEVICE_EXTENSION
{
    ULONG_PTR Reserved[4];      /**< PortCls-reserved device extension storage. */
    CVirtioSndAdapter *Adapter; /**< Reference owned by this device extension. */
} VIRTIO_SND_DEVICE_EXTENSION, *PVIRTIO_SND_DEVICE_EXTENSION;

/**
 * @brief Opaque per-stream playback context.
 *
 * Created by CVirtioSndAdapter::OpenPlayback and carried by the WaveCyclic
 * stream. Holds a borrowed back-pointer to its owning adapter so the playback
 * helper routines can reach adapter state without a global.
 */
struct _VIRTIO_SND_PLAYBACK
{
    CVirtioSndAdapter *Adapter; /**< Borrowed pointer to the owning adapter. */
    ULONG StreamId;             /**< VirtIO PCM stream assigned to this playback. */
    ULONG Channels;             /**< Number of interleaved audio channels. */
    ULONG Features;             /**< Negotiated stream features. */
    ULONG InFlightCount;        /**< Number of queued periods not yet completed. */
    PKEVENT WakeEvent;          /**< Event signaled when playback work is available. */
    BOOLEAN Prepared;           /**< Whether the device accepted PCM parameters. */
    BOOLEAN Started;            /**< Whether playback has been started. */
    BOOLEAN DeviceActive;       /**< Whether this context still owns a device stream. */
#if defined(VIRTIO_SND_TEST_XRUN_INJECTION)
    volatile LONG TestXrunInjected; /**< Whether the test XRUN has been injected. */
#endif
    PVIRTIO_SND_TX_SLOT Slots[VIRTIO_SND_TX_SLOT_COUNT]; /**< Transfer slots for queued periods. */
};

/**
 * @brief The VirtIO sound adapter.
 *
 * One heap-allocated, reference-counted instance exists per device. It holds the
 * VirtIO transport state and the master volume/mute state and implements IAdapterPowerManagement.
 */
class CVirtioSndAdapter : public CUnknownImpl<IAdapterPowerManagement>
{
  public:
    /** @brief Creates an uninitialized per-device adapter. */
    CVirtioSndAdapter();

    /**
     * @brief Queries the interfaces supported by this adapter.
     * @param Interface Interface identifier to query.
     * @param Object Receives the requested interface pointer.
     * @return STATUS_SUCCESS if supported, otherwise STATUS_NOINTERFACE.
     */
    STDMETHODIMP_(NTSTATUS) QueryInterface(_In_ REFIID Interface, _Out_ PVOID *Object);
    IMP_IAdapterPowerManagement;

    /** @name Device lifecycle
     * @{
     */
    /**
     * @brief Initializes the VirtIO device and its transport.
     * @param DeviceObject Owning device object.
     * @param ResourceList PnP-assigned resources.
     * @return Status of the initialization.
     */
    NTSTATUS
    StartTransport(_In_ PDEVICE_OBJECT DeviceObject, _In_ PRESOURCELIST ResourceList);
    /**
     * @brief Stops the transport and releases initialized resources.
     * @param DeviceObject Device object used for resource teardown, if available.
     */
    VOID
    StopTransport(_In_opt_ PDEVICE_OBJECT DeviceObject);
    /**
     * @brief Checks whether the transport is stopping or not started.
     * @return TRUE if playback must not use the transport.
     */
    BOOLEAN TransportStopping(VOID);
    /** @brief Signals that the playback worker has quiesced. */
    VOID PlaybackWorkerQuiesced(VOID);
    /** @} */

    /** @name Playback capability queries
     * @{
     */
    /**
     * @brief Retrieves the channel map for a playback channel count.
     * @param Channels Number of channels.
     * @param Positions Receives the channel positions.
     * @return Status of the query.
     */
    NTSTATUS
    GetPlaybackChannelMap(_In_ ULONG Channels, _Out_writes_bytes_(Channels) PUCHAR Positions);
    /**
     * @brief Retrieves the rates supported by a playback format.
     * @param Channels Number of channels.
     * @param BitsPerSample Sample width in bits.
     * @param Rates Receives the supported-rate bit mask.
     * @return Status of the query.
     */
    NTSTATUS
    GetPlaybackRates(_In_ ULONG Channels, _In_ ULONG BitsPerSample, _Out_ ULONGLONG *Rates);
    /** @} */

    /** @name Playback stream operations
     * @{
     */
    /**
     * @brief Opens and prepares a playback stream.
     * @param Channels Number of audio channels (1 or 2).
     * @param BitsPerSample Sample width in bits (16 or 32).
     * @param SampleRate Playback rate in hertz.
     * @param WakeEvent Event signaled when playback work is available.
     * @param Playback Receives the playback context.
     * @return Status of the operation.
     */
    NTSTATUS
    OpenPlayback(
        _In_ ULONG Channels,
        _In_ ULONG BitsPerSample,
        _In_ ULONG SampleRate,
        _In_ PKEVENT WakeEvent,
        _Out_ PVIRTIO_SND_PLAYBACK *Playback);
    /**
     * @brief Starts a prepared playback stream.
     * @param Playback Playback context to start.
     * @return Status of the operation.
     */
    NTSTATUS
    StartPlayback(_Inout_ PVIRTIO_SND_PLAYBACK Playback);
    /**
     * @brief Stops playback and drains outstanding periods.
     * @param Playback Playback context to stop.
     * @return Status of the operation.
     */
    NTSTATUS
    StopPlayback(_Inout_ PVIRTIO_SND_PLAYBACK Playback);
    /**
     * @brief Releases a playback context and its transfer slots.
     * @param Playback Playback context to close.
     */
    VOID
    ClosePlayback(_Inout_ PVIRTIO_SND_PLAYBACK Playback);
    /**
     * @brief Retrieves the next completed period's status and latency.
     * @param Playback Playback context to inspect.
     * @param DeviceStatus Receives the device completion status.
     * @param LatencyBytes Receives the reported latency in bytes.
     * @return Status of the operation.
     */
    NTSTATUS
    GetPlaybackCompletion(_Inout_ PVIRTIO_SND_PLAYBACK Playback, _Out_ PULONG DeviceStatus, _Out_ PULONG LatencyBytes);
    /**
     * @brief Processes pending playback events, including XRUN notifications.
     * @param Playback Playback context whose events are processed.
     * @param XrunCount Receives the number of XRUN events.
     * @return Status of the operation.
     */
    NTSTATUS
    ProcessPlaybackEvents(_Inout_ PVIRTIO_SND_PLAYBACK Playback, _Out_ PULONG XrunCount);
    /**
     * @brief Arms transmit-queue completion notifications.
     * @param Playback Playback context using the transmit queue.
     * @return TRUE if completion notifications were armed.
     */
    BOOLEAN
    ArmPlaybackCompletion(_Inout_ PVIRTIO_SND_PLAYBACK Playback);
    /**
     * @brief Checks whether a playback context is active on this adapter.
     * @param Playback Playback context to check.
     * @return TRUE if playback can continue.
     */
    BOOLEAN
    PlaybackIsActive(_In_ PVIRTIO_SND_PLAYBACK Playback);
    /**
     * @brief Queues one audio period for playback.
     * @param Playback Playback context receiving the period.
     * @param Audio Audio data to queue.
     * @param Length Length of the audio data in bytes.
     * @return Status of the operation.
     */
    NTSTATUS
    QueuePlayback(_Inout_ PVIRTIO_SND_PLAYBACK Playback, _In_reads_bytes_(Length) PVOID Audio, _In_ ULONG Length);
    /** @} */

    /** @name Master volume and mute controls
     * @{
     */
    /**
     * @brief Gets the master volume in hundredths of a decibel.
     * @return Current master volume.
     */
    LONG GetMasterVolume(VOID);
    /**
     * @brief Sets the master volume in hundredths of a decibel.
     * @param VolumeLevel Requested master volume.
     * @return Status of the operation.
     */
    NTSTATUS
    SetMasterVolume(_In_ LONG VolumeLevel);
    /**
     * @brief Gets the master mute state.
     * @return TRUE if muted, FALSE otherwise.
     */
    BOOLEAN GetMasterMute(VOID);
    /**
     * @brief Sets the master mute state.
     * @param Muted TRUE to mute, FALSE to unmute.
     */
    VOID
    SetMasterMute(_In_ BOOLEAN Muted);
    /**
     * @brief Applies the current master volume and mute settings to audio.
     * @param Audio Audio buffer to modify.
     * @param Length Length of the audio buffer in bytes.
     * @param BitsPerSample Sample width in bits.
     */
    VOID
    ApplyMasterControls(_Inout_updates_bytes_(Length) PVOID Audio, _In_ ULONG Length, _In_ ULONG BitsPerSample);
    /** @} */

    /** @name Interrupt handling
     * @{
     */
    /** @brief Wakes playback processing from the interrupt DPC. */
    VOID OnInterruptDpc(VOID);
    /** @brief Checks and acknowledges a device interrupt.
     * @return TRUE if the interrupt belongs to this device.
     */
    BOOLEAN OnInterruptService(VOID);
    /** @} */

  public:
    /** @name Transport state
     * @{
     */
    PDEVICE_OBJECT OwnerDeviceObject;            /**< Device object that owns this adapter. */
    VIRTIO_SND_TRANSPORT Transport;              /**< Captured PCI transport resources. */
    VirtIODevice Device;                         /**< VirtIO device transport instance. */
    VIRTIO_SND_CONFIG Config;                    /**< Device-reported sound capabilities. */
    ULONGLONG HostFeatures;                      /**< Features offered by the device. */
    ULONGLONG DriverFeatures;                    /**< Features accepted by the driver. */
    struct virtqueue *Queues[VIRTIO_SND_VQ_MAX]; /**< Device virtqueues. */
    PVIRTIO_SND_PCM_INFO PcmInfo;                /**< Discovered PCM stream capabilities. */
    ULONG PcmInfoCount;                          /**< Number of PCM capability entries. */
    PVIRTIO_SND_CHMAP_INFO ChannelMapInfo;       /**< Discovered channel maps. */
    ULONG ChannelMapInfoCount;                   /**< Number of channel map entries. */
    PVIRTIO_SND_EVENT_SLOT EventSlots;           /**< Buffers posted to the event queue. */
    ULONG EventSlotCount;                        /**< Number of allocated event slots. */
    PVOID OutstandingBuffer;                     /**< Contiguous buffer retained for a device request. */
    BOOLEAN DeviceInitializeAttempted;           /**< Whether device initialization was attempted. */
    BOOLEAN DeviceInitialized;                   /**< Whether device initialization succeeded. */
    BOOLEAN QueuesInitialized;                   /**< Whether virtqueues were initialized. */
    BOOLEAN Started;                             /**< Whether transport startup completed. */
    PKINTERRUPT InterruptObject;                 /**< Connected device interrupt. */
    KDPC InterruptDpc;                           /**< DPC queued for device interrupts. */
    KSPIN_LOCK PlaybackLock;                     /**< Protects playback interrupt signaling state. */
    PKEVENT PlaybackEvent;                       /**< Current playback worker's wake event. */
    KEVENT PlaybackQuiesced;                     /**< Signaled when playback work has quiesced. */
    KEVENT TransportStopped;                     /**< Signaled when transport shutdown has completed. */
    PVIRTIO_SND_PLAYBACK PlaybackContext;        /**< Current playback stream context. */
    volatile LONG Stopping;                      /**< Atomic transport-stopping flag. */
    LONG PlaybackOpen;                           /**< Interlocked guard for the single playback stream. */
    /** @} */

    /** @name Master volume and mute state
     * @{
     */
    volatile LONG MasterVolume; /**< Master volume in hundredths of a decibel. */
    volatile LONG MasterMute;   /**< Nonzero when master mute is enabled. */
    /** @} */
};

/*
 * The miniport factories retain C linkage because PortCls's
 * factory callbacks are C-style.
 */
extern "C"
{
    /**
     * @brief Installs the WaveCyclic render subdevice.
     * @param DeviceObject Owning device object.
     * @param Irp Start-device request being processed.
     * @param ResourceList PnP-assigned resources.
     * @param Adapter Per-device VirtIO adapter.
     * @return Status of the installation.
     */
    NTSTATUS
    VirtioSndInstallRenderSubdevice(
        _In_ PDEVICE_OBJECT DeviceObject,
        _In_ PIRP Irp,
        _In_ PRESOURCELIST ResourceList,
        _In_ CVirtioSndAdapter *Adapter);

    /**
     * @brief Installs the topology subdevice.
     * @param DeviceObject Owning device object.
     * @param Irp Start-device request being processed.
     * @param ResourceList PnP-assigned resources.
     * @param Adapter Per-device VirtIO adapter.
     * @param TopologyUnknown Receives the topology miniport interface.
     * @return Status of the installation.
     */
    NTSTATUS
    VirtioSndInstallTopologySubdevice(
        _In_ PDEVICE_OBJECT DeviceObject,
        _In_ PIRP Irp,
        _In_ PRESOURCELIST ResourceList,
        _In_ CVirtioSndAdapter *Adapter,
        _Out_ PUNKNOWN *TopologyUnknown);
}

#endif /* _VIRTIOSND_ADAPTER_HPP_ */
