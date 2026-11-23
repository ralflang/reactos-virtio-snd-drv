/*
 * PROJECT:     ReactOS VirtIO Sound Driver
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Driver entry-point declarations
 * COPYRIGHT:   Copyright 2026 Ralf Lang <ralf.lang@ralf-lang.de>
 */

#ifndef _VIRTIOSND_H_
#define _VIRTIOSND_H_

#include <ntddk.h>
#include <portcls.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initializes the VirtIO sound device and registers its subdevices.
     * @param DeviceObject Device object being started.
     * @param Irp Start-device request being processed.
     * @param ResourceList PnP-assigned resources.
     * @return Status of the operation.
     */
    NTSTATUS
    NTAPI
    VirtioSndStartDevice(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp, _In_ PRESOURCELIST ResourceList);

#ifdef __cplusplus
}
#endif

#endif /* _VIRTIOSND_H_ */
