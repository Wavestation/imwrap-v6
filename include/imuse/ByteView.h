/* ==========================================================================
 *
 * iMWRAP V6 - A modern iMuse implementation attempt with Adventure Game Studio Companion Plugin
 *
 * This program is the legal property of Masami Komuro and few other contributors,
 * Please refer to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ========================================================================== */

#ifndef IMUSE_BYTE_VIEW_H
#define IMUSE_BYTE_VIEW_H

#include <cstddef>
#include <cstdint>

namespace imuse {

class ByteView {
public:
    constexpr ByteView() = default;
    constexpr ByteView(const uint8_t *data, std::size_t size) : _data(data), _size(size) {}

    constexpr const uint8_t *data() const { return _data; }
    constexpr std::size_t size() const { return _size; }
    constexpr bool empty() const { return _size == 0; }

    constexpr ByteView subview(std::size_t offset, std::size_t length) const {
        return (offset <= _size && length <= (_size - offset))
            ? ByteView(_data + offset, length)
            : ByteView();
    }

private:
    const uint8_t *_data = nullptr;
    std::size_t _size = 0;
};

} // namespace imuse

#endif
