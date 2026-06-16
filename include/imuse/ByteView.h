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
