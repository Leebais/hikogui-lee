// Copyright 2019 Pokitec
// All rights reserved.

#include "audio_device_win32.hpp"
#include "../logger.hpp"
#include "../strings.hpp"
#include "../exceptions.hpp"
#include <Windows.h>
#include <propsys.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

namespace tt {

static std::string getStringProperty(void *propertyStore, REFPROPERTYKEY key)
{
    auto propertyStore_ = static_cast<IPropertyStore *>(propertyStore);
    tt_assert(propertyStore_ != nullptr);

    PROPVARIANT textProperty;
    PropVariantInit(&textProperty);

    hresult_assert_or_throw(propertyStore_->GetValue(PKEY_Device_FriendlyName, &textProperty));
    auto textWString = std::wstring_view(textProperty.pwszVal);
    auto textString = to_string(textWString);

    PropVariantClear(&textProperty);
    return textString;
}

audio_device_win32::audio_device_win32(void *device) :
    audio_device(), _device(device)
{
    tt_assert(device != nullptr);

    id = get_id_from_device(device);
    
    auto device_ = static_cast<IMMDevice *>(device);
    
    hresult_assert_or_throw(device_->OpenPropertyStore(STGM_READ, reinterpret_cast<IPropertyStore **>(&_property_store)));
}

audio_device_win32::~audio_device_win32()
{
    auto propertyStore_ = static_cast<IPropertyStore *>(_property_store);
    tt_assert(propertyStore_ != nullptr);
    propertyStore_->Release();

    auto device_ = static_cast<IMMDevice *>(_device);
    tt_assert(device_ != nullptr);
    device_->Release();
}

std::string audio_device_win32::name() const noexcept
{
    return getStringProperty(_property_store, PKEY_Device_FriendlyName);
}

std::string audio_device_win32::device_name() const noexcept
{
    return getStringProperty(_property_store, PKEY_DeviceInterface_FriendlyName);
}

std::string audio_device_win32::end_point_name() const noexcept
{
    return getStringProperty(_property_store, PKEY_Device_DeviceDesc);
}

audio_device_state audio_device_win32::state() const noexcept
{
    auto device_ = static_cast<IMMDevice *>(_device);

    DWORD state;
    hresult_assert_or_throw(device_->GetState(&state));

    switch (state) {
    case DEVICE_STATE_ACTIVE:
        return audio_device_state::active;
    case DEVICE_STATE_DISABLED:
        return audio_device_state::disabled;
    case DEVICE_STATE_NOTPRESENT:
        return audio_device_state::not_present;
    case DEVICE_STATE_UNPLUGGED:
        return audio_device_state::unplugged;
    default:
        tt_no_default();
    }
}

std::string audio_device_win32::get_id_from_device(void *device) noexcept
{
    auto device_ = static_cast<IMMDevice *>(device);

    // Get the cross-reboot-unique-id-string of the device.
    LPWSTR id_wcharstr;
    tt_assert(device_ != nullptr);
    hresult_assert_or_throw(device_->GetId(&id_wcharstr));

    ttlet id_wstring = std::wstring_view(id_wcharstr);
    auto id = to_string(id_wstring);
    CoTaskMemFree(id_wcharstr);
    return id;
}

}