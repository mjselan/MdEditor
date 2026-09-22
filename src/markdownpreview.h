#pragma once

#include <QColor>
#include <QHash>
#include <QPointer>
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

    // Live-source mode: renders pull the text from the document only when the
    // debounce fires, so large documents are not copied on every keystroke.
    void setSourceDocument(QTextDocument *document);
    void scheduleRender();

    // Base directory used to resolve relative image URLs.
    void setDocumentDirectory(const QString &dir);

public slots:
    // Programmatic scroll (from editor sync); does not emit scrollRatioChanged.
    void setScrollRatio(double ratio);

    // Render immediately, ignoring the debounce (e.g. before HTML export).
    void renderNow();

signals:
    // Emitted when the user scrolls the preview, for reverse sync.
    void scrollRatioChanged(double ratio);
    // Emitted right after a re-render completes.
    void documentRendered();

protected:
    QVariant loadResource(int type, const QUrl &name) override;

private:
    QTimer m_debounce;
    QString m_pendingMarkdown;
    QPointer<QTextDocument> m_sourceDocument;
    QString m_documentDir;
    double m_lastRatio = 0.0;
};
