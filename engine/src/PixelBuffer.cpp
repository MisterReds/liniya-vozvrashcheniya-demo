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

void PixelBuffer::MultiplyRect(int x, int y, int width, int height, const std::uint8_t factor) {
    const int left = std::clamp(x, 0, Width);
    const int top = std::clamp(y, 0, Height);
    const int right = std::clamp(x + width, 0, Width);
    const int bottom = std::clamp(y + height, 0, Height);
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            auto& pixel = pixels_[static_cast<std::size_t>(row * Width + column)];
            const auto red = ((pixel >> 16u) & 0xffu) * factor / 255u;
            const auto green = ((pixel >> 8u) & 0xffu) * factor / 255u;
            const auto blue = (pixel & 0xffu) * factor / 255u;
            pixel = 0xff000000u | (red << 16u) | (green << 8u) | blue;
        }
    }
}

void PixelBuffer::AddLightRect(int x, int y, int width, int height, const std::uint8_t red,
                               const std::uint8_t green, const std::uint8_t blue) {
    const int left = std::clamp(x, 0, Width);
    const int top = std::clamp(y, 0, Height);
    const int right = std::clamp(x + width, 0, Width);
    const int bottom = std::clamp(y + height, 0, Height);
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            auto& pixel = pixels_[static_cast<std::size_t>(row * Width + column)];
            const auto r = std::min(255u, ((pixel >> 16u) & 0xffu) + red);
            const auto g = std::min(255u, ((pixel >> 8u) & 0xffu) + green);
            const auto b = std::min(255u, (pixel & 0xffu) + blue);
            pixel = 0xff000000u | (r << 16u) | (g << 8u) | b;
        }
    }
}

} // namespace returnline
