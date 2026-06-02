#include "lunacube.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <system_error>

namespace lunalite::asset {
namespace {
constexpr std::array<char, 8> LunaCubeMagic{'L', 'U', 'N', 'A', 'C', 'U', 'B', 'E'};
constexpr uint32_t LunaCubeHeaderSize = 32;

uint32_t calculateMaxMipCount(uint32_t size)
{
    uint32_t mipCount = 0;
    while (size > 0) {
        ++mipCount;
        size >>= 1;
    }
    return mipCount;
}

void writeUInt32(std::ostream& out, uint32_t value)
{
    const char bytes[] = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8u) & 0xffu),
        static_cast<char>((value >> 16u) & 0xffu),
        static_cast<char>((value >> 24u) & 0xffu),
    };
    out.write(bytes, sizeof(bytes));
}

bool readUInt32(std::istream& in, uint32_t& value)
{
    std::array<unsigned char, 4> bytes{};
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in.good()) {
        return false;
    }

    value = static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8u) |
            (static_cast<uint32_t>(bytes[2]) << 16u) | (static_cast<uint32_t>(bytes[3]) << 24u);
    return true;
}

bool isValidImage(const LunaCubeImage& image)
{
    const auto payloadSize = calculateLunaCubePayloadSize(image.format, image.size, image.mip_count);
    return payloadSize && image.pixels.size() == *payloadSize;
}
} // namespace

uint32_t getLunaCubeBytesPerPixel(LunaCubeFormat format)
{
    switch (format) {
        case LunaCubeFormat::RGBA32F:
            return 4 * sizeof(float);
        default:
            return 0;
    }
}

uint32_t getLunaCubeMipSize(const LunaCubeImage& image, uint32_t mip)
{
    if (image.size == 0 || mip >= image.mip_count) {
        return 0;
    }

    return std::max(1u, image.size >> mip);
}

std::optional<LunaCubeImage> createLunaCubeImage(LunaCubeFormat format, uint32_t size, uint32_t mip_count)
{
    const auto payloadSize = calculateLunaCubePayloadSize(format, size, mip_count);
    if (!payloadSize) {
        return std::nullopt;
    }

    LunaCubeImage image;
    image.format = format;
    image.size = size;
    image.mip_count = mip_count;
    image.pixels.resize(*payloadSize);
    return image;
}

std::optional<size_t> calculateLunaCubePayloadSize(LunaCubeFormat format, uint32_t size, uint32_t mip_count)
{
    const auto bytesPerPixel = getLunaCubeBytesPerPixel(format);
    if (bytesPerPixel == 0 || size == 0 || mip_count == 0 || mip_count > calculateMaxMipCount(size)) {
        return std::nullopt;
    }

    uint64_t totalBytes = 0;
    for (uint32_t mip = 0; mip < mip_count; ++mip) {
        const auto mipSize = std::max(1u, size >> mip);
        totalBytes += static_cast<uint64_t>(LunaCubeFaceCount) * mipSize * mipSize * bytesPerPixel;
        if (totalBytes > std::numeric_limits<size_t>::max()) {
            return std::nullopt;
        }
    }

    return static_cast<size_t>(totalBytes);
}

std::optional<size_t> calculateLunaCubeFaceOffsetBytes(const LunaCubeImage& image, uint32_t mip, uint32_t face)
{
    if (face >= LunaCubeFaceCount || mip >= image.mip_count) {
        return std::nullopt;
    }

    const auto bytesPerPixel = getLunaCubeBytesPerPixel(image.format);
    const auto payloadSize = calculateLunaCubePayloadSize(image.format, image.size, image.mip_count);
    if (bytesPerPixel == 0 || !payloadSize) {
        return std::nullopt;
    }

    size_t offset = 0;
    for (uint32_t mipIndex = 0; mipIndex < mip; ++mipIndex) {
        const auto mipSize = std::max(1u, image.size >> mipIndex);
        offset += static_cast<size_t>(LunaCubeFaceCount) * mipSize * mipSize * bytesPerPixel;
    }

    const auto mipSize = std::max(1u, image.size >> mip);
    offset += static_cast<size_t>(face) * mipSize * mipSize * bytesPerPixel;
    if (offset >= *payloadSize) {
        return std::nullopt;
    }

    return offset;
}

const uint8_t* getLunaCubeFaceData(const LunaCubeImage& image, uint32_t mip, uint32_t face)
{
    const auto offset = calculateLunaCubeFaceOffsetBytes(image, mip, face);
    if (!offset || *offset >= image.pixels.size()) {
        return nullptr;
    }

    return image.pixels.data() + *offset;
}

uint8_t* getLunaCubeFaceData(LunaCubeImage& image, uint32_t mip, uint32_t face)
{
    const auto offset = calculateLunaCubeFaceOffsetBytes(image, mip, face);
    if (!offset || *offset >= image.pixels.size()) {
        return nullptr;
    }

    return image.pixels.data() + *offset;
}

bool writeLunaCube(const std::filesystem::path& path, const LunaCubeImage& image)
{
    if (!isValidImage(image)) {
        return false;
    }

    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    out.write(LunaCubeMagic.data(), static_cast<std::streamsize>(LunaCubeMagic.size()));
    writeUInt32(out, LunaCubeVersion);
    writeUInt32(out, LunaCubeHeaderSize);
    writeUInt32(out, static_cast<uint32_t>(image.format));
    writeUInt32(out, image.size);
    writeUInt32(out, image.mip_count);
    writeUInt32(out, LunaCubeFaceCount);
    out.write(reinterpret_cast<const char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
    return out.good();
}

std::optional<LunaCubeImage> readLunaCube(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }

    std::array<char, LunaCubeMagic.size()> magic{};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!in.good() || magic != LunaCubeMagic) {
        return std::nullopt;
    }

    uint32_t version = 0;
    uint32_t headerSize = 0;
    uint32_t formatValue = 0;
    uint32_t size = 0;
    uint32_t mipCount = 0;
    uint32_t faceCount = 0;
    if (!readUInt32(in, version) || !readUInt32(in, headerSize) || !readUInt32(in, formatValue) ||
        !readUInt32(in, size) || !readUInt32(in, mipCount) || !readUInt32(in, faceCount)) {
        return std::nullopt;
    }

    if (version != LunaCubeVersion || headerSize != LunaCubeHeaderSize || faceCount != LunaCubeFaceCount) {
        return std::nullopt;
    }

    const auto format = static_cast<LunaCubeFormat>(formatValue);
    const auto payloadSize = calculateLunaCubePayloadSize(format, size, mipCount);
    if (!payloadSize) {
        return std::nullopt;
    }

    std::error_code error;
    const auto fileSize = std::filesystem::file_size(path, error);
    if (error || fileSize != static_cast<uintmax_t>(LunaCubeHeaderSize) + static_cast<uintmax_t>(*payloadSize)) {
        return std::nullopt;
    }

    LunaCubeImage image;
    image.format = format;
    image.size = size;
    image.mip_count = mipCount;
    image.pixels.resize(*payloadSize);
    in.read(reinterpret_cast<char*>(image.pixels.data()), static_cast<std::streamsize>(image.pixels.size()));
    if (!in.good()) {
        return std::nullopt;
    }

    return image;
}
} // namespace lunalite::asset
