/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2018 by Mariusz Glebocki <mglb@arccos-1.net>

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

#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include "template.h"

static const QString unescape(const QStringRef &str) {
    QString result;
    result.reserve(str.length());
    for(int i = 0; i < str.length(); ++i) {
        if(i < str.length() - 1 && str[i] == QLatin1Char('\\'))
            result += str[++i];
        else
            result += str[i];
    }
    return result;
}

//
// Template::Element
//
const QString Template::Element::findFmt(Var::DataType type) const {
    const Template::Element *element;
    for(element = this; element != nullptr; element = element->parent) {
        if(!element->fmt.isEmpty() && isValidFmt(element->fmt, type)) {
            return element->fmt;
        }
    }
    return defaultFmt(type);
}

QString Template::Element::path() const {
    QStringList namesList;
    const Template::Element *element;
    for(element = this; element != nullptr; element = element->parent) {
        if(!element->hasName() && element->parent != nullptr) {
            QString anonName = QStringLiteral("[anon]");
            for(int i = 0; i < element->parent->children.size(); ++i) {
                if(&element->parent->children[i] == element) {
                    anonName = QStringLiteral("[%1]").arg(i);
                    break;
                }
            }
            namesList.prepend(anonName);
        } else {
            namesList.prepend(element->name);
        }
    }
    return namesList.join(QLatin1Char('.'));
}

const QString Template::Element::defaultFmt(Var::DataType type) {
    switch(type) {
        case Var::DataType::Number: return QStringLiteral("%d");
        case Var::DataType::String: return QStringLiteral("%s");
        default: Q_UNREACHABLE();
    }
}

bool Template::Element::isValidFmt(const QString &fmt, Var::DataType type) {
    switch(type) {
        case Var::DataType::String: return fmt.endsWith(QLatin1Char('s'));
        case Var::DataType::Number: return true; // regexp in parser takes care of it
        default: return false;
    }
}

//
// Template
//

Template::Template(const QString &text): _text(text) {
    _root.name      = QStringLiteral("[root]");
    _root.outer     = QStringRef(&_text);
    _root.inner     = QStringRef(&_text);
    _root.parent    = nullptr;
    _root.line      = 1;
    _root.column    = 1;
}

void Template::parse() {
    _root.children.clear();
    _root.outer = QStringRef(&_text);
    _root.inner = QStringRef(&_text);
    parseRecursively(_root);
//    dbgDumpTree(_root);
}

QString Template::generate(const Var &data) {
    QString result;
    result.reserve(_text.size());
    generateRecursively(result, _root, data);
    return result;
}

static inline void warn(const Template::Element &element, const QString &id, const QString &msg) {
    const QString path = id.isEmpty() ? element.path() : Template::Element(&element, id).path();
    qWarning() << QStringLiteral("Warning: %1:%2: %3: %4").arg(element.line).arg(element.column).arg(path, msg);
}
static inline void warn(const Template::Element &element, const QString &msg) {
    warn(element, QString(), msg);
}

void Template::executeCommand(Element &element, const Template::Element &childStub, const QStringList &argv) {
    // Insert content N times
    if(argv[0] == QStringLiteral("repeat")) {
        bool ok;
        unsigned count = argv.value(1).toInt(&ok);
        if(!ok || count < 1) {
            warn(element, QStringLiteral("!") + argv[0], QStringLiteral("invalid repeat count (%1), assuming 0.").arg(argv[1]));
            return;
        }

        element.children.append(childStub);
        Template::Element &cmdElement = element.children.last();
        if(!cmdElement.inner.isEmpty()) {
            // Parse children
            parseRecursively(cmdElement);
            // Remember how many children was there before replication
            int originalChildrenCount = cmdElement.children.size();
            // Replicate children
            for(unsigned i = 1; i < count; ++i) {
                for(int chId = 0; chId < originalChildrenCount; ++chId) {
                    cmdElement.children.append(cmdElement.children[chId]);
                }
            }
        }
    // Set printf-like format (with leading %) applied for strings and numbers
    // inside the group
    } else if(argv[0] == QStringLiteral("fmt")) {
        static const QRegularExpression FMT_RE(QStringLiteral(R":(^%[-0 +#]?(?:[1-9][0-9]*)?\.?[0-9]*[diouxXs]$):"),
                                               QRegularExpression::OptimizeOnFirstUsageOption);
        const auto match = FMT_RE.match(argv.value(1));
        QString fmt = QStringLiteral("");
        if(!match.hasMatch())
            warn(element, QStringLiteral("!") + argv[0], QStringLiteral("invalid format (%1), assuming default").arg(argv[1]));
        else
            fmt = match.captured();

        element.children.append(childStub);
        Template::Element &cmdElement = element.children.last();
        cmdElement.fmt = fmt;
        parseRecursively(cmdElement);
    }
}

void Template::parseRecursively(Element &element) {
    static const QRegularExpression RE(QStringLiteral(R":((?'comment'«\*(([^:]*):)?.*?(?(-2):\g{-1})\*»)|):"
                                                      R":(«(?:(?'name'[-_a-zA-Z0-9]*)|(?:!(?'cmd'[-_a-zA-Z0-9]+(?: +(?:[^\\:]+|(?:\\.)+)+)?)))):"
                                                      R":((?::(?:~[ \t]*\n)?(?'inner'(?:[^«]*?|(?R))*))?(?:\n[ \t]*~)?»):"),
                                       QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption |
                                       QRegularExpression::OptimizeOnFirstUsageOption);
    static const QRegularExpression CMD_SPLIT_RE(QStringLiteral(R":((?:"((?:(?:\\.)*|[^"]*)*)"|(?:[^\\ "]+|(?:\\.)+)+)):"),
                                                 QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption |
                                                 QRegularExpression::OptimizeOnFirstUsageOption);
    static const QRegularExpression UNESCAPE_RE(QStringLiteral(R":(\\(.)):"),
                                                 QRegularExpression::DotMatchesEverythingOption | QRegularExpression::MultilineOption |
                                                 QRegularExpression::OptimizeOnFirstUsageOption);
    static const QString nameGroupName      = QStringLiteral("name");
    static const QString innerGroupName     = QStringLiteral("inner");
    static const QString cmdGroupName       = QStringLiteral("cmd");
    static const QString commentGroupName   = QStringLiteral("comment");

    int  posOffset = element.outer.position();
    uint posLine   = element.line;
    uint posColumn = element.column;

    auto matchIter = RE.globalMatch(element.inner);
    while(matchIter.hasNext()) {
        auto match      = matchIter.next();
        auto cmd        = match.captured(cmdGroupName);
        auto comment    = match.captured(commentGroupName);

        const auto localOuterRef = match.capturedRef(0);
        const auto localInnerRef = match.capturedRef(innerGroupName);

        auto outerRef = QStringRef(&_text, localOuterRef.position(), localOuterRef.length());
        auto innerRef = QStringRef(&_text, localInnerRef.position(), localInnerRef.length());

        while(posOffset < outerRef.position() && posOffset < _text.size()) {
            if(_text[posOffset++] == QLatin1Char('\n')) {
                ++posLine;
                posColumn = 1;
            } else {
                ++posColumn;
            }
        }

        if(!cmd.isEmpty()) {
            QStringList cmdArgv;
            auto cmdArgIter = CMD_SPLIT_RE.globalMatch(cmd);
            while(cmdArgIter.hasNext()) {
                auto cmdArg = cmdArgIter.next();
                cmdArgv += cmdArg.captured(cmdArg.captured(1).isEmpty() ? 0 : 1);
                cmdArgv.last().replace(UNESCAPE_RE, QStringLiteral("\1"));
            }

            Template::Element childStub = Template::Element(&element);
            childStub.outer     = outerRef;
            childStub.name      = QLatin1Char('!') + cmd;
            childStub.inner     = innerRef;
            childStub.line      = posLine;
            childStub.column    = posColumn;
            executeCommand(element, childStub, cmdArgv);
        } else if (!comment.isEmpty()) {
            element.children.append(Element(&element));
            Template::Element &child = element.children.last();
            child.outer        = outerRef;
            child.name         = QString();
            child.inner        = QStringRef();
            child.line         = posLine;
            child.column       = posColumn;
            child.isComment    = true;
        } else {
            element.children.append(Element(&element));
            Template::Element &child = element.children.last();
            child.outer     = outerRef;
            child.name      = match.captured(nameGroupName);
            child.inner     = innerRef;
            child.line      = posLine;
            child.column    = posColumn;
            if(!child.inner.isEmpty())
                parseRecursively(child);
        }
    }
}

int Template::generateRecursively(QString &result, const Template::Element &element, const Var &data, int consumed) {
    int consumedDataItems = consumed;

    if(!element.children.isEmpty()) {
        int totalDataItems;
        switch(data.dataType()) {
            case Var::DataType::Number:
            case Var::DataType::String:
            case Var::DataType::Map:
                totalDataItems = 1;
                break;
            case Var::DataType::Vector:
                totalDataItems = data.vec.size();
                break;
            case Var::DataType::Invalid:
            default:
                Q_UNREACHABLE();
        }

        while(consumedDataItems < totalDataItems) {
            int prevChildEndPosition = element.inner.position();
            for(const auto &child: element.children) {
                const int characterCountBetweenChildren = child.outer.position() - prevChildEndPosition;
                if(characterCountBetweenChildren > 0) {
                    // Add text between previous child (or inner beginning) and this child.
                    result += unescape(_text.midRef(prevChildEndPosition, characterCountBetweenChildren));
                } else if(characterCountBetweenChildren < 0) {
                    // Repeated item; they overlap and end1 > start2
                    result += unescape(element.inner.mid(prevChildEndPosition - element.inner.position()));
                    result += unescape(element.inner.left(child.outer.position() - element.inner.position()));
                }

                switch(data.dataType()) {
                case Var::DataType::Number:
                case Var::DataType::String:
                    generateRecursively(result, child, data);
                    consumedDataItems = 1; // Deepest child always consumes number/string
                    break;
                case Var::DataType::Vector:
                    if(!data.vec.isEmpty()) {
                        if(!child.hasName() && !child.isCommand() && consumedDataItems < data.vec.size()) {
                            consumedDataItems += generateRecursively(result, child, data[consumedDataItems]);
                        } else {
                            consumedDataItems += generateRecursively(result, child, data.vec.mid(consumedDataItems));
                        }
                    } else {
                        warn(child, QStringLiteral("no more items available in parent's list."));
                    }
                    break;
                case Var::DataType::Map:
                    if(!child.hasName()) {
                        consumedDataItems = generateRecursively(result, child, data);
                    } else if(data.map.contains(child.name)) {
                        generateRecursively(result, child, data.map[child.name]);
                        // Always consume, repeating doesn't change anything
                        consumedDataItems = 1;
                    } else {
                        warn(child, QStringLiteral("missing value for the element in parent's map."));
                    }
                    break;
                default:
                    break;
                }
                prevChildEndPosition = child.outer.position() + child.outer.length();
            }

            result += unescape(element.inner.mid(prevChildEndPosition - element.inner.position(), -1));

            if(element.isCommand()) {
                break;
            }

            const bool isLast = consumedDataItems >= totalDataItems;
            if(!isLast) {
                // Collapse empty lines between elements
                int nlNum = 0;
                for(int i = 0; i < element.inner.size() / 2; ++i) {
                    if(element.inner.at(i) == QLatin1Char('\n') &&
                            element.inner.at(i) == element.inner.at(element.inner.size() - i - 1))
                        nlNum++;
                    else
                        break;
                }
                if(nlNum > 0)
                    result.chop(nlNum);
            }
        }
    } else if (!element.isComment) {
        // Handle leaf element
        switch(data.dataType()) {
            case Var::DataType::Number: {
                const QString fmt = element.findFmt(Var::DataType::Number);
                result += QString::asprintf(qUtf8Printable(fmt), data.num);
                break;
            }
            case Var::DataType::String: {
                const QString fmt = element.findFmt(Var::DataType::String);
                result += QString::asprintf(qUtf8Printable(fmt), qUtf8Printable(data.str));
                break;
            }
            case Var::DataType::Vector:
                if(data.vec.isEmpty()) {
                    warn(element, QStringLiteral("got empty list."));
                } else if(data.vec.at(0).dataType() == Var::DataType::Number) {
                    const QString fmt = element.findFmt(Var::DataType::Number);
                    result += QString::asprintf(qUtf8Printable(fmt), data.num);
                } else if(data.vec.at(0).dataType() == Var::DataType::String) {
                    const QString fmt = element.findFmt(Var::DataType::String);
                    result += QString::asprintf(qUtf8Printable(fmt), qUtf8Printable(data.str));
                } else {
                    warn(element, QStringLiteral("the list entry data type (%1) is not supported in childrenless elements.").
                                  arg(data.vec.at(0).dataTypeAsString()));
                }
                break;
            case Var::DataType::Map:
                warn(element, QStringLiteral("map type is not supported in childrenless elements."));
                break;
            case Var::DataType::Invalid:
                break;
        }
        consumedDataItems = 1;
    }

    return consumedDataItems;
}

/*
void dbgDumpTree(const Template::Element &element) {
    static int indent = 0;
    QString type;
    if(element.isCommand())
        type = QStringLiteral("command");
    else if(element.isComment)
        type = QStringLiteral("comment");
    else if(element.hasName() && element.inner.isEmpty())
        type = QStringLiteral("empty named");
    else if(element.hasName())
        type = QStringLiteral("named");
    else if(element.inner.isEmpty())
        type = QStringLiteral("empty anonymous");
    else
        type = QStringLiteral("anonymous");

    qDebug().noquote() << QStringLiteral("%1[%2] \"%3\" %4:%5")
                          .arg(QStringLiteral("·   ").repeated(indent), type, element.name)
                          .arg(element.line)
                          .arg(element.column);
    indent++;
    for(const auto &child: element.children) {
        dbgDumpTree(child);
    }
    indent--;
}
*/
