// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

#include <QObject>
#include <QStringList>

// Spell-check facade over KDE Frameworks Sonnet's core Speller. When the
// library was not found at CMake configure time (MARKDOWNEDITOR_HAVE_SONNET
// undefined) every method degrades to a harmless no-op and available()
// returns false, so callers can wire the UI up unconditionally.
//
// The markdown highlighter asks the checker about words it tokenizes and
// draws the misspelled underline itself; that keeps spell formatting and
// markdown formatting in one highlighter so they can never fight over the
// same block.
class SpellChecker : public QObject
{
    Q_OBJECT

public:
    explicit SpellChecker(QObject *parent = nullptr);
    ~SpellChecker() override;

    bool available() const;
    bool enabled() const;

    QString currentDictionary() const;
    QStringList dictionaries() const;

    // True when the word needs no underline: correct, session-ignored,
    // personal, containing digits, or spell checking disabled/absent.
    bool isWordCorrect(const QString &word) const;

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
    std::unique_ptr<Impl> d;
};
