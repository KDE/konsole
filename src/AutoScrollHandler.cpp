

#include "AutoScrollHandler.h"
#include <QApplication>
#include <QAccessible>

using namespace Konsole;

AutoScrollHandler::AutoScrollHandler(QWidget* parent)
    : QObject(parent)
    , _timerId(0)
{
    parent->installEventFilter(this);
}

void AutoScrollHandler::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != _timerId) {
        return;
    }

    QMouseEvent mouseEvent(QEvent::MouseMove,
                           widget()->mapFromGlobal(QCursor::pos()),
                           Qt::NoButton,
                           Qt::LeftButton,
                           Qt::NoModifier);

    QApplication::sendEvent(widget(), &mouseEvent);
}

bool AutoScrollHandler::eventFilter(QObject* watched, QEvent* event)
{
    Q_ASSERT(watched == parent());
    Q_UNUSED(watched)

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        bool mouseInWidget = widget()->rect().contains(mouseEvent->pos());
        if (mouseInWidget) {
            if (_timerId != 0) {
                killTimer(_timerId);
            }

            _timerId = 0;
        } else {
            if ((_timerId == 0) && ((mouseEvent->buttons() & Qt::LeftButton) != 0u)) {
                _timerId = startTimer(100);
            }
        }

        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if ((_timerId != 0) && ((mouseEvent->buttons() & ~Qt::LeftButton) != 0u)) {
            killTimer(_timerId);
            _timerId = 0;
        }
        break;
    }
    default:
        break;
    }

    return false;
}
