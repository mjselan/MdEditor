// SPDX-License-Identifier: GPL-3.0-or-later
#include "appicons.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QPainterPath>
#include <QPixmap>
#include <QVector>

#include <iterator>

namespace appicons {

namespace {

// Render sizes: one high-resolution master per DPR bucket keeps QIcon from
// interpolating a single bitmap across DPIs.
constexpr int kBase = 24; // logical icon size on the toolbar

constexpr qreal kPenWidth = 1.7; // line weight in the 24x24 canvas

QColor accentFor(const QColor &background)
{
    return background.lightness() < 128 ? QColor(0xd6, 0xde, 0xeb) // pale slate
                                        : QColor(0x1f, 0x29, 0x33);
}

using Painter = void (*)(QPainter &);

// --- glyph painters (all in a 24x24 box) ------------------------------------

void paintNewFile(QPainter &p)
{
    p.drawLine(6, 3, 14, 3);
    p.drawLine(6, 3, 6, 21);
    p.drawLine(6, 21, 18, 21);
    p.drawLine(18, 21, 18, 11);
    p.drawLine(14, 3, 18, 8);
    p.drawLine(18, 8, 18, 10);
    p.drawLine(10, 9, 14, 9);
    p.drawLine(10, 12, 14, 12);
    p.drawLine(10, 15, 14, 15);
    // Folded corner
    p.drawLine(14, 3, 14, 7);
    p.drawLine(14, 7, 18, 7);
}

void paintOpen(QPainter &p)
{
    p.drawPolyline(QVector<QPoint>{ { 3, 6 }, { 9, 6 }, { 11, 8 }, { 18, 8 } });
    p.drawLine(3, 6, 3, 19);
    p.drawPolyline(QVector<QPoint>{ { 3, 19 }, { 6, 12 }, { 21, 12 }, { 18, 19 }, { 3, 19 } });
}

void paintSave(QPainter &p)
{
    p.drawPolyline(QVector<QPoint>{ { 4, 4 }, { 16, 4 }, { 20, 8 }, { 20, 20 }, { 4, 20 }, { 4, 4 } });
    p.drawRect(8, 4, 8, 5);
    p.drawRect(7, 13, 10, 7);
}

void paintExportHtml(QPainter &p)
{
    p.drawRect(4, 4, 16, 14);
    p.drawLine(4, 8, 20, 8);
    p.drawLine(7, 6, 7, 6); // dot
    p.drawLine(9, 12, 7, 14);
    p.drawLine(7, 14, 9, 16);
    p.drawLine(17, 12, 19, 14);
    p.drawLine(19, 14, 17, 16);
    p.drawLine(13, 11, 11, 17);
}

void paintExportPdf(QPainter &p)
{
    p.drawPolyline(QVector<QPoint>{ { 6, 3 }, { 14, 3 }, { 18, 7 }, { 18, 21 }, { 6, 21 }, { 6, 3 } });
    p.drawLine(14, 3, 14, 7);
    p.drawLine(14, 7, 18, 7);
    p.drawText(QRect(6, 12, 12, 8), Qt::AlignCenter, QStringLiteral("PDF"));
}

void paintPrint(QPainter &p)
{
    p.drawRect(7, 3, 10, 5);
    p.drawRect(4, 8, 16, 8);
    p.drawRect(7, 14, 10, 7);
    p.drawLine(17, 11, 17, 11); // indicator dot
}

void paintUndo(QPainter &p)
{
    p.drawPolyline(QVector<QPoint>{ { 8, 4 }, { 4, 8 }, { 8, 12 } });
    p.drawPolyline(QVector<QPoint>{ { 4, 8 }, { 13, 8 }, { 13, 8 } });
    p.drawArc(QRect(9, 6, 10, 12), 0, 180 * 16);
}

void paintRedo(QPainter &p)
{
    p.drawPolyline(QVector<QPoint>{ { 16, 4 }, { 20, 8 }, { 16, 12 } });
    p.drawPolyline(QVector<QPoint>{ { 20, 8 }, { 11, 8 }, { 11, 8 } });
    p.drawArc(QRect(5, 6, 10, 12), 0, 180 * 16);
}

void paintBold(QPainter &p)
{
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(17);
    p.setFont(f);
    p.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, QStringLiteral("B"));
}

void paintItalic(QPainter &p)
{
    QFont f = p.font();
    f.setItalic(true);
    f.setPixelSize(17);
    p.setFont(f);
    p.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, QStringLiteral("I"));
}

void paintStrikethrough(QPainter &p)
{
    p.drawLine(4, 12, 20, 12);
    p.drawArc(QRect(6, 4, 12, 12), 0, 180 * 16);
    p.drawArc(QRect(6, 8, 12, 12), 180 * 16, 180 * 16);
}

void paintH1(QPainter &p)
{
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(13);
    p.setFont(f);
    p.drawText(QRect(0, 0, 15, 24), Qt::AlignCenter, QStringLiteral("H"));
    p.drawText(QRect(13, 0, 11, 24), Qt::AlignCenter, QStringLiteral("1"));
}

void paintH2(QPainter &p)
{
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(13);
    p.setFont(f);
    p.drawText(QRect(0, 0, 15, 24), Qt::AlignCenter, QStringLiteral("H"));
    p.drawText(QRect(13, 0, 11, 24), Qt::AlignCenter, QStringLiteral("2"));
}

void paintH3(QPainter &p)
{
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(13);
    p.setFont(f);
    p.drawText(QRect(0, 0, 15, 24), Qt::AlignCenter, QStringLiteral("H"));
    p.drawText(QRect(13, 0, 11, 24), Qt::AlignCenter, QStringLiteral("3"));
}

void paintPlainParagraph(QPainter &p)
{
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(15);
    p.setFont(f);
    p.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, QStringLiteral("¶"));
}

void paintLink(QPainter &p)
{
    QPainterPath left;
    left.moveTo(10, 8);
    left.quadTo(4, 8, 4, 12);
    left.quadTo(4, 16, 10, 16);
    p.drawPath(left);
    QPainterPath right;
    right.moveTo(14, 8);
    right.quadTo(20, 8, 20, 12);
    right.quadTo(20, 16, 14, 16);
    p.drawPath(right);
    p.drawLine(8, 12, 16, 12);
}

void paintInlineCode(QPainter &p)
{
    p.drawRect(3, 8, 18, 8);
    p.drawPolyline(QVector<QPoint>{ { 9, 10 }, { 7, 12 }, { 9, 14 } });
    p.drawPolyline(QVector<QPoint>{ { 15, 10 }, { 17, 12 }, { 15, 14 } });
}

void paintCodeBlock(QPainter &p)
{
    p.drawRect(3, 4, 18, 16);
    p.drawPolyline(QVector<QPoint>{ { 9, 9 }, { 7, 12 }, { 9, 15 } });
    p.drawPolyline(QVector<QPoint>{ { 15, 9 }, { 17, 12 }, { 15, 15 } });
}

void paintBlockquote(QPainter &p)
{
    p.drawLine(4, 5, 4, 19);
    p.drawPolyline(QVector<QPoint>{ { 9, 8 }, { 20, 8 } });
    p.drawPolyline(QVector<QPoint>{ { 9, 12 }, { 20, 12 } });
    p.drawPolyline(QVector<QPoint>{ { 9, 16 }, { 16, 16 } });
}

void paintBulletList(QPainter &p)
{
    // Zero-length round-cap lines render as crisp dots at any scale.
    p.drawLine(6, 6, 6, 6);
    p.drawLine(6, 12, 6, 12);
    p.drawLine(6, 16, 6, 16);
    p.drawLine(10, 6, 20, 6);
    p.drawLine(10, 12, 20, 12);
    p.drawLine(10, 16, 20, 16);
}

void paintNumberedList(QPainter &p)
{
    p.drawLine(10, 5, 20, 5);
    p.drawLine(10, 12, 20, 12);
    p.drawLine(10, 19, 20, 19);
    p.drawText(QRect(3, 1, 6, 8), Qt::AlignCenter, QStringLiteral("1"));
    p.drawText(QRect(3, 8, 6, 8), Qt::AlignCenter, QStringLiteral("2"));
    p.drawText(QRect(3, 15, 6, 8), Qt::AlignCenter, QStringLiteral("3"));
}

constexpr struct {
    Icon id;
    Painter paint;
} kGlyphs[] = {
    { Icon::NewFile, paintNewFile },         { Icon::Open, paintOpen },
    { Icon::Save, paintSave },               { Icon::ExportHtml, paintExportHtml },
    { Icon::ExportPdf, paintExportPdf },     { Icon::Print, paintPrint },
    { Icon::Undo, paintUndo },               { Icon::Redo, paintRedo },
    { Icon::Bold, paintBold },               { Icon::Italic, paintItalic },
    { Icon::Strikethrough, paintStrikethrough },
    { Icon::H1, paintH1 },                   { Icon::H2, paintH2 },
    { Icon::H3, paintH3 },                   { Icon::PlainParagraph, paintPlainParagraph },
    { Icon::Link, paintLink },               { Icon::InlineCode, paintInlineCode },
    { Icon::CodeBlock, paintCodeBlock },     { Icon::Blockquote, paintBlockquote },
    { Icon::BulletList, paintBulletList },   { Icon::NumberedList, paintNumberedList },
};

QHash<int, QIcon> &cache()
{
    static QHash<int, QIcon> cache;
    return cache;
}

int cacheKey(Icon which, const QColor &background)
{
    return int(which) * 2 + (background.lightness() < 128 ? 1 : 0);
}

} // namespace

QIcon makeIcon(Icon which, const QColor &background)
{
    const QColor accent = accentFor(background);
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 64 }) {
        QPixmap pm(size * 2, size * 2);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.scale(size / qreal(kBase), size / qreal(kBase));
        QPen pen(accent, kPenWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setFont([] {
            QFont f;
            f.setPixelSize(kBase);
            f.setFamily(QStringLiteral("Segoe UI"));
            return f;
        }());
        for (const auto &glyph : kGlyphs) {
            if (glyph.id == which) {
                glyph.paint(p);
                break;
            }
        }
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

void refresh(const QColor &background)
{
    cache().clear();
    for (int i = 0; i < int(std::size(kGlyphs)); ++i)
        cache().insert(cacheKey(Icon(i), background), makeIcon(Icon(i), background));
}

QIcon iconFor(Icon which)
{
    if (cache().isEmpty())
        refresh(QGuiApplication::palette().window().color());
    const QColor bg = QGuiApplication::palette().window().color();
    const int key = cacheKey(which, bg);
    if (!cache().contains(key))
        cache().insert(key, makeIcon(which, bg));
    return cache().value(key);
}

// --- application brand icon --------------------------------------------------

QPixmap applicationIconPixmap(int size)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal s = size / 24.0; // design on a 24x24 grid
    p.scale(s, s);

    // Rounded-square badge: indigo -> deep purple vertical gradient.
    const QRectF badge(0.75, 0.75, 22.5, 22.5);
    QLinearGradient grad(badge.topLeft(), badge.bottomLeft());
    grad.setColorAt(0.0, QColor(0x53, 0x61, 0xe8)); // indigo
    grad.setColorAt(1.0, QColor(0x7b, 0x3f, 0xd4)); // purple
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawRoundedRect(badge, 5.0, 5.0);

    // Down-arrow page fold (markdown's signature angle) across the badge.
    QPen arrow(QColor(255, 255, 255, 235), 2.0);
    arrow.setCapStyle(Qt::RoundCap);
    arrow.setJoinStyle(Qt::RoundJoin);
    p.setPen(arrow);
    p.setBrush(Qt::NoBrush);

    // The markdown mark: bold white "M" with the trailing stroke dipping
    // down like the markdown logo's arrow.
    p.drawLine(5.0, 16.0, 5.0, 8.5);
    p.drawPolyline(QVector<QPointF>{ { 5.0, 8.5 }, { 9.5, 14.0 }, { 14.0, 8.5 } });
    p.drawLine(14.0, 8.5, 14.0, 16.0);
    // The downward tail: vertical stub then arrowhead to the lower right.
    p.drawLine(16.5, 9.0, 16.5, 15.0);
    p.drawPolyline(QVector<QPointF>{ { 13.8, 12.6 }, { 16.5, 15.6 }, { 19.2, 12.6 } });
    p.end();
    return pm;
}

QIcon applicationIcon()
{
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 64, 128, 256 }) {
        QPixmap pm = applicationIconPixmap(size * 2);
        pm.setDevicePixelRatio(2.0);
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace appicons
