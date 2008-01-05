/*
    This file is part of the Konsole Terminal.
    
    Copyright (C) 2006 Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef VIEWSPLITTER_H
#define VIEWSPLITTER_H

#include <QtCore/QList>
#include <QtGui/QSplitter>

class QFocusEvent;

namespace Konsole
{

class ViewContainer;

/**
 * A splitter which holds a number of ViewContainer objects and allows
 * the user to control the size of each view container by dragging a splitter
 * bar between them.
 *
 * Each splitter can also contain child ViewSplitter widgets, allowing
 * for a hierarchy of view splitters and containers.
 *
 * The addContainer() method is used to split the existing view and 
 * insert a new view container.
 * Containers can only be removed from the hierarchy by deleting them.
 */
class ViewSplitter : public QSplitter
{
Q_OBJECT

public:
    ViewSplitter(QWidget* parent = 0); 

    /**
     * Locates the child ViewSplitter widget which currently has the focus
     * and inserts the container into it.   
     * 
     * @param container The container to insert
     * @param orientation Specifies whether the view should be split
     *                    horizontally or vertically.  If the orientation
     *                    is the same as the ViewSplitter into which the
     *                    container is to be inserted, or if the splitter
     *                    has fewer than two child widgets then the container
     *                    will be added to that splitter.  If the orientation
     *                    is different, then a new child splitter
     *                    will be created, into which the container will
     *                    be inserted.   
     */
    void addContainer( ViewContainer* container , Qt::Orientation orientation );  

	/** Removes a container from the splitter.  The container is not deleted. */
	void removeContainer( ViewContainer* container );

    /** Returns the child ViewSplitter widget which currently has the focus */
    ViewSplitter* activeSplitter() ;
 
    /** 
     * Returns the container which currently has the focus or 0 if none
     * of the immediate child containers have the focus.  This does not 
     * search through child splitters.  activeSplitter() can be used
     * to search recursively through child splitters for the splitter
     * which currently has the focus.
     *
     * To find the currently active container, use
     * mySplitter->activeSplitter()->activeContainer() where mySplitter
     * is the ViewSplitter widget at the top of the hierarchy.
     */
    ViewContainer* activeContainer() const; 

    /**
     * Gives the focus to the active view in the specified container 
     */
    void setActiveContainer(ViewContainer* container);

    /**
     * Returns a list of the containers held by this splitter
     */
    QList<ViewContainer*> containers() const {return _containers;}
   
    /**
     * Gives the focus to the active view in the next container
     */
    void activateNextContainer();

    /**
     * Changes the size of the specified @p container by a given @p percentage.
     * @p percentage may be positive ( in which case the size of the container 
     * is increased ) or negative ( in which case the size of the container 
     * is decreased ).  
     *
     * The sizes of the remaining containers are increased or decreased
     * uniformly to maintain the width of the splitter.
     */
    void adjustContainerSize(ViewContainer* container , int percentage);

   /**
    * Gives the focus to the active view in the previous container
    */
   void activatePreviousContainer();

   /**
    * Specifies whether the view may be split recursively.
    * 
    * If this is false, all containers will be placed into the same
    * top-level splitter.  Adding a container with an orientation
    * which is different to that specified when adding the previous
    * containers will change the orientation for all dividers 
    * between containers.
    *
    * If this is true, adding a container to the view splitter with
    * an orientation different to the orientation of the previous
    * area will result in the previously active container being
    * replaced with a new splitter containing the active container
    * and the newly added container.  
    */
   void setRecursiveSplitting(bool recursive);

   /** 
    * Returns whether the view may be split recursively.
    * See setRecursiveSplitting() 
    */
   bool recursiveSplitting() const;

signals:
    /** Signal emitted when the last child widget is removed from the splitter */
    void empty(ViewSplitter* splitter);

    /** 
     * Signal emitted when the containers held by this splitter become empty, this
     * differs from the empty() signal which is only emitted when all of the containers
     * are deleted.  This signal is emitted even if there are still container widgets.
     *
     * TODO: This does not yet work recursively (ie. when splitters inside splitters have empty containers)  
     */
    void allContainersEmpty();

protected:
    //virtual void focusEvent(QFocusEvent* event);

private:
    // Adds container to splitter's internal list and
    // connects signals and slots
    void registerContainer( ViewContainer* container );
    // Removes container from splitter's internal list and
    // removes signals and slots
    void unregisterContainer( ViewContainer* container );

    void updateSizes();

private slots:
    // Called to indicate that a child ViewContainer has been deleted
    void containerDestroyed( ViewContainer* object );

    // Called to indicate that a child ViewContainer is empty
    void containerEmpty( ViewContainer* object );

    // Called to indicate that a child ViewSplitter is empty
    // (ie. all child widgets have been deleted)
    void childEmpty( ViewSplitter* splitter );

private:
    QList<ViewContainer*> _containers;
    bool _recursiveSplitting;
};

}
#endif //VIEWSPLITTER_H

