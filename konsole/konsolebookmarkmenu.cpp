#include <stdio.h>
#include <stdlib.h>

#include <qtextstream.h>

#include <kbookmarkimporter.h>
#include <kmimetype.h>
#include <kpopupmenu.h>
#include <ksavefile.h>
#include <kstandarddirs.h>

#include "konsole.h"
#include "konsolebookmarkmenu.h"
#include "konsolebookmarkhandler.h"

#include <qfile.h>
#include <qregexp.h>
#include <kaction.h>

KonsoleBookmarkMenu::KonsoleBookmarkMenu( KBookmarkManager* mgr,
                     KonsoleBookmarkHandler * _owner, KPopupMenu * _parentMenu,
                     KActionCollection *collec, bool _isRoot, bool _add,
                     const QString & parentAddress )
: KBookmarkMenu( mgr, _owner, _parentMenu, collec, _isRoot, _add,
                 parentAddress),
  m_kOwner(_owner)
{
    /*
     * First, we disconnect KBookmarkMenu::slotAboutToShow()
     * Then,  we connect    KonsoleBookmarkMenu::slotAboutToShow().
     * They are named differently because the SLOT() macro thinks we want
     * KonsoleBookmarkMenu::KBookmarkMenu::slotAboutToShow()
     * Could this be solved if slotAboutToShow() is virtual in KBookmarMenu?
     */
    disconnect( _parentMenu, SIGNAL( aboutToShow() ), this,
                SLOT( slotAboutToShow() ) );
    connect( _parentMenu, SIGNAL( aboutToShow() ),
             SLOT( slotAboutToShow2() ) );
}

/*
 * Duplicate this exactly because KBookmarkMenu::slotBookmarkSelected can't
 * be overrided.  I would have preferred to NOT have to do this.
 *
 * Why did I do this?
 *   - when KBookmarkMenu::fillbBookmarkMenu() creates sub-KBookmarkMenus.
 *   - when ... adds KActions, it uses KBookmarkMenu::slotBookmarkSelected()
 *     instead of KonsoleBookmarkMenu::slotBookmarkSelected().
 */
void KonsoleBookmarkMenu::slotAboutToShow2()
{
  // Did the bookmarks change since the last time we showed them ?
  if ( m_bDirty )
  {
    m_bDirty = false;
    refill();
  }
}

void KonsoleBookmarkMenu::refill()
{
  //kdDebug(1203) << "KBookmarkMenu::refill()" << endl;
  m_lstSubMenus.clear();
  QPtrListIterator<KAction> it( m_actions );
  for (; it.current(); ++it )
    it.current()->unplug( m_parentMenu );
  m_parentMenu->clear();
  m_actions.clear();
  fillBookmarkMenu();
  m_parentMenu->adjustSize();
}

void KonsoleBookmarkMenu::fillBookmarkMenu()
{
  if ( m_bIsRoot )
  {
    if ( m_bAddBookmark )
      addAddBookmark();

    addEditBookmarks();

    if ( m_bAddBookmark )
      addNewFolder();

    if ( m_pManager->showNSBookmarks()
         && QFile::exists( KNSBookmarkImporter::netscapeBookmarksFile() ) )
    {
      m_parentMenu->insertSeparator();

      KActionMenu * actionMenu = new KActionMenu( i18n("Netscape Bookmarks"),
                                                  "netscape",
                                                  m_actionCollection, 0L );
      actionMenu->plug( m_parentMenu );
      m_actions.append( actionMenu );
      KonsoleBookmarkMenu *subMenu = new KonsoleBookmarkMenu( m_pManager,
                                         m_kOwner, actionMenu->popupMenu(),
                                         m_actionCollection, false,
					 m_bAddBookmark, QString::null );
      m_lstSubMenus.append(subMenu);
      connect( actionMenu->popupMenu(), SIGNAL(aboutToShow()), subMenu,
               SLOT(slotNSLoad()));
    }
  }

  KBookmarkGroup parentBookmark = m_pManager->findByAddress( m_parentAddress ).toGroup();
  Q_ASSERT(!parentBookmark.isNull());
  bool separatorInserted = false;
  for ( KBookmark bm = parentBookmark.first(); !bm.isNull();
        bm = parentBookmark.next(bm) )
  {
    QString text = bm.text();
    text.replace( QRegExp( "&" ), "&&" );
    if ( !separatorInserted && m_bIsRoot) { // inserted before the first konq bookmark, to avoid the separator if no konq bookmark
      m_parentMenu->insertSeparator();
      separatorInserted = true;
    }
    if ( !bm.isGroup() )
    {
      if ( bm.isSeparator() )
      {
        m_parentMenu->insertSeparator();
      }
      else
      {
        // kdDebug(1203) << "Creating URL bookmark menu item for " << bm.text() << endl;
        // create a normal URL item, with ID as a name
        KAction * action = new KAction( text, bm.icon(), 0,
                                        this, SLOT( slotBookmarkSelected() ),
                                        m_actionCollection, bm.url().url().utf8() );

        action->setStatusText( bm.url().prettyURL() );

        action->plug( m_parentMenu );
        m_actions.append( action );
      }
    }
    else
    {
      // kdDebug(1203) << "Creating bookmark submenu named " << bm.text() << endl;
      KActionMenu * actionMenu = new KActionMenu( text, bm.icon(),
                                                  m_actionCollection, 0L );
      actionMenu->plug( m_parentMenu );
      m_actions.append( actionMenu );
      KonsoleBookmarkMenu *subMenu = new KonsoleBookmarkMenu( m_pManager,
                                         m_kOwner, actionMenu->popupMenu(),
                                         m_actionCollection, false,
                                         m_bAddBookmark, bm.address() );
      m_lstSubMenus.append( subMenu );
    }
  }

  if ( !m_bIsRoot && m_bAddBookmark )
  {
    m_parentMenu->insertSeparator();
    addAddBookmark();
    addNewFolder();
  }
}

void KonsoleBookmarkMenu::slotBookmarkSelected()
{
    KAction * a;
    QString b;

    if ( !m_pOwner ) return; // this view doesn't handle bookmarks...
    a = (KAction*)sender();
    b = a->text();
    m_kOwner->openBookmarkURL( QString::fromUtf8(sender()->name()), /* URL */
                               ( (KAction *)sender() )->text() /* Title */ );
}

void KonsoleBookmarkMenu::slotNSBookmarkSelected()
{
    KAction *a;
    QString b;

    QString link(sender()->name()+8);
    a = (KAction*)sender();
    b = a->text();
    m_kOwner->openBookmarkURL( link, /*URL */
                               ( (KAction *)sender() )->text()  /* Title */ );
}

#include "konsolebookmarkmenu.moc"
