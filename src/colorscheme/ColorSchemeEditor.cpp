/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ColorSchemeEditor.h"

// Qt
#include <QCompleter>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFontMetrics>
#include <QIcon>
#include <QImageReader>

// KDE
#include <KLocalizedString>
#include <KWindowSystem>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

// Konsole
#include "../characters/CharacterColor.h"
#include "ColorScheme.h"
#include "ui_ColorSchemeEditor.h"

using namespace Konsole;

// colorTable is one third the length of _table in ColorScheme class,
// because intense and faint colors are in separate columns
const int COLOR_TABLE_ROW_LENGTH = TABLE_COLORS / 3;

const int NAME_COLUMN = 0; // column 0 : color names
const int COLOR_COLUMN = 1; // column 1 : actual colors
const int INTENSE_COLOR_COLUMN = 2; // column 2 : intense colors
const int FAINT_COLOR_COLUMN = 3; // column 2 : faint colors

ColorSchemeEditor::ColorSchemeEditor(QWidget *parent)
    : QDialog(parent)
    , _isNewScheme(false)
    , _ui(nullptr)
{
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ColorSchemeEditor::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ColorSchemeEditor::reject);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &Konsole::ColorSchemeEditor::saveColorScheme);
    connect(okButton, &QPushButton::clicked, this, &Konsole::ColorSchemeEditor::saveColorScheme);

    // ui
    _ui = new Ui::ColorSchemeEditor();
    _ui->setupUi(mainWidget);

    // description edit
    _ui->descriptionEdit->setClearButtonEnabled(true);
    connect(_ui->descriptionEdit, &QLineEdit::textChanged, this, &Konsole::ColorSchemeEditor::setDescription);

    // transparency slider
    QFontMetrics metrics(font());
    _ui->transparencyPercentLabel->setMinimumWidth(metrics.boundingRect(QStringLiteral("100%")).width());

    connect(_ui->transparencySlider, &QSlider::valueChanged, this, &Konsole::ColorSchemeEditor::setTransparencyPercentLabel);

    // blur behind window
    connect(_ui->blurCheckBox, &QCheckBox::toggled, this, &Konsole::ColorSchemeEditor::setBlur);

    // randomized background
    connect(_ui->randomizedBackgroundCheck, &QCheckBox::toggled, this, &Konsole::ColorSchemeEditor::setRandomizedBackgroundColor);

    // wallpaper stuff
    auto dirModel = new QFileSystemModel(this);
    dirModel->setFilter(QDir::AllEntries);
    dirModel->setRootPath(QStringLiteral("/"));
    auto completer = new QCompleter(this);
    completer->setModel(dirModel);
    _ui->wallpaperPath->setCompleter(completer);

    _ui->wallpaperPath->setClearButtonEnabled(true);
    _ui->wallpaperSelectButton->setIcon(QIcon::fromTheme(QStringLiteral("image-x-generic")));

    connect(_ui->wallpaperTransparencySlider, &QSlider::valueChanged, this, &Konsole::ColorSchemeEditor::setWallpaperOpacity);

    connect(_ui->wallpaperSelectButton, &QToolButton::clicked, this, &Konsole::ColorSchemeEditor::selectWallpaper);
    connect(_ui->wallpaperPath, &QLineEdit::textChanged, this, &Konsole::ColorSchemeEditor::wallpaperPathChanged);

    connect(_ui->wallpaperScalingType, &QComboBox::currentTextChanged, this, &Konsole::ColorSchemeEditor::scalingTypeChanged);
    connect(_ui->wallpaperHorizontalAnchorSlider, &QSlider::valueChanged, this, &Konsole::ColorSchemeEditor::horizontalAnchorChanged);
    connect(_ui->wallpaperVerticalAnchorSlider, &QSlider::valueChanged, this, &Konsole::ColorSchemeEditor::verticalAnchorChanged);

    // color table
    _ui->colorTable->setColumnCount(4);
    _ui->colorTable->setRowCount(COLOR_TABLE_ROW_LENGTH);

    QStringList labels;
    labels << i18nc("@label:listbox Column header text for color names", "Name") << i18nc("@label:listbox Column header text for the actual colors", "Color")
           << i18nc("@label:listbox Column header text for the actual intense colors", "Intense color")
           << i18nc("@label:listbox Column header text for the actual faint colors", "Faint color");
    _ui->colorTable->setHorizontalHeaderLabels(labels);

    // Set resize mode for colorTable columns
    _ui->colorTable->horizontalHeader()->setSectionResizeMode(NAME_COLUMN, QHeaderView::ResizeToContents);
    _ui->colorTable->horizontalHeader()->setSectionResizeMode(COLOR_COLUMN, QHeaderView::Stretch);
    _ui->colorTable->horizontalHeader()->setSectionResizeMode(INTENSE_COLOR_COLUMN, QHeaderView::Stretch);
    _ui->colorTable->horizontalHeader()->setSectionResizeMode(FAINT_COLOR_COLUMN, QHeaderView::Stretch);

    QTableWidgetItem *item = new QTableWidgetItem(QStringLiteral("Test"));
    _ui->colorTable->setItem(0, 0, item);

    _ui->colorTable->verticalHeader()->hide();

    connect(_ui->colorTable, &QTableWidget::itemClicked, this, &Konsole::ColorSchemeEditor::editColorItem);

    // warning label when transparency is not available
    _ui->transparencyWarningWidget->setWordWrap(true);
    _ui->transparencyWarningWidget->setCloseButtonVisible(false);
    _ui->transparencyWarningWidget->setMessageType(KMessageWidget::Warning);

    if (KWindowSystem::compositingActive()) {
        _ui->transparencyWarningWidget->setVisible(false);
    } else {
        _ui->transparencyWarningWidget->setText(i18nc("@info:status",
                                                      "The background transparency setting will not"
                                                      " be used because your desktop does not appear to support"
                                                      " transparent windows."));
    }
}

ColorSchemeEditor::~ColorSchemeEditor()
{
    delete _ui;
}

void ColorSchemeEditor::editColorItem(QTableWidgetItem *item)
{
    // ignore if this is not a color column
    if (item->column() != COLOR_COLUMN && item->column() != INTENSE_COLOR_COLUMN && item->column() != FAINT_COLOR_COLUMN) {
        return;
    }

    QColor color = item->background().color();
    color = QColorDialog::getColor(color);
    if (color.isValid()) {
        item->setBackground(color);

        int colorSchemeRow = item->row();
        // Intense colors row are in the middle third of the color table
        if (item->column() == INTENSE_COLOR_COLUMN) {
            colorSchemeRow += COLOR_TABLE_ROW_LENGTH;
        }

        // and the faint color rows are in the final third of the color table
        if (item->column() == FAINT_COLOR_COLUMN) {
            colorSchemeRow += 2 * COLOR_TABLE_ROW_LENGTH;
        }

        _colors->setColorTableEntry(colorSchemeRow, color);

        Q_EMIT colorsChanged(_colors);
    }
}

void ColorSchemeEditor::selectWallpaper()
{
    // Get supported image formats and convert to QString for getOpenFileName()
    const QList<QByteArray> mimeTypes = QImageReader::supportedImageFormats();
    QString fileFormats = QStringLiteral("(");
    for (const QByteArray &mime : mimeTypes) {
        fileFormats += QStringLiteral("*.%1 ").arg(QLatin1String(mime));
    }
    fileFormats += QLatin1String(")");

    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          i18nc("@title:window", "Select wallpaper image file"),
                                                          _ui->wallpaperPath->text(),
                                                          i18nc("@label:textbox Filter in file open dialog", "Supported Images") + fileFormats);

    if (!fileName.isEmpty()) {
        _ui->wallpaperPath->setText(fileName);
    }
}

void ColorSchemeEditor::setWallpaperOpacity(int percent)
{
    _ui->wallpaperTransparencyPercentLabel->setText(QStringLiteral("%1%").arg(percent));

    const qreal opacity = (100.0 - percent) / 100.0;
    _colors->setWallpaper(_colors->wallpaper()->path(), _colors->wallpaper()->style(), _colors->wallpaper()->anchor(), opacity);
}

void ColorSchemeEditor::wallpaperPathChanged(const QString &path)
{
    if (path.isEmpty()) {
        _colors->setWallpaper(path, _colors->wallpaper()->style(), _colors->wallpaper()->anchor(), _colors->wallpaper()->opacity());
        enableWallpaperSettings(false);
    } else {
        QFileInfo i(path);

        if (i.exists() && i.isFile() && i.isReadable()) {
            _colors->setWallpaper(path, _colors->wallpaper()->style(), _colors->wallpaper()->anchor(), _colors->wallpaper()->opacity());
            enableWallpaperSettings(true);
        }
    }
}

void ColorSchemeEditor::scalingTypeChanged(QString style)
{
    _colors->setWallpaper(_colors->wallpaper()->path(), style, _colors->wallpaper()->anchor(), _colors->wallpaper()->opacity());
}

void ColorSchemeEditor::horizontalAnchorChanged(int pos)
{
    QPointF anch = _colors->wallpaper()->anchor();
    _colors->setWallpaper(_colors->wallpaper()->path(), _colors->wallpaper()->style(), QPointF(pos / 2.0, anch.y()), _colors->wallpaper()->opacity());
    switch (pos) {
    case 2:
        _ui->wallpaperHorizontalAnchorPosition->setText(QString::fromLatin1("Right"));
        break;
    case 1:
        _ui->wallpaperHorizontalAnchorPosition->setText(QString::fromLatin1("Center"));
        break;
    case 0:
    default:
        _ui->wallpaperHorizontalAnchorPosition->setText(QString::fromLatin1("Left"));
        break;
    }
}

void ColorSchemeEditor::verticalAnchorChanged(int pos)
{
    QPointF anch = _colors->wallpaper()->anchor();
    _colors->setWallpaper(_colors->wallpaper()->path(), _colors->wallpaper()->style(), QPointF(anch.x(), pos / 2.0), _colors->wallpaper()->opacity());
    switch (pos) {
    case 2:
        _ui->wallpaperVerticalAnchorPosition->setText(QString::fromLatin1("Bottom"));
        break;
    case 1:
        _ui->wallpaperVerticalAnchorPosition->setText(QString::fromLatin1("Middle"));
        break;
    case 0:
    default:
        _ui->wallpaperVerticalAnchorPosition->setText(QString::fromLatin1("Top"));
        break;
    }
}

void ColorSchemeEditor::setDescription(const QString &description)
{
    if (_colors != nullptr) {
        _colors->setDescription(description);
    }

    if (_ui->descriptionEdit->text() != description) {
        _ui->descriptionEdit->setText(description);
    }
}

void ColorSchemeEditor::setTransparencyPercentLabel(int percent)
{
    _ui->transparencyPercentLabel->setText(QStringLiteral("%1%").arg(percent));

    const qreal opacity = (100.0 - percent) / 100.0;
    _colors->setOpacity(opacity);
}

void ColorSchemeEditor::setBlur(bool blur)
{
    _colors->setBlur(blur);
}

void ColorSchemeEditor::setRandomizedBackgroundColor(bool randomized)
{
    _colors->setColorRandomization(randomized);
}

void ColorSchemeEditor::setup(const std::shared_ptr<const ColorScheme> &scheme, bool isNewScheme)
{
    _isNewScheme = isNewScheme;

    _colors = std::make_shared<ColorScheme>(*scheme);

    if (_isNewScheme) {
        setWindowTitle(i18nc("@title:window", "New Color Scheme"));
        setDescription(QStringLiteral("New Color Scheme"));
    } else {
        setWindowTitle(i18nc("@title:window", "Edit Color Scheme"));
    }

    // setup description edit
    _ui->descriptionEdit->setText(_colors->description());

    // setup color table
    setupColorTable(_colors);

    // setup transparency slider
    const int colorTransparencyPercent = qRound((1 - _colors->opacity()) * 100);
    const int wallpaperTransparencyPercent = qRound((1 - _colors->wallpaper()->opacity()) * 100);
    _ui->transparencySlider->setValue(colorTransparencyPercent);
    _ui->wallpaperTransparencySlider->setValue(wallpaperTransparencyPercent);
    setTransparencyPercentLabel(colorTransparencyPercent);
    setWallpaperOpacity(wallpaperTransparencyPercent);

    // blur behind window checkbox
    _ui->blurCheckBox->setChecked(scheme->blur());

    // randomized background color checkbox
    _ui->randomizedBackgroundCheck->setChecked(scheme->isColorRandomizationEnabled());

    // wallpaper stuff
    int ax = qRound(scheme->wallpaper()->anchor().x() * 2.0);
    int ay = qRound(scheme->wallpaper()->anchor().y() * 2.0);
    _ui->wallpaperPath->setText(scheme->wallpaper()->path());
    _ui->wallpaperScalingType->setCurrentIndex(scheme->wallpaper()->style());
    _ui->wallpaperHorizontalAnchorSlider->setValue(ax);
    _ui->wallpaperVerticalAnchorSlider->setValue(ay);
    enableWallpaperSettings(!scheme->wallpaper()->isNull());
}

void ColorSchemeEditor::setupColorTable(const std::shared_ptr<ColorScheme> &colors)
{
    QColor table[TABLE_COLORS];
    colors->getColorTable(table);

    for (int row = 0; row < COLOR_TABLE_ROW_LENGTH; row++) {
        QTableWidgetItem *nameItem = new QTableWidgetItem(ColorScheme::translatedColorNameForIndex(row));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

        auto colorItem = new QTableWidgetItem();
        colorItem->setBackground(table[row]);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        colorItem->setToolTip(i18nc("@info:tooltip", "Click to choose color"));

        auto colorItemIntense = new QTableWidgetItem();
        colorItemIntense->setBackground(table[COLOR_TABLE_ROW_LENGTH + row]);
        colorItemIntense->setFlags(colorItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        colorItemIntense->setToolTip(i18nc("@info:tooltip", "Click to choose intense color"));

        auto colorItemFaint = new QTableWidgetItem();
        colorItemFaint->setBackground(table[2 * COLOR_TABLE_ROW_LENGTH + row]);
        colorItemFaint->setFlags(colorItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        colorItemFaint->setToolTip(i18nc("@info:tooltip", "Click to choose Faint color"));

        _ui->colorTable->setItem(row, NAME_COLUMN, nameItem);
        _ui->colorTable->setItem(row, COLOR_COLUMN, colorItem);
        _ui->colorTable->setItem(row, INTENSE_COLOR_COLUMN, colorItemIntense);
        _ui->colorTable->setItem(row, FAINT_COLOR_COLUMN, colorItemFaint);
    }
    // ensure that color names are as fully visible as possible
    _ui->colorTable->resizeColumnToContents(0);
}

ColorScheme &ColorSchemeEditor::colorScheme() const
{
    return *_colors;
}

bool ColorSchemeEditor::isNewScheme() const
{
    return _isNewScheme;
}

void ColorSchemeEditor::saveColorScheme()
{
    Q_EMIT colorSchemeSaveRequested(colorScheme(), _isNewScheme);
}

void ColorSchemeEditor::enableWallpaperSettings(bool enable)
{
    _ui->wallpaperHorizontalAnchorSlider->setEnabled(enable);
    _ui->wallpaperVerticalAnchorSlider->setEnabled(enable);
    _ui->wallpaperTransparencySlider->setEnabled(enable);
    _ui->wallpaperScalingType->setEnabled(enable);
}
