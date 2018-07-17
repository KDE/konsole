#ifndef DETACHABLETABBAR_H
#define DETACHABLETABBAR_H

#include <QTabBar>
#include <QCursor>

class DetachableTabBar : public QTabBar {
    Q_OBJECT
public:
    DetachableTabBar(QWidget *parent = nullptr);
Q_SIGNALS:
    void detachTab(int idx);
protected:
    void mouseMoveEvent(QMouseEvent*event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private:
    bool _draggingOutside;
    QCursor _originalCursor;
};

#endif
