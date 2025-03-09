/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robert.knight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// Own
#include "DumbEmulation.h"
#include "config-konsole.h"

// Standard
#include <cstdio>
#include <unistd.h>

// Qt
#include <QEvent>
#include <QKeyEvent>

// KDE
#include <KLocalizedString>

// Konsole
#include "keyboardtranslator/KeyboardTranslator.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalDisplay.h"

using Konsole::DumbEmulation;

DumbEmulation::DumbEmulation()
    : Emulation()
{
    TERM = QStringLiteral("dumb");
}

DumbEmulation::~DumbEmulation()
{
}

void DumbEmulation::clearEntireScreen()
{
    _currentScreen->clearEntireScreen();
    bufferedUpdate();
}

void DumbEmulation::reset([[maybe_unused]] bool softReset, [[maybe_unused]] bool preservePrompt)
{

    // Save the current codec so we can set it later.
    // Ideally we would want to use the profile setting
    const QByteArray currentCodec(encoder().name());

    if (currentCodec != nullptr) {
        setCodec(currentCodec);
    } else {
        setCodec(LocaleCodec);
    }

    Q_EMIT resetCursorStyleRequest();

    bufferedUpdate();
}

// process an incoming unicode character
void DumbEmulation::receiveChars(const QVector<uint> &chars)
{
    for (uint cc : chars) {
        if (cc == '\r') {
            _currentScreen->nextLine();
            continue;
        }
        _currentScreen->displayCharacter(cc);
    }
}

void DumbEmulation::sendString(const QByteArray &s)
{
    Q_EMIT sendData(s);
}

void DumbEmulation::sendText(const QString &text)
{
    if (!text.isEmpty()) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, text);
        sendKeyEvent(&event); // expose as a big fat keypress event
    }
}

void DumbEmulation::sendKeyEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    KeyboardTranslator::States states = KeyboardTranslator::NoState;

    TerminalDisplay *currentView = _currentScreen->currentTerminalDisplay();
    bool isReadOnly = false;
    if (currentView != nullptr) {
        isReadOnly = currentView->getReadOnly();
    }

    if (!isReadOnly) {
        // check flow control state
        if ((modifiers & Qt::ControlModifier) != 0U) {
            switch (event->key()) {
            case Qt::Key_S:
                Q_EMIT flowControlKeyPressed(true);
                break;
            case Qt::Key_C:
                // No Sixel support
            case Qt::Key_Q: // cancel flow control
                Q_EMIT flowControlKeyPressed(false);
                break;
            }
        }
    }

    // look up key binding
    if (_keyTranslator != nullptr) {
        KeyboardTranslator::Entry entry = _keyTranslator->findEntry(event->key(), modifiers, states);

        // send result to terminal
        QByteArray textToSend;

        // special handling for the Alt (aka. Meta) modifier.  pressing
        // Alt+[Character] results in Esc+[Character] being sent
        // (unless there is an entry defined for this particular combination
        //  in the keyboard modifier)
        const bool wantsAltModifier = ((entry.modifiers() & entry.modifierMask() & Qt::AltModifier) != 0U);
        const bool wantsMetaModifier = ((entry.modifiers() & entry.modifierMask() & Qt::MetaModifier) != 0U);
        const bool wantsAnyModifier = ((entry.state() & entry.stateMask() & KeyboardTranslator::AnyModifierState) != 0);

        if (((modifiers & Qt::AltModifier) != 0U) && !(wantsAltModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\033");
        }
        if (((modifiers & Qt::MetaModifier) != 0U) && !(wantsMetaModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\030@s");
        }

        if (entry.command() != KeyboardTranslator::NoCommand) {
            if ((entry.command() & KeyboardTranslator::EraseCommand) != 0) {
                textToSend += eraseChar();
            }
        } else if (!entry.text().isEmpty()) {
            textToSend += entry.text(true, modifiers);
        } else {
            Q_ASSERT(_encoder.isValid());
            textToSend += _encoder.encode(event->text());
        }

        if (!isReadOnly) {
            Q_EMIT sendData(textToSend);
        }
    } else {
        if (!isReadOnly) {
            // print an error message to the terminal if no key translator has been
            // set
            QString translatorError = i18n(
                "No keyboard translator available.  "
                "The information needed to convert key presses "
                "into characters to send to the terminal "
                "is missing.");
            reset();
            receiveData(translatorError.toLatin1().constData(), translatorError.count());
        }
    }
}

void DumbEmulation::setMode(int)
{
}

void DumbEmulation::resetMode(int)
{
}

void DumbEmulation::saveCursor()
{
    _currentScreen->saveCursor();
}

void DumbEmulation::restoreCursor()
{
    _currentScreen->restoreCursor();
}

void DumbEmulation::updateSessionAttributes()
{
}

char DumbEmulation::eraseChar() const
{
    return '\b';
}

void DumbEmulation::focusChanged([[maybe_unused]] bool focused)
{
}

void DumbEmulation::sendMouseEvent([[maybe_unused]] int cb, [[maybe_unused]] int cx, [[maybe_unused]] int cy, [[maybe_unused]] int eventType)
{
}
