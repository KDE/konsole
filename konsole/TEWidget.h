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

#include <qbitarray.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qcolor.h>
#include <qkeycode.h>
#include <qscrollbar.h>

#include <kpopupmenu.h>

#include "TECommon.h"

extern unsigned short vt100_graphics[32];

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

    bool blinkingCursor() { return hasBlinkingCursor; }
    void setBlinkingCursor(bool blink);

    void setCtrlDrag(bool enable) { ctrldrag=enable; }
    bool ctrlDrag() { return ctrldrag; }

    void setCutToBeginningOfLine(bool enable) { cuttobeginningofline=enable; }
    bool cutToBeginningOfLine() { return cuttobeginningofline; }

    void setLineSpacing(uint);
    uint lineSpacing() const;

    void emitSelection(bool useXselection,bool appendReturn);
    void emitText(QString text);

    void setImage(const ca* const newimg, int lines, int columns);
    void setLineWrapped(QBitArray line_wrapped) { m_line_wrapped=line_wrapped; }

    void setCursorPos(const int curx, const int cury);

    int  Lines()   { return lines;   }
    int  Columns() { return columns; }

    void calcGeometry();
    void propagateSize();
    QSize calcSize(int cols, int lins) const;

    QSize sizeHint() const;

    void setWordCharacters(QString wc);
    QString wordCharacters() { return word_characters; }

    void setBellMode(int mode);
    int isBellMode() { return bellMode; }
    enum { BELLNONE=0, BELLSYSTEM=1, BELLVISUAL=2 };
    void Bell();

    void setSelection(const QString &t);

    virtual void setFont(const QFont &);
    void setVTFont(const QFont &);

    void setMouseMarks(bool on);
    static void setAntialias( bool enable ) { s_antialias = enable; }
    static bool antialias()                 { return s_antialias;   }

    void setTerminalSizeHint(bool on) { terminalSizeHint=on; }
    bool isTerminalSizeHint() { return terminalSizeHint; }
    void setTerminalSizeStartup(bool on) { terminalSizeStartup=on; }

public slots:

    void copyClipboard();
    void pasteClipboard();
    void onClearSelection();

signals:

    void keyPressedSignal(QKeyEvent *e);
    void mouseSignal(int cb, int cx, int cy);
    void changedImageSizeSignal(int lines, int columns);
    void changedHistoryCursor(int value);
    void configureRequest( TEWidget*, int state, int x, int y );

    void copySelectionSignal();
    void clearSelectionSignal();
    void beginSelectionSignal( const int x, const int y );
    void extendSelectionSignal( const int x, const int y );
    void endSelectionSignal(const bool preserve_line_breaks);
    void isBusySelecting(bool);
    void testIsSelected(const int x, const int y, bool &selected /* result */);
  void sendStringToEmu(const char*);

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
    void wheelEvent( QWheelEvent* );

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );
    bool focusNextPrevChild( bool next );
    // Dnd
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    void doDrag();
    enum DragState { diNone, diPending, diDragging };

    struct _dragInfo {
      DragState       state;
      QPoint          start;
      QTextDrag       *dragObject;
    } dragInfo;

    virtual int charClass(UINT16) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent* ev);

protected slots:

    void scrollChanged(int value);
    void blinkEvent();
    void blinkCursorEvent();

private:

    QChar (*fontMap)(QChar); // possible vt100 font extention

    bool fixed_font; // has fixed pitch
    int  font_h;     // height
    int  font_w;     // width
    int  font_a;     // ascend

    int bX;    // offset
    int bY;    // offset

    int lines;
    int columns;
    ca *image; // [lines][columns]
    int image_size;
    QBitArray m_line_wrapped;

    ColorEntry color_table[TABLE_COLORS];

    bool resizing;
    bool terminalSizeHint,terminalSizeStartup;
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
    int         bellMode;

    bool blinking;   // hide text in paintEvent
    bool hasBlinker; // has characters to blink
    bool cursorBlinking;     // hide cursor in paintEvent
    bool hasBlinkingCursor;  // has blinking cursor enabled
    bool ctrldrag;           // require Ctrl key for drag
    bool cuttobeginningofline; // triple click only selects forward
    bool isBlinkEvent; // paintEvent due to blinking.
    QTimer* blinkT;  // active when hasBlinker
    QTimer* blinkCursorT;  // active when hasBlinkingCursor
    
    KPopupMenu* m_drop;
    QString dropText;
    int m_dnd_file_count;

    bool possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
                               // after QApplication::doubleClickInterval() delay

    static bool s_antialias; // do we antialias or not

    QFrame *mResizeWidget;
    QLabel *mResizeLabel;
    QTimer *mResizeTimer;

    uint m_lineSpacing;

    QRect       cursorRect; //for quick changing of cursor

private slots:
    void drop_menu_activated(int item);
    void swapColorTable();
    void tripleClickTimeout();  // resets possibleTripleClick
};

#endif // TE_WIDGET_H
