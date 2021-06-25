/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 2021 Jonah Brüchert <jbb@kaidan.im>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include <QMimeData>
#include <QString>
#include <QApplication>
#include <QClipboard>
#include <QUrl>
#include <QKeyEvent>

#include <KShell>
#include <KLocalizedString>

#include "TerminalClipboard.h"

namespace Konsole::terminalClipboard {

// Most code in Konsole uses UTF-32. We're filtering
// UTF-16 here, as all control characters can be represented
// in this encoding as single code unit. If you ever need to
// filter anything above 0xFFFF (specific code points or
// categories which contain such code points), convert text to
// UTF-32 using QString::toUcs4() and use QChar static
// methods which take "uint ucs4".
static constexpr std::array<ushort, 3> ALLOWLIST = { u'\t', u'\r', u'\n' };

QString pasteFromClipboard()
{
    QString text;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Clipboard);

    // When pasting urls of local files:
    // - remove the scheme part, "file://"
    // - paste the path(s) as a space-separated list of strings, which are quoted if needed
    if (!mimeData->hasUrls()) { // fast path if there are no urls
        text = mimeData->text();
    } else { // handle local file urls
        const QList<QUrl> list = mimeData->urls();
        for (const QUrl &url : list) {
            if (url.isLocalFile()) {
                text += KShell::quoteArg(url.toLocalFile());
                text += QLatin1Char(' ');
            } else { // can users copy urls of both local and remote files at the same time?
                text = mimeData->text();
                break;
            }
        }
    }

    return text;
}

QString sanitizeString(const QString &text)
{
    QString sanitized;
    for (const QChar &c : text) {
        if (!isUnsafe(c)) {
            sanitized.append(c);
        }
    }
    return sanitized;
}


std::optional<QString> prepareStringForPasting(QString text, bool appendReturn, bool bracketedPasteMode)
{
    if (appendReturn) {
        text.append(QLatin1String("\r"));
    }

    if (!text.isEmpty()) {
        // replace CRLF with CR first, fixes issues with pasting multiline
        // text from gtk apps (e.g. Firefox), bug 421480
        text.replace(QLatin1String("\r\n"), QLatin1String("\r"));

        text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
        if (bracketedPasteMode) {
            text.remove(QLatin1String("\033"));
            text.prepend(QLatin1String("\033[200~"));
            text.append(QLatin1String("\033[201~"));
        }

        return text;
    }

    return {};
}

QStringList checkForUnsafeCharacters(const QString &text)
{
    // Returns control sequence string (e.g. "^C") for control character c
    static const auto charToSequence = [](const QChar &c) {
        if (c.unicode() <= 0x1F) {
            return QStringLiteral("^%1").arg(QChar(u'@' + c.unicode()));
        } else if (c.unicode() == 0x7F) {
            return QStringLiteral("^?");
        } else if (c.unicode() >= 0x80 && c.unicode() <= 0x9F){
            return QStringLiteral("^[%1").arg(QChar(u'@' + c.unicode() - 0x80));
        }
        return QString();
    };

    const QMap<ushort, QString> characterDescriptions = {
        {0x0003, i18n("End Of Text/Interrupt: may exit the current process")},
        {0x0004, i18n("End Of Transmission: may exit the current process")},
        {0x0007, i18n("Bell: will try to emit an audible warning")},
        {0x0008, i18n("Backspace")},
        {0x0013, i18n("Device Control Three/XOFF: suspends output")},
        {0x001a, i18n("Substitute/Suspend: may suspend current process")},
        {0x001b, i18n("Escape: used for manipulating terminal state")},
        {0x001c, i18n("File Separator/Quit: may abort the current process")},
    };

    QStringList unsafeCharactersDescriptions;
    for (const QChar &c : text) {
        if (isUnsafe(c)) {
            const QString sequence = charToSequence(c);
            const QString description = characterDescriptions.value(c.unicode(), QString());
            QString entry = QStringLiteral("U+%1").arg(c.unicode(), 4, 16, QLatin1Char('0'));
            if(!sequence.isEmpty()) {
                entry += QStringLiteral("\t%1").arg(sequence);
            }
            if(!description.isEmpty()) {
                entry += QStringLiteral("\t%1").arg(description);
            }
            unsafeCharactersDescriptions.append(entry);
        }
    }
    unsafeCharactersDescriptions.removeDuplicates();

    return unsafeCharactersDescriptions;
}

bool isUnsafe(const QChar c) {
    return (c.category() == QChar::Category::Other_Control && std::find(ALLOWLIST.begin(), ALLOWLIST.end(), c.unicode()) != ALLOWLIST.end());
}

void copyToX11Selection(const QString &textToCopy, const QString &htmlToCopy, bool autoCopySelectedText)
{
    if (textToCopy.isEmpty()) {
        return;
    }

    auto mimeData = new QMimeData;
    mimeData->setText(textToCopy);

    if (!htmlToCopy.isEmpty()) {
        mimeData->setHtml(htmlToCopy);
    }

    if (QApplication::clipboard()->supportsSelection()) {
        QApplication::clipboard()->setMimeData(mimeData, QClipboard::Selection);
    }

    if (autoCopySelectedText) {
        QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
    }
}

}
