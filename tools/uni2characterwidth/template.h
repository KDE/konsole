/*
    This file is part of Konsole, a terminal emulator for KDE.

    SPDX-FileCopyrightText: 2018 Mariusz Glebocki <mglb@arccos-1.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <QMap>
#include <QString>
#include <QVector>

// QVariant doesn't offer modification in place. Var does.
class Var
{
public:
    using Number = qint64;
    using String = QString;
    using Map = QMap<String, Var>;
    using Vector = QVector<Var>;

    enum class DataType {
        Invalid,
        Number,
        String,
        Vector,
        Map,
    };

    const QString dataTypeAsString() const
    {
        switch (dataType()) {
        case DataType::Invalid:
            return QStringLiteral("Invalid");
        case DataType::Number:
            return QStringLiteral("Number");
        case DataType::String:
            return QStringLiteral("String");
        case DataType::Vector:
            return QStringLiteral("Vector");
        case DataType::Map:
            return QStringLiteral("Map");
        default:
            return QStringLiteral("Unknown?");
        }
    }

    Var()
        : num(0)
        , _dataType(DataType::Invalid)
    {
    }
    Var(const Var &other)
    {
        *this = other;
    }

    Var(const Number &newNum)
        : _dataType(DataType::Number)
    {
        new (&num) auto(newNum);
    }
    Var(const String &newStr)
        : _dataType(DataType::String)
    {
        new (&str) auto(newStr);
    }
    Var(const Vector &newVec)
        : _dataType(DataType::Vector)
    {
        new (&vec) auto(newVec);
    }
    Var(const Map &newMap)
        : _dataType(DataType::Map)
    {
        new (&map) auto(newMap);
    }

    // Allow initialization without type name
    Var(const char *newStr)
        : _dataType(DataType::String)
    {
        new (&str) String(QString::fromUtf8(newStr));
    }
    Var(std::initializer_list<Var> newVec)
        : _dataType(DataType::Vector)
    {
        new (&vec) Vector(newVec);
    }

    ~Var()
    {
        switch (dataType()) {
        case DataType::String:
            str.~String();
            break;
        case DataType::Vector:
            vec.~Vector();
            break;
        case DataType::Map:
            map.~Map();
            break;
        default:
            break;
        }
    }

    Var &operator=(const Var &other)
    {
        _dataType = other.dataType();
        switch (other.dataType()) {
        case DataType::Number:
            new (&num) auto(other.num);
            break;
        case DataType::String:
            new (&str) auto(other.str);
            break;
        case DataType::Vector:
            new (&vec) auto(other.vec);
            break;
        case DataType::Map:
            new (&map) auto(other.map);
            break;
        default:
            break;
        }
        return *this;
    }

    Var &operator[](unsigned index)
    {
        Q_ASSERT(_dataType == DataType::Vector);
        return vec.data()[index];
    }
    const Var &operator[](unsigned index) const
    {
        Q_ASSERT(_dataType == DataType::Vector);
        return vec.constData()[index];
    }
    Var &operator[](const String &key)
    {
        Q_ASSERT(_dataType == DataType::Map);
        return map[key];
    }
    const Var &operator[](const String &key) const
    {
        Q_ASSERT(_dataType == DataType::Map);
        return *map.find(key);
    }

    DataType dataType() const
    {
        return _dataType;
    }

    union {
        Number num;
        String str;
        Vector vec;
        Map map;
    };

private:
    DataType _dataType;
};

class Template
{
public:
    Template(const QString &text);
    void parse();
    QString generate(const Var &data);

    struct Element {
        Element(const Element *parent = nullptr, const QString &name = QString())
            : outer()
            , inner()
            , name(name)
            , fmt()
            , line(0)
            , column(0)
            , isComment(false)
            , children()
            , parent(parent)
        {
        }

        Element(const Element &other)
            : outer(other.outer)
            , inner(other.inner)
            , name(other.name)
            , fmt(other.fmt)
            , line(other.line)
            , column(other.column)
            , isComment(other.isComment)
            , parent(other.parent)
        {
            for (const auto &child : other.children) {
                children.append(child);
            }
        }

        const QString findFmt(Var::DataType type) const;
        QString path() const;
        bool isCommand() const
        {
            return name.startsWith(QLatin1Char('!'));
        }
        bool hasName() const
        {
            return !isCommand() && !name.isEmpty();
        }

        static const QString defaultFmt(Var::DataType type);
        static bool isValidFmt(const QString &fmt, Var::DataType type);

        QStringRef outer;
        QStringRef inner;
        QString name;
        QString fmt;
        uint line;
        uint column;
        bool isComment;
        QList<Element> children;
        const Element *parent;
    };

private:
    void executeCommand(Element &element, const Element &childStub, const QStringList &argv);
    void parseRecursively(Element &element);
    int generateRecursively(QString &result, const Element &element, const Var &data, int consumed = 0);

    QString _text; // FIXME: make it pointer (?)
    Element _root; // FIXME: make it pointer
};

#endif
