/*
 *  SPDX-FileCopyrightText: 2002 Waldo Bastian <bastian@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-only
 **/

#ifndef ZMODEM_DIALOG_H
#define ZMODEM_DIALOG_H

#include <QDialog>

class KTextEdit;
class QDialogButtonBox;
namespace Konsole
{
class ZModemDialog : public QDialog
{
    Q_OBJECT

public:
    ZModemDialog(QWidget *parent, bool modal, const QString &caption);

    /**
     * Adds a line of text to the progress window
     */
    void addText(const QString &);

    /**
     * Adds a line of text without a new line to the progress window
     */
    void addProgressText(const QString &);

    /**
     * To indicate the process is finished.
     */
    void transferDone();

Q_SIGNALS:
    void zmodemCancel();

private Q_SLOTS:
    void slotClose();
    void slotCancel();

private:
    Q_DISABLE_COPY(ZModemDialog)

    void delayedDestruct();
    KTextEdit *_textEdit;
    QDialogButtonBox *mButtonBox;
};
}

#endif
