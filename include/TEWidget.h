/* ----------------------------------------------------------------------- */
/*                                                                         */
/* [te_widget.h]           Terminal Emulation Widget                       */
/*                                                                         */
/* ----------------------------------------------------------------------- */
/*                                                                         */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>         */
/*                                                                         */
/* This file is part of Konsole - an X terminal for KDE                    */
/*                                                                         */
/* ----------------------------------------------------------------------- */

#ifndef TE_WIDGET_H
#define TE_WIDGET_H

#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qcolor.h>
#include <qkeycode.h>
#include <qscrollbar.h>

#include <kpopupmenu.h>

#include "TECommon.h"

extern unsigned short vt100_graphics[32];

class TESession;
class Konsole;

class TEWidget : public QFrame
// a widget representing attributed text
{
   Q_OBJECT

  friend class Konsole;
public:

    TEWidget(QWidget *parent=0, const char *name=0);
    virtual ~TEWidget();

    QColor getDefaultBackColor();

    const ColorEntry* getColorTable() const;
    void              setColorTable(const ColorEntry table[]);

    void setScrollbarLocation(int loc);
    enum { SCRNONE=0, SCRLEFT=1, SCRRIGHT=2 };

    void setScroll(int cursor, int lines);
    void doScroll(int lines);

    void emitSelection();

    void setImage(const ca* const newimg, int lines, int columns);

    int  Lines()   { return lines;   }
    int  Columns() { return columns; }

    void calcGeometry();
    void propagateSize();
    QSize calcSize(int cols, int lins) const;

    QSize sizeHint() const;

    void setWordCharacters(QString wc);

    void Bell();
    void setSelection(const QString &t);

    virtual void setFont(const QFont &);
    void setVTFont(const QFont &);

    void setMouseMarks(bool on);

public slots:

    void onClearSelection();

signals:

    void keyPressedSignal(QKeyEvent *e);
    void mouseSignal(int cb, int cx, int cy);
    void changedImageSizeSignal(int lines, int columns);
    void changedHistoryCursor(int value);
    void configureRequest( TEWidget*, int state, int x, int y );

    void clearSelectionSignal();
    void beginSelectionSignal( const int x, const int y );
    void extendSelectionSignal( const int x, const int y );
    void endSelectionSignal(const bool preserve_line_breaks);


protected:

    virtual void styleChange( QStyle& );

    bool eventFilter( QObject *, QEvent * );

    void drawAttrStr(QPainter &paint, QRect rect,
                     QString& str, ca attr, bool pm, bool clear);
    void paintEvent( QPaintEvent * );

    void resizeEvent(QResizeEvent*);

    void fontChange(const QFont &font);
    void frameChanged();

    void mouseDoubleClickEvent(QMouseEvent* ev);
    void mousePressEvent( QMouseEvent* );
    void mouseReleaseEvent( QMouseEvent* );
    void mouseMoveEvent( QMouseEvent* );

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );
    bool focusNextPrevChild( bool next );
    // Dnd
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);


    virtual int charClass(UINT16) const;

    void clearImage();

protected slots:

    void scrollChanged(int value);
    void blinkEvent();

private:

    QChar (*fontMap)(QChar); // possible vt100 font extention

    bool fixed_font; // has fixed pitch
    int  font_h;     // height
    int  font_w;     // width
    int  font_a;     // ascend

    int blX;    // actual offset (left)
    int brX;    // actual offset (right)
    int bY;     // actual offset

    int lines;
    int columns;
    ca *image; // [lines][columns]

    ColorEntry color_table[TABLE_COLORS];

    bool resizing;
    bool mouse_marks;

    void makeImage();

    QPoint iPntSel; // initial selection point
    QPoint pntSel; // current selection point
    int    actSel; // selection state
    bool    word_selection_mode;
    bool    line_selection_mode;
    bool    preserve_line_breaks;

    QClipboard*    cb;
    QScrollBar* scrollbar;
    int         scrollLoc;
    QString     word_characters;

//#define SCRNONE  0
//#define SCRLEFT  1
//#define SCRRIGHT 2

    bool blinking;   // hide text in paintEvent
    bool hasBlinker; // has characters to blink
    QTimer* blinkT;  // active when hasBlinker
    KPopupMenu* m_drop;
    QString dropText;
    bool possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
                               // after QApplication::doubleClickInterval() delay
    void mouseTripleClickEvent(QMouseEvent* ev);

 public:
    // current session in this widget
    TESession *currentSession;
private slots:
    void drop_menu_activated(int item);
    void tripleClickTimeout();  // resets possibleTripleClick
};

#endif // TE_WIDGET_H
