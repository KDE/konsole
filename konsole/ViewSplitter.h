
#ifndef VIEWSPLITTER_H
#define VIEWSPLITTER_H

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
    
signals:
    /** Signal emitted when the last child widget is removed from the splitter */
    void empty(ViewSplitter* splitter);

private:
    // Adds container to splitter's internal list and
    // connects signals and slots
    void registerContainer( ViewContainer* container );
    // Removes container from splitter's internal list and
    // removes signals and slots
    void unregisterContainer( ViewContainer* container );

private slots:
    // Called to indicate that a child ViewContainer has been deleted
    void containerDestroyed( ViewContainer* object );
    // Called to indicate that a child ViewSplitter is empty
    // (ie. all child widgets have been deleted)
    void childEmpty( ViewSplitter* splitter );

private:
    QList<ViewContainer*> _containers;
};

#endif //VIEWSPLITTER_H

