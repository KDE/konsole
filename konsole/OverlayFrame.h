/*
    This file is part of Konsole, an X terminal.
    (C) 2006 Robert Knight
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

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
