// SPDX-License-Identifier: GPL-3.0-or-later
#include "spellchecker.h"

#ifdef MARKDOWNEDITOR_HAVE_SONNET
#include <Sonnet/Speller>
#endif

class SpellChecker::Impl
{
public:
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    Sonnet::Speller speller; // value type in KF6 Sonnet; no heap wrapper needed
    bool enabled = false;
#endif
};

SpellChecker::SpellChecker(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
{
}

SpellChecker::~SpellChecker() = default;

bool SpellChecker::available() const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    return d->speller.isValid();
#else
    return false;
#endif
}

bool SpellChecker::enabled() const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    return available() && d->enabled;
#else
    return false;
#endif
}

QString SpellChecker::currentDictionary() const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->speller.isValid())
        return d->speller.language();
#endif
    return {};
}

QStringList SpellChecker::dictionaries() const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->speller.isValid())
        return d->speller.availableLanguages();
#endif
    return {};
}

bool SpellChecker::isWordCorrect(const QString &word) const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (!enabled() || !d->speller.isValid())
        return true;
    if (word.size() < 2)
        return true;
    // Words containing digits (versions, hashes) are never flagged.
    for (const QChar &ch : word) {
        if (ch.isDigit())
            return true;
    }
    return !d->speller.isMisspelled(word);
#else
    Q_UNUSED(word);
    return true;
#endif
}

QStringList SpellChecker::suggestionsFor(const QString &word) const
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (enabled() && d->speller.isValid() && d->speller.isMisspelled(word))
        return d->speller.suggest(word);
#endif
    return {};
}

void SpellChecker::ignoreWord(const QString &word)
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->speller.isValid()) {
        d->speller.addToSession(word);
        if (d->enabled)
            emit stateChanged(); // underline refresh for this session word
    }
    return;
#endif
    Q_UNUSED(word);
}

void SpellChecker::addToPersonal(const QString &word)
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->speller.isValid()) {
        d->speller.addToPersonal(word);
        if (d->enabled)
            emit stateChanged();
    }
    return;
#endif
    Q_UNUSED(word);
}

void SpellChecker::setEnabled(bool enabled)
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->enabled == enabled)
        return;
    d->enabled = enabled;
    emit stateChanged();
    return;
#endif
    Q_UNUSED(enabled);
}

void SpellChecker::setDictionary(const QString &dictionary)
{
#ifdef MARKDOWNEDITOR_HAVE_SONNET
    if (d->speller.isValid()) {
        d->speller.setLanguage(dictionary);
        if (d->enabled)
            emit stateChanged();
    }
    return;
#endif
    Q_UNUSED(dictionary);
}
