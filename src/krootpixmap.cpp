/* vi: ts=8 sts=4 sw=4
 *
 * $Id: $
 *
 * This file is part of the KDE project, module kdeui.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 * 
 * You can Freely distribute this program under the GNU Library
 * General Public License. See the file "COPYING.LIB" for the exact 
 * licensing terms.
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
#include <kwm.h>
#include <kpixmap.h>
#include <ksharedpixmap.h>
#include <kpixmapeffect.h>
#include <krootpixmap.h>
#include <kmessagebox.h>

#include <X11/X.h>
#include <X11/Xlib.h>


KRootPixmap::KRootPixmap(QWidget *widget)
{
    m_pWidget = widget;
    m_pPixmap = new KSharedPixmap;
    m_pPixmap->setProp("KDE_ROOT_PIXMAPS");
    m_bInit = false;
    m_bActive = false;

    connect(kapp, SIGNAL(backgroundChanged(int)), 
	    SLOT(slotBackgroundChanged(int)));

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
    {
	QResizeEvent *e = (QResizeEvent *) event;
	if ( (e->size().width() < e->oldSize().width()) &&
	     (e->size().height() < e->oldSize().height())
	   )
	    break;
	
	repaint();
	break;
    }

    case QEvent::Move:
	repaint();
	break;

    case QEvent::Paint:
	// init after first paint event
	if (!m_bInit) {
	    m_bInit = true;
	    m_Desk = KWM::currentDesktop();
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
    if (!force && (m_Rect == QRect(p1, p2)))
	return;
    m_Rect = QRect(p1, p2);

    // KSharedPixmap will correctly clip the pixmap for us.
    m_pPixmap->loadFromShared(QString("DESKTOP%1").arg(m_Desk), m_Rect);
    if (m_pPixmap->isNull()) {
	qDebug("Background pixmap not available.");
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


bool KRootPixmap::checkAvail(bool show_dlg)
{
    if (KSharedPixmap::query(QString("DESKTOP%1").arg(KWM::currentDesktop()),
	    "KDE_ROOT_PIXMAPS") == QSize()) {
	if (show_dlg)
	    KMessageBox::sorry(0L, 
		i18n("Cannot find the desktop background. Pseudo transparency\n"
		     "cannot be used! To make the desktop background available,\n"
		     "go to Preferences -> Display -> Advanced and enable\n"
		     "the setting `Export background to shared Pixmap'"),
		i18n("Warning: Pseudo Transparency not Available"));
	return false;
    } else
	return true;
}
	

#include "krootpixmap.moc"
