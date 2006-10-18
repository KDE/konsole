#ifndef VIEWCONTAINER_H
#define VIEWCONTAINER_H

/**
 * An interface for container widgets which can hold one or more views.
 *
 * The container widget typically displays a list of the views which
 * it has and provides a means of switching between them.  
 *
 * Subclasses should reimplement the viewAdded() and viewRemoved() functions
 * to actually add or remove view widgets from the container widget, as well
 * as updating any navigation aids.
 * 
 */
class ViewContainer : public QObject
{
Q_OBJECT
    
public:
    virtual ~ViewContainer() { emit destroyed(this); }

    /** Returns the widget which contains the view widgets */
    virtual QWidget* containerWidget() const = 0;

    /** Adds a new view to the container widget */
    void addView(QWidget* view);
    /** Removes a view from the container */
    void removeView(QWidget* view);

    /** Returns a list of the contained views */
    const QList<QWidget*> views();
   
    /** 
     * Returns the view which currently has the focus or 0 if none
     * of the child views have the focus.
     */ 
    virtual QWidget* activeView() const = 0;

signals:
    /** Emitted when the container is deleted */
    void destroyed(ViewContainer* container);

protected:
    /** 
     * Performs the task of adding the view widget
     * to the container widget.
     */
    virtual void viewAdded(QWidget* view) = 0;
    /**
     * Performs the task of removing the view widget
     * from the container widget.
     */
    virtual void viewRemoved(QWidget* view) = 0;
    

private:
    QList<QWidget*> _views;
};

/** 
 * A view container which uses a QTabWidget to display the views and 
 * allow the user to switch between them.
 */
class TabbedViewContainer : public ViewContainer  
{
public:
    TabbedViewContainer();
    virtual ~TabbedViewContainer();
    
    virtual QWidget* containerWidget() const;
    virtual QWidget* activeView() const;

protected:
    virtual void viewAdded( QWidget* view );
    virtual void viewRemoved( QWidget* view ); 

private:
    QTabWidget* _tabWidget;  

    static int debug; 

};

#endif //VIEWCONTAINER_H
