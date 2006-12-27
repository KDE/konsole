#include <KMainWindow>

class ViewSplitter;

class KonsoleMainWindow : public KMainWindow
{
    public:
        KonsoleMainWindow();

    protected:
        void setupActions();

    private:
        ViewSplitter* m_viewSplitter;
};
