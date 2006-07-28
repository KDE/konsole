#ifndef OVERLAY_FRAME_H
#define OVERLAY_FRAME_H

#include <QFrame>
#include <QTimer>

/** 
 * A translucent frame which is useful as an overlay on top of other widgets
 * to display status information and so on.
 */
class OverlayFrame : public QFrame
{
Q_OBJECT

public:
	OverlayFrame(QWidget* parent) : QFrame(parent) , opacity(0) {}

public slots:
	virtual void setVisible( bool visible );

protected:
	
	virtual void paintEvent(QPaintEvent* event);

protected slots:
	void fadeIn();
	void fadeOut();

private:
	int opacity;
	static const int MAX_OPACITY = 200;
	static const int OPACITY_STEP_OUT = 20;
	static const int OPACITY_STEP_IN = 50;
		
	QTimer displayTimer;
	int elapsed;
	
};

#endif //OVERLAY_FRAME_H
