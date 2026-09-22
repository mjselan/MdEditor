#include "markdownpreview.h"

#include <QDir>
#include <QImage>
#include <QScrollBar>
#include <QUrl>
#include <QVariant>

MarkdownPreview::MarkdownPreview(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenExternalLinks(true);
    setFrameShape(QTextBrowser::NoFrame);

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &MarkdownPreview::renderNow);

    connect(verticalScrollBar(), &QAbstractSlider::valueChanged, this, [this](int value) {
        const int max = verticalScrollBar()->maximum();
        const double ratio = max > 0 ? double(value) / double(max) : 0.0;
        m_lastRatio = ratio;
        emit scrollRatioChanged(ratio);
    });
}

void MarkdownPreview::setSourceMarkdown(const QString &markdown)
{
    m_sourceDocument.clear();
    m_pendingMarkdown = markdown;
    m_debounce.start();
}

void MarkdownPreview::setSourceDocument(QTextDocument *document)
{
    m_sourceDocument = document;
}

void MarkdownPreview::scheduleRender()
{
    if (m_debounce.isActive())
        return; // already pending; keep the original deadline
    m_debounce.start();
}

void MarkdownPreview::setDocumentDirectory(const QString &dir)
{
    m_documentDir = dir;
}

void MarkdownPreview::renderNow()
{
    m_debounce.stop();
    if (m_sourceDocument)
        m_pendingMarkdown = m_sourceDocument->toPlainText(); // pull once, lazily
    document()->setMarkdown(m_pendingMarkdown);
    emit documentRendered();
    // setMarkdown() resets scrolling; restore the previous position silently.
    setScrollRatio(m_lastRatio);
}

void MarkdownPreview::setScrollRatio(double ratio)
{
    QScrollBar *bar = verticalScrollBar();
    const QSignalBlocker blocker(bar); // keep editor sync from echoing back
    bar->setValue(qRound(ratio * double(bar->maximum())));
}

QVariant MarkdownPreview::loadResource(int type, const QUrl &name)
{
    if (type == QTextDocument::ImageResource && name.isRelative() && !m_documentDir.isEmpty()) {
        const QString path = m_documentDir + QLatin1Char('/')
                             + name.toString(QUrl::FullyDecoded);
        const QImage image(path);
        if (!image.isNull()) {
            document()->addResource(QTextDocument::ImageResource, name, image);
            return image;
        }
    }
    return QTextBrowser::loadResource(type, name);
}
