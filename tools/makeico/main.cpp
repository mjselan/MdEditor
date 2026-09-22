// Generates resources/app.ico from the code-painted application icon.
// Writes classic uncompressed Windows ICO entries (BMP payloads), which every
// shell version renders. One-shot developer tool, not part of the shipped app.
#include "../../src/appicons.h"

#include <QGuiApplication>
#include <QImage>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

#pragma pack(push, 1)
struct IcoHeader
{
    uint16_t reserved = 0;
    uint16_t type = 1; // icon
    uint16_t count = 0;
};

struct IcoEntry
{
    uint8_t width = 0;  // 0 means 256
    uint8_t height = 0; // 0 means 256
    uint8_t colors = 0;
    uint8_t reserved = 0;
    uint16_t planes = 1;
    uint16_t bpp = 32;
    uint32_t bytes = 0;
    uint32_t offset = 0;
};
#pragma pack(pop)

// BITMAPINFOHEADER with doubled height (XOR + AND masks), bottom-up rows.
std::vector<uint8_t> bmpFor(const QImage &image)
{
    const int w = image.width();
    const int h = image.height();
    const int stride = ((w * 32 + 31) / 32) * 4;

    std::vector<uint8_t> out(40 + stride * h + stride * h, 0);
    auto put32 = [&out](size_t at, uint32_t v) {
        out[at] = v & 0xff;
        out[at + 1] = (v >> 8) & 0xff;
        out[at + 2] = (v >> 16) & 0xff;
        out[at + 3] = (v >> 24) & 0xff;
    };
    put32(0, 40);
    put32(4, uint32_t(w));
    put32(8, uint32_t(h * 2)); // XOR + AND
    put32(12, 1);
    put32(16, 32); // bpp

    // Bottom-up XOR (BGRA, premultiplied to satisfy some shells).
    QImage argb = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(argb.constScanLine(h - 1 - y));
        for (int x = 0; x < w; ++x) {
            const QRgb c = line[x];
            uint8_t *dst = out.data() + 40 + y * stride + x * 4;
            dst[0] = uint8_t(qBlue(c) * qAlpha(c) / 255);
            dst[1] = uint8_t(qGreen(c) * qAlpha(c) / 255);
            dst[2] = uint8_t(qRed(c) * qAlpha(c) / 255);
            dst[3] = uint8_t(qAlpha(c));
        }
    }
    // AND mask stays zero: alpha channel carries transparency.
    return out;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        qInfo("usage: makeico <output.ico>");
        return 2;
    }

    const int sizes[] = { 16, 24, 32, 48, 64, 128, 256 };

    std::vector<IcoEntry> entries(std::size(sizes));
    std::vector<std::vector<uint8_t>> bitmaps(std::size(sizes));

    size_t offset = sizeof(IcoHeader) + entries.size() * sizeof(IcoEntry);
    for (size_t i = 0; i < std::size(sizes); ++i) {
        bitmaps[i] = bmpFor(appicons::applicationIconPixmap(sizes[i])
                                .toImage()
                                .convertToFormat(QImage::Format_ARGB32));
        entries[i].width = uint8_t(sizes[i] == 256 ? 0 : sizes[i]);
        entries[i].height = uint8_t(sizes[i] == 256 ? 0 : sizes[i]);
        entries[i].bytes = uint32_t(bitmaps[i].size());
        entries[i].offset = uint32_t(offset);
        offset += bitmaps[i].size();
    }

    std::ofstream file(argv[1], std::ios::binary);
    if (!file) {
        qWarning("cannot open %s for writing", argv[1]);
        return 1;
    }

    IcoHeader header;
    header.count = uint16_t(entries.size());
    file.write(reinterpret_cast<const char *>(&header), sizeof(header));
    for (const auto &e : entries)
        file.write(reinterpret_cast<const char *>(&e), sizeof(e));
    for (const auto &bmp : bitmaps)
        file.write(reinterpret_cast<const char *>(bmp.data()), std::streamsize(bmp.size()));
    file.close();

    qInfo("wrote %s (%d sizes)", argv[1], int(std::size(sizes)));
    return 0;
}
