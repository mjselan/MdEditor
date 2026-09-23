// SPDX-License-Identifier: GPL-3.0-or-later
// Generates a macOS .icns file from the code-painted application icon.
// The build uses this only on Apple platforms; the shipped app still keeps
// its artwork in C++ and therefore has no runtime asset dependency.
#include "../../src/appicons.h"

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QImage>

namespace {

void appendBigEndian32(QByteArray &out, quint32 value)
{
    out.append(char((value >> 24) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char(value & 0xff));
}

QByteArray pngData(int size)
{
    const QImage image = appicons::applicationIconPixmap(size)
                             .toImage()
                             .convertToFormat(QImage::Format_ARGB32);
    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};
    return data;
}

struct IconEntry
{
    const char *type;
    int size;
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc != 2) {
        qInfo("usage: makeicns <output.icns>");
        return 2;
    }

    // Modern ICNS types contain PNG data.  Keeping several sizes makes the
    // same icon usable in Finder, the Dock, Launchpad, and the IFW wizard.
    const IconEntry entries[] = {
        { "icp4", 16 },
        { "icp5", 32 },
        { "icp6", 64 },
        { "ic07", 128 },
        { "ic08", 256 },
        { "ic09", 512 },
        { "ic10", 1024 },
    };

    QByteArray body;
    for (const IconEntry &entry : entries) {
        const QByteArray png = pngData(entry.size);
        if (png.isEmpty()) {
            qWarning("could not encode %dx%d PNG", entry.size, entry.size);
            return 1;
        }
        body.append(entry.type, 4);
        appendBigEndian32(body, quint32(png.size() + 8));
        body.append(png);
    }

    QByteArray icns;
    icns.append("icns", 4);
    appendBigEndian32(icns, quint32(body.size() + 8));
    icns.append(body);

    QFile output(argv[1]);
    if (!output.open(QIODevice::WriteOnly) || output.write(icns) != icns.size()) {
        qWarning("cannot write %s", argv[1]);
        return 1;
    }
    output.close();

    qInfo("wrote %s (%d sizes)", argv[1], int(sizeof(entries) / sizeof(entries[0])));
    return 0;
}
