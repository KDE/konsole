// [kcmkonsole.h]				emacs, this is written in -*-c++-*-

#ifndef KCMCONFIG_include
#define KCMCONFIG_include

#include <qwidget.h>
#include <qlabel.h>
#include <qlistbox.h>
#include <qslider.h>
#include <kcontrol.h>
#include "schema.h"

class PageFrame : public QWidget
{ Q_OBJECT
public:
  PageFrame(QWidget* parent);
  void Contents(const char* header, QWidget* body, const char* footer);
  ~PageFrame();
};

class GeneralPage : public PageFrame
{ Q_OBJECT
public:
  GeneralPage(QWidget* parent);
  ~GeneralPage();
};

class ColorTable : public QLabel
{
public:
  ColorTable(QWidget* parent, int lower, int upper);
  void setSchema(ColorSchema* s);
protected:
  void paintEvent(QPaintEvent* e);
private:
  ColorSchema* schema;
public:
  float scale;
  float shift;
  float color;
private:
  int lower;
  int upper;
};

class SchemaConfig : public PageFrame
{ Q_OBJECT
public:
  SchemaConfig(QWidget* parent);
  ~SchemaConfig();
protected slots: 
  void setSchema(int n);
protected slots: 
  void sl0ValueChanged(int n);
  void sl1ValueChanged(int n);
  void sl2ValueChanged(int n);
private:
  QListBox* lbox;
  ColorTable* colorTableW[6];
  QSlider* sl0; //FIXME: name
  QSlider* sl1; //FIXME: name
  QSlider* sl2; //FIXME: name
};

class SessionConfig : public PageFrame
{ Q_OBJECT
public:
  SessionConfig(QWidget* parent);
  ~SessionConfig();
};

class KcmKonsole : public KControlApplication
{
public:

  KcmKonsole(int &argc, char **arg, const char *name);

  void init();
  void apply();
  void defaultValues();

private:

  SessionConfig*  sessions;
  SchemaConfig*   schemes;
  GeneralPage*    general;
};

#endif
