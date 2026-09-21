#pragma once

#include <QColor>
#include <QHash>
#include <QTextBrowser>
#include <QTimer>

// Rendered markdown pane. Renders on a short debounce instead of every
// keystroke, restores the scroll ratio after each re-render, and resolves
// relative image paths against the document directory.
class MarkdownPreview : public QTextBrowser
{
    Q_OBJECT

public:
    explicit MarkdownPreview(QWidget *parent = nullptr);

    // Queue a re-render; actual rendering happens after the debounce interval.
    void setSourceMarkdown(const QString &markdown);

    // Base directory used to resolve relative image URLs.
    void setDocumentDirectory(const QString &dir);

public slots:
    // Programmatic scroll (from editor sync); does not emit scrollRatioChanged.
    void setScrollRatio(double ratio);

signals:
    // Emitted when the user scrolls the preview, for reverse sync.
    void scrollRatioChanged(double ratio);
    // Emitted right after a re-render completes.
    void documentRendered();

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    void renderNow();

    QTimer m_debounce;
    QString m_pendingMarkdown;
    QString m_documentDir;
    double m_lastRatio = 0.0;
};
