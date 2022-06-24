/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORSCHEMEEDITOR_H
#define COLORSCHEMEEDITOR_H

// Qt
#include <QWidget>

// KDE
#include <QDialog>

#include "konsolecolorscheme_export.h"

class QTableWidgetItem;

namespace Ui
{
class ColorSchemeEditor;
}

namespace Konsole
{
class ColorScheme;

/**
 * A dialog for editing color schemes.
 *
 * After creation, the dialog can be initialized with the settings
 * of a color scheme using the setup() method.
 *
 * The dialog creates a copy of the supplied color scheme to which
 * any changes made are applied.  The modified color scheme
 * can be retrieved using the colorScheme() method.
 *
 * When changes are made the colorsChanged() signal is emitted.
 */
class KONSOLECOLORSCHEME_EXPORT ColorSchemeEditor : public QDialog
{
    Q_OBJECT

public:
    /** Constructs a new color scheme editor with the specified parent. */
    explicit ColorSchemeEditor(QWidget *parent = nullptr);
    ~ColorSchemeEditor() override;

    /** Initializes the dialog with the properties of the specified color scheme. */
    void setup(const std::shared_ptr<const ColorScheme> &scheme, bool isNewScheme);
    /** Returns the modified color scheme. */
    ColorScheme &colorScheme() const;
    bool isNewScheme() const;

Q_SIGNALS:
    /** Emitted when the colors in the color scheme change. */
    void colorsChanged(std::shared_ptr<ColorScheme> scheme);
    /** Used to send back colorscheme changes into the profile edited */
    void colorSchemeSaveRequested(const ColorScheme &scheme, bool isNewScheme);

public Q_SLOTS:
    /** Sets the text displayed in the description edit field. */
    void setDescription(const QString &description);

private Q_SLOTS:
    void setTransparencyPercentLabel(int percent);
    void setBlur(bool blur);
    void setRandomizedBackgroundColor(bool randomized);
    void editColorItem(QTableWidgetItem *item);
    void setWallpaperOpacity(int percent);
    void wallpaperPathChanged(const QString &path);
    void scalingTypeChanged(int styleIndex);
    void horizontalAnchorChanged(int pos);
    void verticalAnchorChanged(int pos);
    void selectWallpaper();
    /** Triggered by apply/ok buttons */
    void saveColorScheme();

private:
    Q_DISABLE_COPY(ColorSchemeEditor)

    void setupColorTable(const std::shared_ptr<ColorScheme> &table);
    void enableWallpaperSettings(bool enable);

    bool _isNewScheme;
    Ui::ColorSchemeEditor *_ui;
    std::shared_ptr<ColorScheme> _colors;
};
}

#endif // COLORSCHEMEEDITOR_H
