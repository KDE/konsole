#ifndef KONSOLEMAINWINDOW_H
#define KONSOLEMAINWINDOW_H

#include <KMainWindow>

class ViewSplitter;
class ViewManager;

class KonsoleMainWindow : public KMainWindow
{
    Q_OBJECT

    public:
        KonsoleMainWindow();

    protected:
        void setupActions();
        void setupWidgets();

    private:
        ViewSplitter* _viewSplitter;
        ViewManager*  _viewManager;
};

#endif // KONSOLEMAINWINDOW_H
