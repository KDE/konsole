#include "sessionaction.h"
#include <klocale.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kstdaccel.h>
#include <kpopupmenu.h>
#include <ktoolbar.h>

NewSessionAction::NewSessionAction(const QObject *recvr, const char *slot): 
		KAction(i18n("&New"), "filenew",
                KStdAccel::key(KStdAccel::New), recvr, slot, 0L,
                KStdAction::stdName(KStdAction::New))
{
  m_popup = 0;
}

int NewSessionAction::plug( QWidget *widget, int index )
{  
  if ( widget->inherits( "KToolBar" ) )
  {
    KToolBar *bar = (KToolBar *)widget;

    int id_ = KAction::getToolButtonID();
    bar->insertButton( icon(), id_, SIGNAL( clicked() ), this,
	SLOT( slotActivated() ), isEnabled(), plainText(),
	index );

    addContainer( bar, id_ );

    connect( bar, SIGNAL( destroyed() ), this, SLOT( slotDestroyed() ) );

    bar->setDelayedPopup( id_, m_popup, true );

    return containerCount() - 1;
  }

  return KAction::plug( widget, index ); 
}


void NewSessionAction::setPopup(QPopupMenu * popup)
{
  m_popup=popup;
} 

#include "sessionaction.moc"
