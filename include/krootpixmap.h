/* vi: ts=8 sts=4 sw=4
 *
 * $Id: $
 *
 * This file is part of the KDE project, module kdesktop.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 * 
 * You can Freely distribute this program under the GNU Library General 
 * Public License. See the file "COPYING.LIB" for the exact licensing terms.
 */

#ifndef __KRootPixmap_h_Included__
#define __KRootPixmap_h_Included__

#include <qobject.h>
#include <qcolor.h>

class QEvent;
class QRect;
class QWidget;
class KSharedPixmap;

/**
 * Implements pseudo transparent widgets. 
 * 
 * A pseudo transparent widget is a widget with its background pixmap set to
 * that part of the desktop background which it is currently obscuring. This
 * gives a transparency effect.
 *
 * To create a transparent widget, construct a KRootPixmap and pass it a 
 * pointer to your widget. That's it! Moving, resizing and background changes 
 * are handled automatically.
 */
class KRootPixmap: public QObject
{
    Q_OBJECT

public:
    /**
     * Construct a KRootPixmap.
     *
     * @param widget A pointer to the widget that you want to make pseudo 
     * transparent.
     */
    KRootPixmap(QWidget *widget);
    
    /**
     * Sets the fade effect. This effect will fade the background to the
     * specified color. 
     * @param strength A value between 0 and 1, indicating the strength
     * of the fade. A value of 0 will not change the image, a value of 1
     * will make it the fade color everywhere, and in between.
     * @param color The color to fade to.
     */
    void setFadeEffect(double strength, QColor color);

    /**
     * Check if pseudo transparency is available.
     *
     * @param showDlg If set to true and pseudo transparency is not
     * available, a dialog box is presented to the user explaining the
     * situation and how to resolve it.
     * @return True if pseudo transparency is available, false otherwise.
     */
    bool checkAvail(bool showDlg=false);


    /**
     * Start background handling.
     */
    void start();

    /**
     * Stop background handling.
     */
    void stop();

    /**
     * Repaint the widget background. Normally, you shouldn't need this.
     *
     * @param force Force a repaint, even if the contents did not change.
     */
    void repaint(bool force=false);

protected:
    virtual bool eventFilter(QObject *, QEvent *);

private slots:
    void slotBackgroundChanged(int desk);

private:
    bool m_bActive, m_bInit;
    int m_Desk;

    double m_Fade;
    QColor m_FadeColor;

    QRect m_Rect;
    QWidget *m_pWidget;
    KSharedPixmap *m_pPixmap;
};

#endif // __KRootPixmap_h_Included__

