// [kcmkonsole.h]				emacs, this is written in -*-c++-*-

#ifndef KCMCONFIG_include
#define KCMCONFIG_include

#include <qwidget.h>
#include <qlabel.h>
#include <qlistbox.h>
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
  ColorTable(QWidget* parent);
  void setSchema(ColorSchema* s);
protected:
  void paintEvent(QPaintEvent* e);
private:
  ColorSchema* schema;
};

class SchemaConfig : public PageFrame
{ Q_OBJECT
public:
  SchemaConfig(QWidget* parent);
  ~SchemaConfig();
protected slots: 
  void setSchema(int n);
private:
  QListBox* lbox;
  ColorTable* colorTableW;
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
