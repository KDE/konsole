/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kdeui.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 * 
 * You can Freely distribute this program under the GNU Library
 * General Public License. See the file "COPYING.LIB" for the exact 
 * licensing terms.
 *
 * 18 Dec 99: Geert Jansen
 *  
 *   Ported to the new KSharedPixmap.
 *
 * 11/05/99: Geert Jansen
 *
 *   First implemenation of the KRootPixmap class. I think this should be
 *   moved to kdeui after the KRASH release.
 * 
 */

#include <math.h>

#include <qwidget.h>
#include <qtimer.h>
#include <qrect.h>
#include <qpoint.h>
#include <qevent.h>

#include <kapp.h>
#include <klocale.h>
#include <kwin.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>
#include <kmessagebox.h>

#include <ksharedpixmap.h>
#include <krootpixmap.h>

#include <X11/X.h>
#include <X11/Xlib.h>


KRootPixmap::KRootPixmap(QWidget *widget)
{
    m_pWidget = widget;
    m_pPixmap = new KSharedPixmap();
    m_bInit = false;
    m_bActive = false;
    m_bShown = false;

    connect(kapp, SIGNAL(backgroundChanged(int)), SLOT(slotBackgroundChanged(int)));
    connect(m_pPixmap, SIGNAL(done(bool)), SLOT(slotDone(bool)));

    QObject *obj = m_pWidget;
    while (obj->parent())
	obj = obj->parent();
    obj->installEventFilter(this);
}


void KRootPixmap::start()
{
    m_bActive = true;
    repaint(true);
}


void KRootPixmap::stop()
{
    m_bActive = false;
}


void KRootPixmap::setFadeEffect(double fade, QColor color)
{
    if (fade < 0)
	m_Fade = 0;
    else if (fade > 1)
	m_Fade = 1;
    else
	m_Fade = fade;
    m_FadeColor = color;
}


bool KRootPixmap::eventFilter(QObject *obj, QEvent *event)
{
    if (!obj->isWidgetType())
	return false;

    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Move:
	repaint();
	break;

    case QEvent::Paint:
	// init after first paint event
	if (!m_bInit) {
	    m_bInit = true;
	    repaint();
	}
	break;

    default:
	break;

    }

    return false; // always continue processing
}

	
void KRootPixmap::repaint(bool force)
{
    if (!m_bActive || !m_bInit)
	return;

    QPoint p1 = m_pWidget->mapToGlobal(m_pWidget->rect().topLeft());
    QPoint p2 = m_pWidget->mapToGlobal(m_pWidget->rect().bottomRight());
    if ((p1 == m_Rect.topLeft()) && (m_pWidget->width() < m_Rect.width()) &&
	(m_pWidget->height() < m_Rect.height())
       ) {
	// Due to northwest bit gravity, we don't need to do anything if the
	// bottom right corner of the widget is moved inward.
	// That said, konsole clears the background when it is resized, so
	// we have to reset the background pixmap.
	m_pWidget->setBackgroundPixmap(*m_pPixmap);
	return;
    }

    if (!force && (m_Rect == QRect(p1, p2)))
	return;
    m_Rect = QRect(p1, p2);
    m_Desk = KWin::currentDesktop();

    // KSharedPixmap will correctly generate a tile for us.
    if (!m_pPixmap->loadFromShared(QString("DESKTOP%1").arg(m_Desk), m_Rect))
	qDebug("loading of desktop background failed");
}


bool KRootPixmap::checkAvailable(bool show_warning)
{
    QString name = QString("DESKTOP%1").arg(KWin::currentDesktop());
    bool avail = m_pPixmap->isAvailable(name);
    if (!avail && show_warning)
	KMessageBox::sorry(0L, 
	    i18n("Cannot find the desktop background. Pseudo transparency\n"
		 "cannot be used! To make the desktop background available,\n"
		 "go to Preferences -> Display -> Advanced and enable\n"
		 "the setting `Export background to shared Pixmap'"),
	    i18n("Warning: Pseudo Transparency not Available"));
    return avail;
}


void KRootPixmap::slotDone(bool success)
{
    if (!success) {
	qDebug("loading of desktop background failed");
	return;
    }

    if (m_Fade > 1e-6)
	KPixmapEffect::fade(*m_pPixmap, m_Fade, m_FadeColor);

    m_pWidget->setBackgroundPixmap(*m_pPixmap);
}
  

void KRootPixmap::slotBackgroundChanged(int desk)
{
    if (desk == m_Desk)
	repaint(true);
}


#include "krootpixmap.moc"
