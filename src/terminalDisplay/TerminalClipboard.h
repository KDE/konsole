/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2021 Jonah Brüchert <jbb@kaidan.im>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

class QString;
class QStringList;

#include <QChar>

#include <optional>

namespace Konsole::terminalClipboard {

/**
 * Retrieves the content of the clipboard and preprocesses it for pasting
 * into the display.
 *
 * URLs of local files are treated specially:
 *  - The scheme part, "file://", is removed from each URL
 *  - The URLs are pasted as a space-separated list of file paths
 */
QString pasteFromClipboard();

/**
 * Removes unsafe characters from the string
 */
QString sanitizeString(const QString &text);

/**
 * Does various string operations in preparation for pasting the string into a terminal display
 */
std::optional<QString> prepareStringForPasting(QString text, bool appendReturn, bool bracketedPasteMode);

/**
 * Creates a list of localized description of unsafe characters contained in the given string.
 */
QStringList checkForUnsafeCharacters(const QString &text);

/**
 * Check if it's unsafe to paste the given character into a terminal
 */
bool isUnsafe(const QChar c);

void copyToX11Selection(const QString &textToCopy, bool copyAsHtml, bool autoCopySelectedText);

}
