/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalFonts.h"

// Konsole
#include "konsoledebug.h"
#include "session/Session.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"

// Qt
#include <QFont>
#include <QFontMetrics>

namespace Konsole
{
TerminalFont::TerminalFont(QWidget *parent)
    : m_parent(parent)
    , m_lineSpacing(0)
    , m_fontHeight(1)
    , m_fontWidth(1)
    , m_fontAscent(1)
    , m_boldIntense(false)
    , m_antialiasText(true)
    , m_useFontLineCharacters(false)
    , m_profile(nullptr)
{
}

void TerminalFont::applyProfile(const Profile::Ptr &profile)
{
    m_profile = profile;
    m_antialiasText = profile->antiAliasFonts();
    m_boldIntense = profile->boldIntense();
    m_useFontLineCharacters = profile->useFontLineCharacters();
    m_lineSpacing = uint(profile->lineSpacing());
    setVTFont(profile->font());
}

void TerminalFont::setVTFont(const QFont &f)
{
    QFont newFont(f);
    int strategy = 0;

    // hint that text should be drawn with- or without anti-aliasing.
    // depending on the user's font configuration, this may not be respected
    strategy |= m_antialiasText ? QFont::PreferAntialias : QFont::NoAntialias;

    // Konsole cannot handle non-integer font metrics
    // TODO: Qt6 will remove ForceIntegerMetrics
    // "Use QFontMetrics to retrieve rounded font metrics."
    strategy |= QFont::ForceIntegerMetrics;

    // In case the provided font doesn't have some specific characters it should
    // fall back to a Monospace fonts.
    newFont.setStyleHint(QFont::TypeWriter, QFont::StyleStrategy(strategy));

    // Try to check that a good font has been loaded.
    // For some fonts, ForceIntegerMetrics causes height() == 0 which
    // will cause Konsole to crash later.
    QFontMetrics fontMetrics2(newFont);
    if (fontMetrics2.height() < 1) {
        qCDebug(KonsoleDebug) << "The font " << newFont.toString() << " has an invalid height()";
        // Ask for a generic font so at least it is usable.
        // Font listed in profile's dialog will not be updated.
        newFont = QFont(QStringLiteral("Monospace"));
        // Set style strategy without ForceIntegerMetrics for the font
        // TODO: Qt6 will remove ForceIntegerMetrics
        // "Use QFontMetrics to retrieve rounded font metrics."
        strategy &= ~QFont::ForceIntegerMetrics;
        newFont.setStyleHint(QFont::TypeWriter, QFont::StyleStrategy(strategy));
        qCDebug(KonsoleDebug) << "Font changed to " << newFont.toString();
    }

    // experimental optimization.  Konsole assumes that the terminal is using a
    // mono-spaced font, in which case kerning information should have an effect.
    // Disabling kerning saves some computation when rendering text.
    newFont.setKerning(false);

    // "Draw intense colors in bold font" feature needs to use different font weights. StyleName
    // property, when set, doesn't allow weight changes. Since all properties (weight, stretch,
    // italic, etc) are stored in QFont independently, in almost all cases styleName is not needed.
    newFont.setStyleName(QString());

    if (newFont == qobject_cast<QWidget *>(m_parent)->font()) {
        // Do not process the same font again
        return;
    }

    QFontInfo fontInfo(newFont);

    // QFontInfo::fixedPitch() appears to not match QFont::fixedPitch() - do not test it.
    // related?  https://bugreports.qt.io/browse/QTBUG-34082
    if (fontInfo.family() != newFont.family() || !qFuzzyCompare(fontInfo.pointSizeF(), newFont.pointSizeF()) || fontInfo.styleHint() != newFont.styleHint()
        || fontInfo.weight() != newFont.weight() || fontInfo.style() != newFont.style() || fontInfo.underline() != newFont.underline()
        || fontInfo.strikeOut() != newFont.strikeOut() || fontInfo.rawMode() != newFont.rawMode()) {
        const QString nonMatching = QString::asprintf("%s,%g,%d,%d,%d,%d,%d,%d,%d,%d",
                                                      qPrintable(fontInfo.family()),
                                                      fontInfo.pointSizeF(),
                                                      -1, // pixelSize is not used
                                                      static_cast<int>(fontInfo.styleHint()),
                                                      fontInfo.weight(),
                                                      static_cast<int>(fontInfo.style()),
                                                      static_cast<int>(fontInfo.underline()),
                                                      static_cast<int>(fontInfo.strikeOut()),
                                                      // Intentional newFont use - fixedPitch is bugged, see comment above
                                                      static_cast<int>(newFont.fixedPitch()),
                                                      static_cast<int>(fontInfo.rawMode()));
        qCDebug(KonsoleDebug) << "The font to use in the terminal can not be matched exactly on your system.";
        qCDebug(KonsoleDebug) << " Selected: " << newFont.toString();
        qCDebug(KonsoleDebug) << " System  : " << nonMatching;
    }

    qobject_cast<QWidget *>(m_parent)->setFont(newFont);
    fontChange(newFont);
}

QFont TerminalFont::getVTFont() const
{
    return qobject_cast<QWidget *>(m_parent)->font();
}

void TerminalFont::increaseFontSize()
{
    QFont font = qobject_cast<QWidget *>(m_parent)->font();
    font.setPointSizeF(font.pointSizeF() + 1);
    setVTFont(font);
}

void TerminalFont::decreaseFontSize()
{
    const qreal MinimumFontSize = 6;

    QFont font = qobject_cast<QWidget *>(m_parent)->font();
    font.setPointSizeF(qMax(font.pointSizeF() - 1, MinimumFontSize));
    setVTFont(font);
}

void TerminalFont::resetFontSize()
{
    const qreal MinimumFontSize = 6;

    TerminalDisplay *display = qobject_cast<TerminalDisplay *>(m_parent);
    QFont font = display->font();
    Profile::Ptr currentProfile = SessionManager::instance()->sessionProfile(display->sessionController()->session());
    const qreal defaultFontSize = currentProfile->font().pointSizeF();
    font.setPointSizeF(qMax(defaultFontSize, MinimumFontSize));
    setVTFont(font);
}

void TerminalFont::setLineSpacing(uint i)
{
    m_lineSpacing = i;
    fontChange(qobject_cast<QWidget *>(m_parent)->font());
}

uint TerminalFont::lineSpacing() const
{
    return m_lineSpacing;
}

int TerminalFont::fontHeight() const
{
    return m_fontHeight;
}

int TerminalFont::fontWidth() const
{
    return m_fontWidth;
}

int TerminalFont::fontAscent() const
{
    return m_fontAscent;
}

bool TerminalFont::boldIntense() const
{
    return m_boldIntense;
}

bool TerminalFont::antialiasText() const
{
    return m_antialiasText;
}

bool TerminalFont::useFontLineCharacters() const
{
    return m_useFontLineCharacters;
}

void TerminalFont::fontChange(const QFont &)
{
    QFontMetrics fm(qobject_cast<QWidget *>(m_parent)->font());
    m_fontHeight = fm.height() + m_lineSpacing;

    Q_ASSERT(m_fontHeight > 0);

    m_fontWidth = fm.horizontalAdvance(QLatin1Char('M'));

    if (m_fontWidth < 1) {
        m_fontWidth = 1;
    }

    m_fontAscent = fm.ascent();

    qobject_cast<TerminalDisplay *>(m_parent)->propagateSize();
}

}
