// Copyright Take Vos 2019-2021.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)

#include "win32_headers.hpp"

#include "file_view.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "memory.hpp"
#include "URL.hpp"
#include "required.hpp"
#include <mutex>

namespace hi::inline v1 {

file_view::file_view(std::shared_ptr<file_mapping> const &_file_mapping_object, std::size_t offset, std::size_t size) :
    _file_mapping_object(_file_mapping_object), _offset(offset)
{
    if (size == 0) {
        size = _file_mapping_object->size - _offset;
    }
    hi_assert(_offset + size <= _file_mapping_object->size);

    DWORD desiredAccess;
    if (any(accessMode() & access_mode::read) and any(accessMode() & access_mode::write)) {
        desiredAccess = FILE_MAP_WRITE;
    } else if (any(accessMode() & access_mode::read)) {
        desiredAccess = FILE_MAP_READ;
    } else {
        throw io_error(std::format("{}: Illegal access mode WRONLY/0 when viewing file.", location()));
    }

    DWORD fileOffsetHigh = _offset >> 32;
    DWORD fileOffsetLow = _offset & 0xffffffff;

    void *data;
    if (size == 0) {
        data = nullptr;
    } else {
        if ((data = MapViewOfFile(_file_mapping_object->mapHandle, desiredAccess, fileOffsetHigh, fileOffsetLow, size)) == NULL) {
            throw io_error(std::format("{}: Could not map view of file. '{}'", location(), get_last_error_message()));
        }
    }

    auto *bytes_ptr = new void_span(data, size);
    _bytes = std::shared_ptr<void_span>(bytes_ptr, file_view::unmap);
}

file_view::file_view(URL const &location, access_mode accessMode, std::size_t offset, std::size_t size) :
    file_view(findOrCreateFileMappingObject(location, accessMode, offset + size), offset, size)
{
}

file_view::file_view(file_view const &other) noexcept :
    _file_mapping_object(other._file_mapping_object), _bytes(other._bytes), _offset(other._offset)
{
    hi_axiom(&other != this);
}

file_view &file_view::operator=(file_view const &other) noexcept
{
    hi_return_on_self_assignment(other);
    _file_mapping_object = other._file_mapping_object;
    _offset = other._offset;
    _bytes = other._bytes;
    return *this;
}

file_view::file_view(file_view &&other) noexcept :
    _file_mapping_object(std::move(other._file_mapping_object)), _bytes(std::move(other._bytes)), _offset(other._offset)
{
    hi_axiom(&other != this);
}

file_view &file_view::operator=(file_view &&other) noexcept
{
    hi_return_on_self_assignment(other);
    _file_mapping_object = std::move(other._file_mapping_object);
    _offset = other._offset;
    _bytes = std::move(other._bytes);
    return *this;
}

void file_view::unmap(void_span *bytes) noexcept
{
    if (bytes != nullptr) {
        if (bytes->size() > 0) {
            void *data = bytes->data();
            if (!UnmapViewOfFile(data)) {
                hi_log_error("Could not unmap view on file '{}'", get_last_error_message());
            }
        }
        delete bytes;
    }
}

void file_view::flush(void *base, std::size_t size)
{
    if (!FlushViewOfFile(base, size)) {
        throw io_error(std::format("{}: Could not flush file. '{}'", location(), get_last_error_message()));
    }
}

} // namespace hi::inline v1
