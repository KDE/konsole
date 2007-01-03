/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// System
#include <iostream>

// Qt
#include <QAction>
#include <QApplication>
#include <QString>
#include <QtDebug>
#include <QSharedData>
#include <QFile>

// KDE
#include <KLocale>
#include <KRun>

// Konsole
#include "Filter.h"
#include "TerminalCharacterDecoder.h"

void FilterChain::addFilter(Filter* filter)
{
    append(filter);
}
void FilterChain::removeFilter(Filter* filter)
{
    removeAll(filter);
}
bool FilterChain::containsFilter(Filter* filter)
{
    return contains(filter);
}
void FilterChain::reset()
{
    QListIterator<Filter*> iter(*this);
    while (iter.hasNext())
        iter.next()->reset();
}
void FilterChain::process()
{
    QListIterator<Filter*> iter(*this);
    while (iter.hasNext())
        iter.next()->process();
}
void FilterChain::addLine(const QString& line)
{
    QListIterator<Filter*> iter(*this);
    while (iter.hasNext())
        iter.next()->addLine(line);
}
void FilterChain::clear()
{
    QList<Filter*>::clear();
}
Filter::HotSpot* FilterChain::hotSpotAt(int line , int column) const
{
    QListIterator<Filter*> iter(*this);
    while (iter.hasNext())
    {
        Filter* filter = iter.next();
        Filter::HotSpot* spot = filter->hotSpotAt(line,column);
        if ( spot != 0 )
        {
            return spot;
        }
    }

    return 0;
}

QList<Filter::HotSpot*> FilterChain::hotSpots() const
{
    QList<Filter::HotSpot*> list;
    QListIterator<Filter*> iter(*this);
    while (iter.hasNext())
    {
        Filter* filter = iter.next();
        list << filter->hotSpots();
    }
    return list;
}
//QList<Filter::HotSpot*> FilterChain::hotSpotsAtLine(int line) const;

void TerminalImageFilterChain::addImage(const ca* const image , int lines , int columns)
{
    if (empty())
        return;

    PlainTextDecoder decoder;
    decoder.setTrailingWhitespace(false);
    QString line;
    QTextStream lineStream(&line);

    for (int i=0 ; i < lines ; i++)
    {
        decoder.decodeLine(image + i*columns,columns,0,&lineStream);
        addLine(line);
        line.clear();
    }
}

Filter::~Filter()
{
    QListIterator<HotSpot*> iter(_hotspotList);
    while (iter.hasNext())
    {
        delete iter.next();
    }
}
void Filter::reset()
{
    _hotspots.clear();
    _hotspotList.clear();
    _linePositions.clear();
    _buffer.clear();
}

void Filter::getLineColumn(int position , int& startLine , int& startColumn)
{
    for (int i = 0 ; i < _linePositions.count() ; i++)
    {
        int nextLine = 0;

        if ( i == _linePositions.count()-1 )
        {
            nextLine = _buffer.length();
        }
        else
        {
            nextLine = _linePositions[i+1];
        }

        if ( _linePositions[i] <= position && position <= nextLine ) 
        {
            startLine = i;
            startColumn = position - _linePositions[i];
            return;
        }
    }
}
    

void Filter::addLine(const QString& text)
{
    _linePositions << _buffer.length();
    _buffer.append(text);
}
QString& Filter::buffer()
{
    return _buffer;
}
Filter::HotSpot::~HotSpot()
{
}
void Filter::addHotSpot(HotSpot* spot)
{
    _hotspotList << spot;

    for (int line = spot->startLine() ; line <= spot->endLine() ; line++)
    {
        _hotspots.insert(line,spot);
    }    
}
QList<Filter::HotSpot*> Filter::hotSpots() const
{
    return _hotspotList;
}
QList<Filter::HotSpot*> Filter::hotSpotsAtLine(int line) const
{
    return _hotspots.values(line);
}

Filter::HotSpot* Filter::hotSpotAt(int line , int column) const
{
    QListIterator<HotSpot*> spotIter(_hotspots.values(line));

    while (spotIter.hasNext())
    {
        HotSpot* spot = spotIter.next();
        
        if ( spot->startLine() == line && spot->startColumn() > column )
            continue;
        if ( spot->endLine() == line && spot->endColumn() < column )
            continue;
       
        return spot;
    }

    return 0;
}

Filter::HotSpot::HotSpot(int startLine , int startColumn , int endLine , int endColumn)
    : _startLine(startLine)
    , _startColumn(startColumn)
    , _endLine(endLine)
    , _endColumn(endColumn)
    , _type(NotSpecified)
{
}
QList<QAction*> Filter::HotSpot::actions()
{
    return QList<QAction*>();
}
int Filter::HotSpot::startLine() const
{
    return _startLine;
}
int Filter::HotSpot::endLine() const
{
    return _endLine;
}
int Filter::HotSpot::startColumn() const
{
    return _startColumn;
}
int Filter::HotSpot::endColumn() const
{
    return _endColumn;
}
Filter::HotSpot::Type Filter::HotSpot::type() const
{
    return _type;
}
void Filter::HotSpot::setType(Type type)
{
    _type = type;
}

RegExpFilter::RegExpFilter()
{
}

RegExpFilter::HotSpot::HotSpot(int startLine,int startColumn,int endLine,int endColumn)
    : Filter::HotSpot(startLine,startColumn,endLine,endColumn)
{
    setType(Marker);
}

void RegExpFilter::HotSpot::activate()
{
}

void RegExpFilter::HotSpot::setCapturedTexts(const QStringList& texts)
{
    _capturedTexts = texts;
}
QStringList RegExpFilter::HotSpot::capturedTexts() const
{
    return _capturedTexts;
}

void RegExpFilter::setRegExp(const QRegExp& regExp) 
{
    _searchText = QRegExp(regExp);
}
QRegExp RegExpFilter::regExp() const
{
    return _searchText;
}
/*void RegExpFilter::reset(int)
{
    _buffer = QString::null;
}*/
void RegExpFilter::process()
{
    int pos = 0;
    QString& text = buffer();

    while(pos >= 0)
    {
        pos = _searchText.indexIn(text,pos);

        if ( pos >= 0 )
        {

            int startLine = 0;
            int endLine = 0;
            int startColumn = 0;
            int endColumn = 0;

            getLineColumn(pos,startLine,startColumn);
            getLineColumn(pos + _searchText.matchedLength(),endLine,endColumn);

            RegExpFilter::HotSpot* spot = newHotSpot(startLine,startColumn,
                                           endLine,endColumn);
            spot->setCapturedTexts(_searchText.capturedTexts());

            addHotSpot( spot );  
            pos += _searchText.matchedLength();
        }
    }    
}

RegExpFilter::HotSpot* RegExpFilter::newHotSpot(int startLine,int startColumn,
                                                int endLine,int endColumn)
{
    return new RegExpFilter::HotSpot(startLine,startColumn,
                                                  endLine,endColumn);
}
RegExpFilter::HotSpot* UrlFilter::newHotSpot(int startLine,int startColumn,int endLine,
                                                    int endColumn)
{
    return new UrlFilter::HotSpot(startLine,startColumn,
                                               endLine,endColumn);
}
UrlFilter::HotSpot::HotSpot(int startLine,int startColumn,int endLine,int endColumn)
: RegExpFilter::HotSpot(startLine,startColumn,endLine,endColumn)
, _urlObject(new FilterObject(this))
{
    setType(Link);
}
void UrlFilter::HotSpot::activate()
{
    const QStringList& texts = capturedTexts();

    QString url = texts.first();
    // if the URL path does not include the protocol ( eg. "www.kde.org" ) then
    // prepend http:// ( eg. "www.kde.org" --> "http://www.kde.org" )
    if (!url.contains("://"))
    {
        url.prepend("http://");
    }

    if ( texts.count() > 0 )
    {
        new KRun(url,QApplication::activeWindow());
    }
}
UrlFilter::UrlFilter()
{
    //regexp matches:
    //  protocolname:// or www. followed by numbers, letters dots and dashes 
    setRegExp(QRegExp("([a-z]+://|www\\.)[a-zA-Z0-9\\-\\./]+"));
}
UrlFilter::HotSpot::~HotSpot()
{
    delete _urlObject;
}
void FilterObject::activated()
{
    _filter->activate();
}
QList<QAction*> UrlFilter::HotSpot::actions()
{
    QList<QAction*> list;

    QAction* openAction = new QAction(i18n("Open Link"),_urlObject);
    QObject::connect( openAction , SIGNAL(triggered()) , _urlObject , SLOT(activated()) );
    list << openAction;

    return list; 
}

#include "Filter.moc"
