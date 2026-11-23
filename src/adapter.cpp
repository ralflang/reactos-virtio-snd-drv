/*
 * PROJECT:     ReactOS VirtIO Sound Driver
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     PnP handling and adapter power management
 * COPYRIGHT:   Copyright 2026 Ralf Lang <ralf.lang@ralf-lang.de>
 */

#include "adapter.hpp"

#define VIRTIO_SND_ADAPTER_POOL_TAG 'aSnV'

PVOID
__cdecl
operator new(size_t Size, POOL_TYPE PoolType, ULONG Tag);

CVirtioSndAdapter::CVirtioSndAdapter()
    : OwnerDeviceObject(NULL), HostFeatures(0), DriverFeatures(0), PcmInfo(NULL), PcmInfoCount(0), ChannelMapInfo(NULL),
      ChannelMapInfoCount(0), EventSlots(NULL), EventSlotCount(0), OutstandingBuffer(NULL),
      DeviceInitializeAttempted(FALSE), DeviceInitialized(FALSE), QueuesInitialized(FALSE), Started(FALSE),
      InterruptObject(NULL), PlaybackEvent(NULL), PlaybackContext(NULL), Stopping(FALSE), PlaybackOpen(0),
      MasterVolume(0), MasterMute(FALSE)
{
    RtlZeroMemory(&Transport, sizeof(Transport));
    RtlZeroMemory(&Device, sizeof(Device));
    RtlZeroMemory(&Config, sizeof(Config));
    RtlZeroMemory(Queues, sizeof(Queues));
    KeInitializeSpinLock(&PlaybackLock);
    KeInitializeEvent(&PlaybackQuiesced, NotificationEvent, TRUE);
    KeInitializeEvent(&TransportStopped, NotificationEvent, TRUE);
}

/**
 * @brief Registers the per device adapter for power management.
 *
 * VirtIO doesn't really do power management but exposes the capability.
 * At the moment it's a no-op but we might save some CPU cycles if we stop cycling an all-zero buffer in "power safe"
 * states.
 */
static NTSTATUS
VirtioSndRegisterPowerManagement(_In_ PDEVICE_OBJECT DeviceObject, _In_ CVirtioSndAdapter *Adapter)
{
    PAGED_CODE();

    return PcRegisterAdapterPowerManagement(static_cast<IAdapterPowerManagement *>(Adapter), DeviceObject);
}

/**
 * @brief Starts the device's transport and registers its WaveCyclic miniports.
 */
extern "C" NTSTATUS NTAPI
VirtioSndStartDevice(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp, _In_ PRESOURCELIST ResourceList)
{
    NTSTATUS Status;
    ULONG ResourceCount;
    PVIRTIO_SND_DEVICE_EXTENSION DeviceExtension;
    CVirtioSndAdapter *Adapter;

    PAGED_CODE();

    if (DeviceObject == NULL || Irp == NULL || ResourceList == NULL)
    {
        DbgPrint("virtiosnd: StartDevice received incomplete PnP state\n");
        return STATUS_INVALID_PARAMETER;
    }

    DeviceExtension = static_cast<PVIRTIO_SND_DEVICE_EXTENSION>(DeviceObject->DeviceExtension);
    if (DeviceExtension == NULL)
        return STATUS_INVALID_DEVICE_STATE;
    DeviceExtension->Adapter = NULL;

    ResourceCount = ResourceList->NumberOfEntries();
    if (ResourceCount == 0)
    {
        DbgPrint("virtiosnd: PnP matched the device but assigned no resources\n");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Adapter = new (NonPagedPool, VIRTIO_SND_ADAPTER_POOL_TAG) CVirtioSndAdapter();
    if (Adapter == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

    Adapter->AddRef(); // The device extension owns the adapter until PnP teardown.
    DeviceExtension->Adapter = Adapter;

    Status = Adapter->StartTransport(DeviceObject, ResourceList);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("virtiosnd: StartDevice transport probe failed with status %08lx\n", (ULONG)Status);
        goto Failure;
    }

    Status = VirtioSndRegisterPowerManagement(DeviceObject, Adapter);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("virtiosnd: adapter power-management registration failed with status %08lx\n", (ULONG)Status);
        goto Failure;
    }

    Status = VirtioSndInstallRenderSubdevice(DeviceObject, Irp, ResourceList, Adapter);
    if (!NT_SUCCESS(Status))
    {
        DbgPrint("virtiosnd: WaveCyclic render subdevice registration failed with status %08lx\n", (ULONG)Status);
        goto Failure;
    }

    return STATUS_SUCCESS;

Failure:
    // Unwind partial transport setup before releasing the extension's reference.
    Adapter->StopTransport(DeviceObject);
    DeviceExtension->Adapter = NULL;
    Adapter->Release();
    return Status;
}
