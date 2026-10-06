#pragma once

#include <cstdint>
#include <vector>

namespace returnline {

class PixelBuffer {
public:
    static constexpr int Width = 640;
    static constexpr int Height = 360;

    PixelBuffer();
    void Clear(std::uint32_t argb);
    void FillRect(int x, int y, int width, int height, std::uint32_t argb);
    void MultiplyRect(int x, int y, int width, int height, std::uint8_t factor);
    void AddLightRect(int x, int y, int width, int height, std::uint8_t red,
                      std::uint8_t green, std::uint8_t blue);
    [[nodiscard]] const std::uint32_t* Data() const { return pixels_.data(); }
    [[nodiscard]] int PitchBytes() const { return Width * static_cast<int>(sizeof(std::uint32_t)); }

private:
    std::vector<std::uint32_t> pixels_;
};

} // namespace returnline
