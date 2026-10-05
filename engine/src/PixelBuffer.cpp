#include "returnline/PixelBuffer.hpp"

#include <algorithm>

namespace returnline {

PixelBuffer::PixelBuffer() : pixels_(Width * Height, 0xff000000u) {}

void PixelBuffer::Clear(const std::uint32_t argb) {
    std::fill(pixels_.begin(), pixels_.end(), argb);
}

void PixelBuffer::FillRect(int x, int y, int width, int height, const std::uint32_t argb) {
    const int left = std::clamp(x, 0, Width);
    const int top = std::clamp(y, 0, Height);
    const int right = std::clamp(x + width, 0, Width);
    const int bottom = std::clamp(y + height, 0, Height);
    if (left >= right || top >= bottom) return;

    for (int row = top; row < bottom; ++row) {
        auto begin = pixels_.begin() + row * Width + left;
        std::fill(begin, begin + (right - left), argb);
    }
}

} // namespace returnline
