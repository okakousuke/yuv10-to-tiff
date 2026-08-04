#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint16_t read10(const std::uint8_t* p, int index)
{
    const std::uint32_t word =
        (static_cast<std::uint32_t>(p[0]) << 24) |
        (static_cast<std::uint32_t>(p[1]) << 16) |
        (static_cast<std::uint32_t>(p[2]) << 8) |
        static_cast<std::uint32_t>(p[3]);
    return static_cast<std::uint16_t>((word >> (22 - index * 10)) & 0x3ffU);
}

std::uint8_t clip8(double value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0) + 0.5);
}

void ycbcr_to_rgb(std::uint16_t y10, std::uint16_t cb10, std::uint16_t cr10,
                  std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    const double y = (static_cast<double>(y10) - 64.0) * 255.0 / 876.0;
    const double cb = (static_cast<double>(cb10) - 512.0) * 255.0 / 896.0;
    const double cr = (static_cast<double>(cr10) - 512.0) * 255.0 / 896.0;

    r = clip8(y + 1.5748 * cr);
    g = clip8(y - 0.1873 * cb - 0.4681 * cr);
    b = clip8(y + 1.8556 * cb);
}

void put16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void put32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 24));
}

void write_tiff(const std::string& path, std::uint32_t width, std::uint32_t height,
                const std::vector<std::uint8_t>& rgb)
{
    constexpr std::uint16_t entries = 10;
    constexpr std::uint32_t ifd_offset = 8;
    constexpr std::uint32_t bits_offset = ifd_offset + 2 + entries * 12 + 4;
    constexpr std::uint32_t pixel_offset = bits_offset + 6;

    std::vector<std::uint8_t> out;
    out.reserve(pixel_offset + rgb.size());
    out.push_back('I'); out.push_back('I');
    put16(out, 42);
    put32(out, ifd_offset);
    put16(out, entries);

    const auto entry = [&out](std::uint16_t tag, std::uint16_t type,
                              std::uint32_t count, std::uint32_t value) {
        put16(out, tag); put16(out, type); put32(out, count); put32(out, value);
    };
    entry(256, 4, 1, width);                 // ImageWidth
    entry(257, 4, 1, height);                // ImageLength
    entry(258, 3, 3, bits_offset);           // BitsPerSample
    entry(259, 3, 1, 1);                     // Compression: none
    entry(262, 3, 1, 2);                     // PhotometricInterpretation: RGB
    entry(273, 4, 1, pixel_offset);          // StripOffsets
    entry(277, 3, 1, 3);                     // SamplesPerPixel
    entry(278, 4, 1, height);                // RowsPerStrip
    entry(279, 4, 1, static_cast<std::uint32_t>(rgb.size())); // StripByteCounts
    entry(284, 3, 1, 1);                     // PlanarConfiguration: chunky
    put32(out, 0);                           // next IFD
    put16(out, 8); put16(out, 8); put16(out, 8);
    out.insert(out.end(), rgb.begin(), rgb.end());

    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open output: " + path);
    file.write(reinterpret_cast<const char*>(out.data()),
               static_cast<std::streamsize>(out.size()));
    if (!file) throw std::runtime_error("cannot write output: " + path);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 5) {
        std::cerr << "usage: yuv10_to_tiff INPUT OUTPUT WIDTH HEIGHT\n";
        return 2;
    }
    try {
        const std::string input_path = argv[1];
        const std::string output_path = argv[2];
        const auto width = static_cast<std::uint32_t>(std::stoul(argv[3]));
        const auto height = static_cast<std::uint32_t>(std::stoul(argv[4]));
        if (width == 0 || height == 0 || (width % 2) != 0) {
            throw std::runtime_error("width and height must be non-zero; width must be even");
        }

        std::ifstream file(input_path, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open input: " + input_path);
        const std::vector<std::uint8_t> input{
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        const std::size_t components = static_cast<std::size_t>(width) * height * 2;
        if (components % 3 != 0) {
            throw std::runtime_error("width * height * 2 must be divisible by 3 for this packing");
        }
        const std::size_t expected = components / 3 * 4;
        if (input.size() != expected) {
            throw std::runtime_error("input size is " + std::to_string(input.size()) +
                                     ", expected " + std::to_string(expected));
        }

        std::vector<std::uint8_t> rgb;
        rgb.reserve(static_cast<std::size_t>(width) * height * 3);
        for (std::size_t offset = 0; offset < input.size(); offset += 16) {
            std::uint16_t sample[12]{};
            for (int group = 0; group < 4; ++group) {
                for (int index = 0; index < 3; ++index) {
                    sample[group * 3 + index] = read10(input.data() + offset + group * 4, index);
                }
            }
            for (int i = 0; i < 12; i += 4) {
                const auto cb = sample[i];
                const auto cr = sample[i + 2];
                for (int pixel = 0; pixel < 2; ++pixel) {
                    std::uint8_t r, g, b;
                    ycbcr_to_rgb(sample[i + 1 + pixel * 2], cb, cr, r, g, b);
                    rgb.push_back(r); rgb.push_back(g); rgb.push_back(b);
                }
            }
        }
        write_tiff(output_path, width, height, rgb);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
