// Qt
#include <QApplication>
#include <QPainter>

// local
#include "OverlayFrame.h"

void OverlayFrame::setVisible( bool visible )
{
		elapsed = 0;

		if ( visible )
		{
				QFrame::setVisible(true);
				displayTimer.disconnect();
				connect(&displayTimer , SIGNAL(timeout()) , this , SLOT(fadeIn()) );
				displayTimer.start(50);	
		}
		else
		{
				if (opacity <= 0)
						QFrame::setVisible(false);
				else
				{
						displayTimer.disconnect();
						connect(&displayTimer , SIGNAL(timeout()) , this , SLOT(fadeOut()) );
						displayTimer.start(50);
				}
		}
}

void OverlayFrame::paintEvent(QPaintEvent* /*event*/)
{
		QPalette palette = QApplication::palette();

		QPainter painter;
		painter.begin(this);

		//setup translucent background 
		QColor background = palette.color(QPalette::Window);
		background.setAlpha( opacity );

		QBrush backgroundBrush(background);
		painter.setBrush(backgroundBrush);

		//setup widget outline using a gradient based on the widget's 3D bevel colours.
		QLinearGradient gradient;
	
		QRect area = frameRect();
		int dx = (elapsed / 10); 
		
		gradient.setStart( area.left() + dx , area.top() );
		gradient.setFinalStop( area.right() - dx , area.bottom() );
		
		QColor darkColor = palette.color(QPalette::Dark);
		darkColor.setAlpha( opacity );
		QColor midColor = palette.color(QPalette::Mid);
		midColor.setAlpha( opacity );

		gradient.setColorAt(0,darkColor);
		gradient.setColorAt(1,midColor);

		QPen borderPen;
		int borderWidth = midLineWidth()+lineWidth();
		borderPen.setWidth( borderWidth );
		borderPen.setBrush( QBrush(gradient) );
		painter.setPen(borderPen);

		//draw the frame border.
		//frameRect() is shrunk on all sides by the pen width to ensure that the whole
		//rectangle (outline+fill) is visible.
		painter.drawRect( frameRect().adjusted(  borderWidth,
								borderWidth,
								-borderWidth,
								-borderWidth ) /*, radius*/ );

		painter.end();
}

void OverlayFrame::fadeIn()
{
		elapsed += displayTimer.interval();
		if (opacity < MAX_OPACITY)
		{
				opacity += OPACITY_STEP_IN;
		}
		else
		{
				
				//elapsed=0;
				//displayTimer.stop();
		}

		update();
}

void OverlayFrame::fadeOut()
{
		if (opacity > 0)
		{
				elapsed += displayTimer.interval();
				opacity -= OPACITY_STEP_OUT;
				update();
		}
		else
		{
				elapsed = 0;
				displayTimer.stop();	
				hide();
		}
}

#include "OverlayFrame.moc"
