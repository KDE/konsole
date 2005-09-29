/* This file was part of the KDE libraries
    Copyright (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <kpopupmenu.h>
#include <kstandarddirs.h>
#include <kaction.h>

#include "konsole.h"
#include "konsolebookmarkmenu.h"
#include "konsolebookmarkhandler.h"

KonsoleBookmarkMenu::KonsoleBookmarkMenu( KBookmarkManager* mgr,
                     KonsoleBookmarkHandler * _owner, KPopupMenu * _parentMenu,
                     KActionCollection *collec, bool _isRoot, bool _add,
                     const QString & parentAddress )
: KBookmarkMenu( mgr, _owner, _parentMenu, collec, _isRoot, _add,
                 parentAddress),
  m_kOwner(_owner)
{
    m_bAddShortcuts = false;
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
  }

  KBookmarkGroup parentBookmark = m_pManager->findByAddress( m_parentAddress ).toGroup();
  Q_ASSERT(!parentBookmark.isNull());
  bool separatorInserted = false;
  for ( KBookmark bm = parentBookmark.first(); !bm.isNull();
        bm = parentBookmark.next(bm) )
  {
    QString text = bm.text();
    text.replace( '&', "&&" );
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
      if ( m_parentMenu->count() > 0 )
          m_parentMenu->insertSeparator();
    addAddBookmark();
    addNewFolder();
  }
}

void KonsoleBookmarkMenu::slotBookmarkSelected()
{
    if ( !m_pOwner ) return; // this view doesn't handle bookmarks...
    m_kOwner->openBookmarkURL( QString::fromUtf8(sender()->name()), /* URL */
                               ( (KAction *)sender() )->text() /* Title */ );
}

#include "konsolebookmarkmenu.moc"
