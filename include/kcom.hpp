/*
 * PROJECT:     ReactOS VirtIO Sound Driver
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Reference counting base class template for PortCls miniports
 * COPYRIGHT:   Copyright 2026 Ralf Lang <ralf.lang@ralf-lang.de>
 */

#ifndef _VIRTIOSND_KCOM_HPP_
#define _VIRTIOSND_KCOM_HPP_

#include <stdunk.h>

/**
 * @brief Reference counting template for the driver's PortCls miniports.
 *
 * PortCls hands its miniports around as COM objects.
 * Each must provide AddRef/Release to track how many references exist and free itself when the
 * count reaches zero. This templates avoids repetition of this logic across miniports.
 */
template <typename... Interfaces> class CUnknownImpl : public Interfaces...
{
  public:
    /** @brief Increments and returns the object's reference count. */
    STDMETHODIMP_(ULONG)
    AddRef()
    {
        return InterlockedIncrement(&m_Ref);
    }

    /**
     * @brief Decrements the reference count and deletes the object at zero.
     * @return Remaining reference count.
     */
    STDMETHODIMP_(ULONG)
    Release()
    {
        LONG Ref = InterlockedDecrement(&m_Ref);
        if (Ref == 0)
            delete this;
        return Ref;
    }

    /** @brief Destroys the reference-counted object. */
    virtual ~CUnknownImpl() = default;

  protected:
    /** Current COM-style reference count. */
    volatile LONG m_Ref = 0;
};

#endif /* _VIRTIOSND_KCOM_HPP_ */
