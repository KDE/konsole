/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [te_widget.h]           Terminal Emulation Widget                          */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef TE_WIDGET_H
#define TE_WIDGET_H

#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qcolor.h>
#include <qkeycode.h>
#include <qscrbar.h>

#include "TECommon.h"

class TEWidget : public QFrame
// a widget representing attributed text
{ Q_OBJECT

public:

    TEWidget(QWidget *parent=0, const char *name=0);

public:

    QColor getDefaultBackColor();

    const ColorEntry* getColorTable() const;
    void              setColorTable(const ColorEntry table[]);
    
    void setScrollbarLocation(int loc);
    void setScroll(int cursor, int lines);

public:
    
    void setImage(const ca* const newimg, int lines, int columns);

    int  Lines()   { return lines;   }
    int  Columns() { return columns; }

    void calcGeometry();
    void propagateSize();
    QSize calcSize(int cols, int lins);

public:

    void Bell();

signals:

    void keyPressedSignal(QKeyEvent *e);
    void mouseSignal(int cb, int cx, int cy);
    void changedImageSizeSignal(int lines, int columns);
    void changedHistoryCursor(int value);
    void configureRequest( TEWidget*, int state, int x, int y );

    void clearSelectionSignal();
    void beginSelectionSignal( const int x, const int y );
    void extendSelectionSignal( const int x, const int y );
    void endSelectionSignal(const BOOL preserve_line_breaks);


protected:

    void drawAttrStr(QPainter &paint, QRect rect, 
                     char* str, int len, ca attr, BOOL pm);
    void paintEvent( QPaintEvent * );

    void resizeEvent(QResizeEvent*);

    void focusInEvent(QFocusEvent*);
    void focusOutEvent(QFocusEvent*);
    void keyPressEvent(QKeyEvent *e);

    void fontChange(const QFont &font);
    void frameChanged();

    void mouseDoubleClickEvent(QMouseEvent* ev);
    void mousePressEvent( QMouseEvent* );
    void mouseReleaseEvent( QMouseEvent* );
    void mouseMoveEvent( QMouseEvent* );

    void emitSelection();
    virtual int charClass(char) const;

public:

    void clearImage(); //FIXME: public for special purpose only (see vt102emu)
    void setSelection(const char *t);

    virtual void setFont(const QFont &);
    void setVTFont(const QFont &);

    void setMouseMarks(bool on);

public slots:

    void onClearSelection();

protected slots:

    void scrollChanged(int value);
    void blinkEvent();

private:
    
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

    BOOL resizing;
    bool mouse_marks;

    void makeImage();

    QPoint iPntSel; // initial selection point
    QPoint pntSel; // current selection point
    int    actSel; // selection state
    BOOL    word_selection_mode;
    BOOL    preserve_line_breaks;

    QClipboard*    cb;
    QScrollBar* scrollbar;
    int         scrollLoc;
//FIXME::enum
#define SCRNONE  0
#define SCRLEFT  1
#define SCRRIGHT 2

    BOOL blinking;   // hide text in paintEvent
    BOOL hasBlinker; // has characters to blink
    QTimer* blinkT;  // active when hasBlinker
};

#endif // TE_WIDGET_H
