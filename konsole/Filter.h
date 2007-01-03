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

#ifndef FILTER_H
#define FILTER_H

// Qt
#include <QList>
#include <QObject>
#include <QStringList>
#include <QMultiHash>
#include <QRegExp>

class ca;

class Filter
{
public:
    class HotSpot
    {
    public:
       HotSpot(int startLine , int startColumn , int endLine , int endColumn);
       virtual ~HotSpot();

       enum Type
       {
            // the type of the hotspot is not specified
            NotSpecified,
            // this hotspot represents a clickable link
            Link,
            // this hotspot represents a marker
            Marker
       }; 

       int startLine() const;
       int endLine() const;
       int startColumn() const;
       int endColumn() const;
       Type type() const;
       virtual void activate() = 0; 
       virtual QList<QAction*> actions();

    protected:
       void setType(Type type);

    private:
       int    _startLine;
       int    _startColumn;
       int    _endLine;
       int    _endColumn;
       Type _type;
    
    };

    virtual ~Filter();

    virtual void process() = 0;

    void reset();
    void addLine(const QString& string);

    HotSpot* hotSpotAt(int line , int column) const;
    QList<HotSpot*> hotSpots() const;
    QList<HotSpot*> hotSpotsAtLine(int line) const;

protected:
    void addHotSpot(HotSpot*);
    QString& buffer();
    void getLineColumn(int position , int& startLine , int& startColumn);

private:
    QMultiHash<int,HotSpot*> _hotspots;
    QList<HotSpot*> _hotspotList;
    QList<int> _linePositions;
    QString _buffer;
};

class RegExpFilter : public Filter
{
public:
    class HotSpot : public Filter::HotSpot
    {
    public:
        HotSpot(int startLine, int startColumn, int endLine , int endColumn);
        virtual void activate();

        void setCapturedTexts(const QStringList& texts);
        QStringList capturedTexts() const;
    private:
        QStringList _capturedTexts;
    };

    RegExpFilter();

    void setRegExp(const QRegExp& text);
    QRegExp regExp() const;

    virtual void process();

protected:
    virtual RegExpFilter::HotSpot* newHotSpot(int startLine,int startColumn,
                                    int endLine,int endColumn);

private:
    QRegExp _searchText;
};

class FilterObject;
class UrlFilter : public RegExpFilter 
{
public:
    class HotSpot : public RegExpFilter::HotSpot 
    {
    public:
        HotSpot(int startLine,int startColumn,int endLine,int endColumn);
        virtual ~HotSpot();

        virtual QList<QAction*> actions();
        virtual void activate();
    private:
        FilterObject* _urlObject;
    };

    UrlFilter();

protected:
    virtual RegExpFilter::HotSpot* newHotSpot(int,int,int,int);
};

class FilterObject : public QObject
{
Q_OBJECT
public:
    FilterObject(Filter::HotSpot* filter) : _filter(filter) {};
private slots:
    void activated();
private:
    Filter::HotSpot* _filter;
};

class FilterChain : protected QList<Filter*>
{
public:
    void addFilter(Filter* filter);
    void removeFilter(Filter* filter);
    bool containsFilter(Filter* filter);
    void clear();

    void reset();
    void addLine(const QString& line);
    void process();
    Filter::HotSpot* hotSpotAt(int line , int column) const;
    QList<Filter::HotSpot*> hotSpots() const;
    QList<Filter::HotSpot> hotSpotsAtLine(int line) const;

};

class TerminalImageFilterChain : public FilterChain
{
public:
    void addImage(const ca* const image , int lines , int columns);  
};
#endif //FILTER_H
