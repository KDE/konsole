/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robert.knight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DUMBEMULATION_H
#define DUMBEMULATION_H

// Qt
#include <QVector>

// Konsole
#include "Emulation.h"
#include "Screen.h"

class QTimer;
class QKeyEvent;

namespace Konsole
{
/**
 * Provides a dumb terminal emulation.
 * The only non printable character is \n
 */
class KONSOLEPRIVATE_EXPORT DumbEmulation : public Emulation
{
    Q_OBJECT

public:
    /** Constructs a new emulation */
    DumbEmulation();
    ~DumbEmulation() override;

    // reimplemented from Emulation
    void clearEntireScreen() override;
    void reset(bool softReset = false, bool preservePrompt = false) override;
    char eraseChar() const override;

public Q_SLOTS:
    // reimplemented from Emulation
    void sendString(const QByteArray &string) override;
    void sendText(const QString &text) override;
    void sendKeyEvent(QKeyEvent *) override;
    void sendMouseEvent(int buttons, int column, int line, int eventType) override;
    void focusChanged(bool focused) override;

protected:
    // reimplemented from Emulation
    void setMode(int mode) override;
    void resetMode(int mode) override;
    void receiveChars(const QVector<uint> &chars) override;

private Q_SLOTS:
    // Causes sessionAttributeChanged() to be emitted for each (int,QString)
    // pair in _pendingSessionAttributesUpdates.
    // Used to buffer multiple attribute updates in the current session
    void updateSessionAttributes();

private:
    unsigned int applyCharset(uint c);
    void setCharset(int n, int cs);
    void useCharset(int n);
    void setAndUseCharset(int n, int cs);
    void saveCursor();
    void restoreCursor();
    void resetCharset(int scrno);

    // clears the screen and resizes it to the specified
    // number of columns
    void clearScreenAndSetColumns(int columnCount);
};

}

#endif // DUMBEMULATION_H
