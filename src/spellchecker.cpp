#include "spellchecker.h"

#ifdef FREEBUFF_HAVE_SONNET
#include <Sonnet/Speller>
#include <Sonnet/SpellCheckDecorator>
#endif

class SpellChecker::Impl
{
public:
#ifdef FREEBUFF_HAVE_SONNET
    Sonnet::Speller *speller = nullptr;
    Sonnet::SpellCheckDecorator *decorator = nullptr;
    bool enabled = true;
#else
    bool enabled = false;
#endif
};

SpellChecker::SpellChecker(QObject *parent)
    : QObject(parent)
    , d(new Impl)
{
#ifdef FREEBUFF_HAVE_SONNET
    d->speller = new Sonnet::Speller(this);
#endif
}

SpellChecker::~SpellChecker() = default;

bool SpellChecker::available() const
{
#ifdef FREEBUFF_HAVE_SONNET
    return d->speller->isValid();
#else
    return false;
#endif
}

bool SpellChecker::enabled() const
{
    return available() && d->enabled;
}

void SpellChecker::attachTo(QSyntaxHighlighter *highlighter)
{
    if (!highlighter)
        return;
#ifdef FREEBUFF_HAVE_SONNET
    delete d->decorator;
    d->decorator = new Sonnet::SpellCheckDecorator(highlighter);
    d->decorator->setActive(d->enabled);
#else
    Q_UNUSED(highlighter);
#endif
}

QString SpellChecker::currentDictionary() const
{
#ifdef FREEBUFF_HAVE_SONNET
    if (d->speller->isValid())
        return d->speller->currentLanguage();
#endif
    return {};
}

QStringList SpellChecker::dictionaries() const
{
#ifdef FREEBUFF_HAVE_SONNET
    if (d->speller->isValid())
        return d->speller->availableLanguages();
#endif
    return {};
}

QStringList SpellChecker::suggestionsFor(const QString &word) const
{
#ifdef FREEBUFF_HAVE_SONNET
    if (enabled() && d->speller->isValid() && !d->speller->check(word))
        return d->speller->suggest(word);
#endif
    return {};
}

void SpellChecker::ignoreWord(const QString &word)
{
#ifdef FREEBUFF_HAVE_SONNET
    if (d->speller->isValid())
        d->speller->addToSession(word);
#endif
    Q_UNUSED(word);
}

void SpellChecker::addToPersonal(const QString &word)
{
#ifdef FREEBUFF_HAVE_SONNET
    if (d->speller->isValid())
        d->speller->addToPersonal(word);
#endif
    Q_UNUSED(word);
}

void SpellChecker::setEnabled(bool enabled)
{
    if (d->enabled == enabled)
        return;
    d->enabled = enabled;
#ifdef FREEBUFF_HAVE_SONNET
    if (d->decorator)
        d->decorator->setActive(enabled);
#endif
    emit stateChanged();
}

void SpellChecker::setDictionary(const QString &dictionary)
{
#ifdef FREEBUFF_HAVE_SONNET
    if (d->speller->isValid()) {
        d->speller->setLanguage(dictionary);
        emit stateChanged();
    }
#endif
    Q_UNUSED(dictionary);
}
