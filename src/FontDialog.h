#ifndef FONTDIALOG_H
#define FONTDIALOG_H

// Qt
#include <QDialog>
#include <QCheckBox>
#include <KFontChooser>
#include <QDialogButtonBox>
#include <QToolButton>

namespace Konsole {
class FontDialog: public QDialog
{
    Q_OBJECT

public:
    explicit FontDialog(QWidget *parent = nullptr);

    QFont font() const { return _fontChooser->font(); }
    void setFont(const QFont &font);

Q_SIGNALS:
    void fontChanged(QFont font);

private:
    KFontChooser *_fontChooser;
    QCheckBox *_showAllFonts;
    QToolButton *_showAllFontsWarningButton;
    QDialogButtonBox *_buttonBox;
};
}

#endif // FONTDIALOG_H
