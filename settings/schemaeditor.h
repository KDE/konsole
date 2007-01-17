/***************************************************************************
                          schemaeditor.h  -  description
                             -------------------
    begin                : mar apr 17 16:44:59 CEST 2001
    copyright            : (C) 2001 by Andrea Rizzi
    email                : rizzi@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SCHEMAEDITOR_H
#define SCHEMAEDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kapplication.h>
#include <QWidget>
#include <QPixmap>
#include <QVector>

#include "ui_schemadialog.h"

class SchemaDialog : public QWidget, public Ui::SchemaDialog
{
public:
  SchemaDialog( QWidget *parent ) : QWidget( parent ) {
    setupUi( this );
  }
};


/** SchemaEditor is the base class of the project */
class SchemaEditor : public SchemaDialog
{
  Q_OBJECT
  public:
    /** constructor */
    SchemaEditor(QWidget* parent=0);
    /** destructor */
    ~SchemaEditor();

    QString schema();
    void setSchema(QString);
    bool isModified() const { return schMod; }
    void querySave();

  Q_SIGNALS:
  	void changed();
	void schemaListChanged(const QStringList &titles, const QStringList &filenames);

  public Q_SLOTS:
  	void slotColorChanged(int);
  	void imageSelect();
	void slotTypeChanged(int);
	void readSchema(int);
	void saveCurrent();
	void removeCurrent();
	void getList();
  private Q_SLOTS:
	void schemaModified();
	void loadAllSchema(QString currentFile="");
	void updatePreview();
  private:
	void load();
	bool schMod;
  	QVector<QColor> color;
	QVector<int> type; // 0= custom, 1= sysfg, 2=sysbg, 3=rcolor
	QVector<bool> transparent;
	QVector<bool> bold;
	QPixmap pix;
	QString defaultSchema;
	bool loaded;
	bool schemaLoaded;
	bool change;
	int oldSchema;
	int oldSlot;
	QString readSchemaTitle(const QString& filename);
	void schemaListChanged();

};

#endif
