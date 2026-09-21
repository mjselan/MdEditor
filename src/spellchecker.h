#pragma once

#include <QObject>
#include <QStringList>

class QSyntaxHighlighter;

// Facade over KDE Frameworks Sonnet. When the library was not found at CMake
// configure time (FREEBUFF_HAVE_SONNET undefined) every method degrades to a
// harmless no-op and available() returns false, so callers can wire the UI up
// unconditionally.
class SpellChecker : public QObject
{
    Q_OBJECT

public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;

    bool available() const;
    bool enabled() const;

    // Wraps the given highlighter with Sonnet's decorator so misspelled words
    // get the spell-check underline while our markdown formats stay intact.
    void attachTo(QSyntaxHighlighter *highlighter);

    QString currentDictionary() const;
    QStringList dictionaries() const;

    QStringList suggestionsFor(const QString &word) const;
    void ignoreWord(const QString &word);
    void addToPersonal(const QString &word);

public slots:
    void setEnabled(bool enabled);
    void setDictionary(const QString &dictionary);

signals:
    void stateChanged();

private:
    class Impl;
    Impl *d;
};
