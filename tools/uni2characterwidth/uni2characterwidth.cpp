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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMap>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>
#include <QTextStream>
#include "template.h"

#include <KIO/Job>



static constexpr unsigned int CODE_POINTS_NUM = 0x110000;
static constexpr unsigned int LAST_CODE_POINT = CODE_POINTS_NUM - 1;

struct UcdEntry {
    struct { uint first; uint last; } cp;
    QStringList fields;
};

class UcdParserBase {
public:
    ~UcdParserBase() {
        _source->close();
    }

    bool hasNext() {
        bool hadNext = _hasNext;
        if(!_nextFetched) {
            _hasNext = fetchNext();
            _nextFetched = true;
        }
        return hadNext;
    }

protected:
    UcdParserBase(QIODevice *source, UcdEntry *entry)
            : _source(source)
            , _nextFetched(false)
            , _hasNext(true)
            , _lineNo(0)
            , _entry(entry)
    {
        Q_ASSERT(_source);
        Q_ASSERT(_entry);
    }

    bool fetchNext() {
        Q_ASSERT(_source->isOpen());
        if(!_source->isOpen())
            return false;

        static const QRegularExpression ENTRY_RE = QRegularExpression(QStringLiteral(
            // Match 1: "cp1" - first CP / "cp2" (optional) - last CP
            R"#((?:^(?<cp1>[[:xdigit:]]+)(?:\.\.(?<cp2>[[:xdigit:]]+))?[ \t]*;)#"
            // Match 1: "field0" - first data field"
            //          "udRangeInd" (UnicodeData.txt only) - if present, the line is either first or last line of a range
            R"#([ \t]*(?<field0>[^#;\n]*?(?:, (?<udRangeInd>First|Last)>)?)[ \t]*(?:;|(?:\#.*)?$))|)#"
            // Match 2..n: "field" - n-th field
            R"#((?:\G(?<=;)[ \t]*(?<field>[^#;\n]*?)[ \t]*(?:;|(?:#.*)?$)))#"),
            QRegularExpression::OptimizeOnFirstUsageOption
        );
        static const QRegularExpression UD_RANGE_IND_RE(QStringLiteral(", (First|Last)"));
        static const QRegularExpression COMMENT_RE(QStringLiteral("^[ \t]*(#.*)?$"));

        QString line;
        bool ok;
        _entry->fields.clear();
        while(!_source->atEnd()) {
            line = QString::fromUtf8(_source->readLine());
            _lineNo++;
            auto mit = ENTRY_RE.globalMatch(line);
            if(!mit.hasNext()) {
                // Do not complain about comments and empty lines
                if(!COMMENT_RE.match(line).hasMatch())
                    qDebug() << QStringLiteral("Line %1: does not match - skipping").arg(_lineNo);
                continue;
            }

            auto match = mit.next();
            _entry->cp.first = match.captured(QStringLiteral("cp1")).toUInt(&ok, 16);
            if(!ok) {
                qDebug() << QStringLiteral("Line %d Invalid cp1 - skipping").arg(_lineNo);
                continue;
            }
            _entry->cp.last = match.captured(QStringLiteral("cp2")).toUInt(&ok, 16);
            if(!ok) {
                _entry->cp.last = _entry->cp.first;
            }
            QString field0 = match.captured(QStringLiteral("field0"));
            if(field0.isNull()) {
                qDebug() << QStringLiteral("Line %d: Missing field0 - skipping").arg(_lineNo);
                continue;
            }
            if(!match.captured(QStringLiteral("udRangeInd")).isNull()) {
                if(match.captured(QStringLiteral("udRangeInd")) == QLatin1String("First")) {
                    // Fetch next valid line, as it pairs with the current one to form a range
                    QRegularExpressionMatch nlMatch;
                    int firstLineNo = _lineNo;
                    while(!_source->atEnd() && !nlMatch.hasMatch()) {
                        line = QString::fromUtf8(_source->readLine());
                        _lineNo++;
                        nlMatch = ENTRY_RE.match(line);
                        if(!nlMatch.hasMatch()) {
                            qDebug() << QStringLiteral("Line %d: does not match - skipping").arg(_lineNo);
                        }
                    }
                    if(nlMatch.hasMatch()) {
                        _entry->cp.last = nlMatch.captured(QStringLiteral("cp1")).toUInt(&ok, 16);
                        if(!ok) {
                            qDebug() << QStringLiteral("Line %1-%2: Missing or invalid second cp1 (\"Last\" entry) - skipping")
                                        .arg(firstLineNo).arg(_lineNo);
                            continue;
                        }
                    }
                }
                field0.remove(UD_RANGE_IND_RE);
            }
            _entry->fields.append(field0);

            while(mit.hasNext()) {
                _entry->fields.append(mit.next().captured(QStringLiteral("field")));
            }

            return !_source->atEnd();
        }
        return false;
    }

    QIODevice *_source;
    bool _nextFetched;
    bool _hasNext;

private:
    int _lineNo;
    UcdEntry *_entry;
};

template <class EntryType>
class UcdParser: public UcdParserBase {
public:
    static_assert(std::is_base_of<UcdEntry, EntryType>::value, "'EntryType' has to be derived from UcdParser::Entry");

    UcdParser(QIODevice *source): UcdParserBase(source, &_typedEntry) {}

    inline const EntryType & next() {
        if(!_nextFetched)
            fetchNext();
        _nextFetched = false;
        return _typedEntry;
    }

private:
    EntryType _typedEntry;
};

class KIODevice: public QIODevice {
public:
    enum Error {
        NoError,
        UnknownError,
        TimeoutError,
        UnknownHostError,
        MalformedUrlError,
        NotFoundError,
    };

    KIODevice(const QUrl &url)
          : _url(url)
          , _job(nullptr)
          , _error(NoError) {}

    ~KIODevice() {
        close();
    }

    bool open() {
        if(_job)
            return false;

        _job = KIO::storedGet(_url);
        QObject::connect(_job, &KIO::StoredTransferJob::result,
                         _job, [&](KJob *) {
            if(_job->isErrorPage())
                _eventLoop.exit(KIO::ERR_DOES_NOT_EXIST);
            else if(_job->error() != KJob::NoError)
                _eventLoop.exit(_job->error());
            else
                _data = _job->data();

            _eventLoop.exit(KJob::NoError);
        });

        _eventLoop.exec();
        switch(_job->error()) {
        case KJob::NoError:
            _error = NoError;
            setErrorString(QStringLiteral(""));
            QIODevice::open(QIODevice::ReadOnly | QIODevice::Unbuffered);
            break;
        case KJob::KilledJobError:      _error = TimeoutError; break;
        case KIO::ERR_UNKNOWN_HOST:     _error = UnknownHostError; break;
        case KIO::ERR_DOES_NOT_EXIST:   _error = NotFoundError; break;
        case KIO::ERR_MALFORMED_URL:    _error = MalformedUrlError; break;
        default:                        _error = UnknownError; break;
        }
        if(_error != NoError) {
            setErrorString(QStringLiteral("KIO: ") + _job->errorString());
            delete _job;
            _job = nullptr;
            _data.clear();
        }
        return _error == NoError;
    }
    bool open(OpenMode mode) override {
        Q_ASSERT(mode == QIODevice::ReadOnly);
        return open();
    }
    void close() override {
        if(_job) {
            delete _job;
            _job = nullptr;
            _error = NoError;
            setErrorString(QStringLiteral(""));
            _data.clear();
            QIODevice::close();
        }
    }

    qint64 size() const override {
        return _data.size();
    }

    int error() const { return _error; }
    void unsetError() { _error = NoError; }

protected:
    qint64 writeData(const char *, qint64) override { return -1; }
    qint64 readData(char *data, qint64 maxSize) override {
        Q_UNUSED(maxSize);
        Q_ASSERT(_job);
        Q_ASSERT(_job->error() == NoError);
        Q_ASSERT(data != nullptr);
        if(maxSize == 0 || pos() >= _data.length()) {
            return 0;
        } else if(pos() < _data.length()) {
            qint64 bytesToCopy = qMin(maxSize, _data.length() - pos());
            memcpy(data, _data.data() + pos(), bytesToCopy);
            return bytesToCopy;
        } else {
            return -1;
        }
    }

private:
    QUrl _url;
    KIO::StoredTransferJob *_job;
    Error _error;
    QEventLoop _eventLoop;
    QByteArray _data;
};



struct CategoryProperty {
    enum Flag: uint32_t {
        Invalid = 0,
        #define CATEGORY_PROPERTY_VALUE(val, sym, intVal) sym = intVal,
        #include "properties.h"
    };
    enum Group: uint32_t {
        #define CATEGORY_PROPERTY_GROUP(val, sym, intVal) sym = intVal,
        #include "properties.h"
    };

    CategoryProperty(uint32_t value = Unassigned): _value(value) {}
    CategoryProperty(const QString &string): _value(fromString(string)) {}
    operator uint32_t &() { return _value; }
    operator const uint32_t &() const { return _value; }
    bool isValid() const { return _value != Invalid; }

private:
    static uint32_t fromString(const QString &string) {
        static const QMap<QString, uint32_t> map = {
            #define CATEGORY_PROPERTY_VALUE(val, sym, intVal) { QStringLiteral(#val), sym },
            #include "properties.h"
        };
        return map.contains(string) ? map[string] : uint8_t(Invalid);
    }
    uint32_t _value;
};

struct EastAsianWidthProperty {
    enum Value: uint8_t {
        Invalid = 0x80,
        #define EAST_ASIAN_WIDTH_PROPERTY_VALUE(val, sym, intVal) sym = intVal,
        #include "properties.h"
    };

    EastAsianWidthProperty(uint8_t value = Neutral): _value(value) {}
    EastAsianWidthProperty(const QString &string): _value(fromString(string)) {}
    operator uint8_t &() { return _value; }
    operator const uint8_t &() const { return _value; }
    bool isValid() const { return _value != Invalid; }

private:
    static uint8_t fromString(const QString &string) {
        static const QMap<QString, Value> map = {
            #define EAST_ASIAN_WIDTH_PROPERTY_VALUE(val, sym, intVal) { QStringLiteral(#val), Value::sym },
            #include "properties.h"
        };
        return map.contains(string) ? map[string] : Invalid;
    }
    uint8_t _value;
};

struct EmojiProperty {
    enum Flag: uint8_t {
        Invalid = 0x80,
        #define EMOJI_PROPERTY_VALUE(val, sym, intVal) sym = intVal,
        #include "properties.h"
    };

    EmojiProperty(uint8_t value = None): _value(value) {}
    EmojiProperty(const QString &string): _value(fromString(string)) {}
    operator uint8_t &() { return _value; }
    operator const uint8_t &() const { return _value; }
    bool isValid() const { return !(_value & Invalid); }

private:
    static uint8_t fromString(const QString &string) {
        static const QMap<QString, uint8_t> map = {
            #define EMOJI_PROPERTY_VALUE(val, sym, intVal) { QStringLiteral(#val), sym },
            #include "properties.h"
        };
        return map.contains(string) ? map[string] : uint8_t(Invalid);
    }
    uint8_t _value;
};



struct CharacterWidth {
    enum Width: int8_t {
        Invalid       = SCHAR_MIN,
        _VALID_START  = -3,
        Ambiguous     = -2,
        NonPrintable  = -1,
        // 0
        // 1
        Unassigned    =  1,
        // 2
        _VALID_END    =  3,
    };

    CharacterWidth(const CharacterWidth &other): _width(other._width) {}
    CharacterWidth(int8_t width = Invalid): _width(width) {}
    CharacterWidth & operator =(const CharacterWidth &other) { _width = other._width; return *this; }
    int operator =(const int8_t width) { _width = width; return _width; }
    int width() const { return _width; }
    operator int() const { return width(); }

    const QString toString() const {
        switch(_width) {
        case Ambiguous:     return QStringLiteral("Ambiguous");
        case NonPrintable:  return QStringLiteral("NonPrintable");
        case 0:             return QStringLiteral("0");
        case 1:             return QStringLiteral("1");
        case 2:             return QStringLiteral("2");
        default:
        case Invalid:       return QStringLiteral("Invalid");
        }
    }

    bool isValid() const { return (_width > _VALID_START && _width < _VALID_END); };

private:
    int8_t _width;
};



struct CharacterProperties {
    CategoryProperty category;
    EastAsianWidthProperty eastAsianWidth;
    EmojiProperty emoji;
    CharacterWidth customWidth;
    // For debug purposes in "details" output generator
    uint8_t widthFromPropsRule;
};



struct UnicodeDataEntry: public UcdEntry {
    enum FieldId {
        NameId      = 0,
        CategoryId  = 1,
    };
    CategoryProperty category() const { return CategoryProperty(this->fields.value(CategoryId)); }
};

struct EastAsianWidthEntry: public UcdEntry {
    enum FieldId {
        WidthId = 0,
    };
    EastAsianWidthProperty eastAsianWidth() const { return EastAsianWidthProperty(this->fields.value(WidthId)); }
};

struct EmojiDataEntry: public UcdEntry {
    enum FieldId {
        EmojiId = 0,
    };
    EmojiProperty emoji() const { return EmojiProperty(this->fields.value(EmojiId)); }
};

struct GenericWidthEntry: public UcdEntry {
    enum FieldId {
        WidthId = 0,
    };
    CharacterWidth width() const {
        bool ok;
        CharacterWidth w = this->fields.value(WidthId).toInt(&ok, 10);
        return (ok && w.isValid()) ? w : CharacterWidth::Invalid;
    }
};

struct WidthsRange {
    struct { uint first; uint last; } cp;
    CharacterWidth width;
};

QVector<WidthsRange> rangesFromWidths(const QVector<CharacterWidth> &widths, QPair<uint, uint> ucsRange = {0, CODE_POINTS_NUM}) {
    QVector<WidthsRange> ranges;

    if(ucsRange.second >= CODE_POINTS_NUM)
        ucsRange.second = widths.size() - 1;

    uint first = ucsRange.first;
    for(uint cp = first + 1; cp <= uint(ucsRange.second); ++cp) {
        if(widths[first] != widths[cp]) {
            ranges.append({{first, cp-1}, widths[cp-1]});
            first = cp;
        }
    }
    ranges.append({{first, uint(ucsRange.second)}, widths[ucsRange.second]});

    return ranges;
}

// Real ranges look like this (each continuous letter sequence is a range):
//
//     D    D D D   D D        D D                   8 ranges
//         C C   C C C C     CC C CC                 9 ranges
//  BBB BBB       B     B BBB       BBBBBB           6 ranges
// A           A         A                A          4 ranges
//                                               ∑: 27 ranges
//
// To reduce total ranges count, the holes in groups can be filled with ranges
// from groups above them:
//
//     D    D D D   D D        D D                   8 ranges
//         CCC   C CCCCC     CCCCCCC                 4 ranges
//  BBBBBBB       BBBBBBB BBBBBBBBBBBBBBBB           3 ranges
// AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA          1 ranges
//                                               ∑: 16 ranges
//
// First range is always without change. Last range (A) can be dropped
// (it always contains everything). Search should be done in order: D, C, B (A).
// For simplicity the function returns all ranges, including first and last.
QMap<CharacterWidth, QVector<QPair<uint, uint>>> mergedRangesFromWidths(const QVector<CharacterWidth> &widths, const QVector<CharacterWidth> widthsSortOrder,
                                                                        QPair<uint, uint> ucsRange = {0, CODE_POINTS_NUM}) {
    if(ucsRange.second >= CODE_POINTS_NUM)
        ucsRange.second = widths.size() - 1;
    QVector<WidthsRange> ranges = rangesFromWidths(widths, ucsRange);
    QMap<CharacterWidth, QVector<QPair<uint, uint>>> mergedRanges;

    int cmwi;           // Currently Merged Width Index
    int sri = -1;       // Start Range Index (for current width)
    int cri;            // Current Range Index

    // First width ranges are without change. Last one has one range spanning everything, so we can skip this
    for(cmwi = 1; cmwi < widthsSortOrder.size() - 1; ++cmwi) {
        const CharacterWidth &cmw = widthsSortOrder[cmwi];  // Currently Merged Width
        for(cri = 0; cri < ranges.size(); ++cri) {
            WidthsRange &cr = ranges[cri];    // Current Range
            if(cr.width == cmw) {
                // Range is suitable for merge
                if(sri < 0) {
                    // First one, just remember it
                    sri = cri;
                } else {
                    // Merge
                    ranges[sri].cp.last = cr.cp.last;
                    cr.width = CharacterWidth::Invalid;
                }
            } else {
                // Current range has another width - can we continue merging?
                if(sri >= 0) {
                    const int crwi = widthsSortOrder.indexOf(cr.width); // Current Range Width Index
                    if(!(crwi < cmwi && crwi >= 0)) {
                        // current range is not above currently merged width - stop merging
                        sri = -1;
                    }
                }
            }
        }
    }

    for(const auto &range: qAsConst(ranges)) {
        if(range.width.isValid() && range.width != widthsSortOrder.last())
            mergedRanges[range.width].append({range.cp.first, range.cp.last});
    }
    mergedRanges[widthsSortOrder.last()].append({ucsRange.first, ucsRange.second});

    return mergedRanges;
}

namespace generators {

using GeneratorFunc = bool (*)(QTextStream &, const QVector<CharacterProperties> &,
                               const QVector<CharacterWidth> &, const QMap<QString, QString> &);

bool code(QTextStream &out, const QVector<CharacterProperties> &props, const QVector<CharacterWidth> &widths,
          const QMap<QString, QString> &args) {
    static constexpr int DIRECT_LUT_SIZE = 256;

    Q_UNUSED(props);
    QTextStream eout(stderr, QIODevice::WriteOnly);

    if(args.value(QStringLiteral("param")).isEmpty()) {
        eout << QStringLiteral("Template file not specified.") << endl << endl;
        return false;
    }
    QFile templateFile(args.value(QStringLiteral("param")));
    if(!templateFile.open(QIODevice::ReadOnly)) {
        eout << QStringLiteral("Could not open file ") << templateFile.fileName() << ": " << templateFile.errorString();
        exit(1);
    }

    const QString templateText = QString::fromUtf8(templateFile.readAll());
    templateFile.close();

    Var::Map data = {
        {QStringLiteral("gen-file-warning"),     QStringLiteral("THIS IS A GENERATED FILE. DO NOT EDIT.")},
        {QStringLiteral("cmdline"),              args.value(QStringLiteral("cmdline"))},
        {QStringLiteral("direct-lut"),           Var::Vector(DIRECT_LUT_SIZE)},
        {QStringLiteral("direct-lut-size"),      DIRECT_LUT_SIZE},
        {QStringLiteral("ranges-luts"),          Var::Vector()},
        {QStringLiteral("ranges-lut-list"),      Var::Vector()},
        {QStringLiteral("ranges-lut-list-size"), 0},
    };

    // Fill direct-lut with widths of 0x00-0xFF
    for(unsigned i = 0; i < DIRECT_LUT_SIZE; ++i) {
        Q_ASSERT(widths[i].isValid());
        data[QStringLiteral("direct-lut")].vec[i] = int(widths[i]);
    }

    static const QVector<CharacterWidth> widthsSortOrder = {CharacterWidth::NonPrintable, 2, CharacterWidth::Ambiguous, 0, 1};
    const QMap<CharacterWidth, QVector<QPair<uint, uint>>> mergedRanges
            = mergedRangesFromWidths(widths, widthsSortOrder, {DIRECT_LUT_SIZE, CODE_POINTS_NUM});

    // Find last non-empty ranges lut
    int lastWidthId = 0;
    for(int wi = widthsSortOrder.size() - 1; wi > 0; --wi) {
        if(mergedRanges.contains(widthsSortOrder[wi])) {
            lastWidthId = wi;
            break;
        }
    }
    // Create ranges-luts for all widths except last non-empty one and empty ones
    for(int wi = 0; lastWidthId != 0 && wi < lastWidthId; ++wi) {
        const CharacterWidth width = widthsSortOrder[wi];
        auto currentMergedRangesIt = mergedRanges.find(width);
        if(currentMergedRangesIt == mergedRanges.end() || currentMergedRangesIt.value().isEmpty())
            continue;
        const int size = mergedRanges[width].size();
        const QString name = QString(QStringLiteral("LUT_%1")).arg(width.toString().toUpper());
        data[QStringLiteral("ranges-luts")].vec.append(Var::Map {
            {QStringLiteral("name"),    name},
            {QStringLiteral("ranges"),  Var::Vector()},
            {QStringLiteral("size"),    size},
        });
        data[QStringLiteral("ranges-lut-list")].vec.append(Var::Map {
            {QStringLiteral("width"),   int(width)},
            {QStringLiteral("name"),    name},
            {QStringLiteral("size"),    size},
        });
        auto &currentLut = data[QStringLiteral("ranges-luts")].vec.last()[QStringLiteral("ranges")].vec;
        for(const auto &range: *currentMergedRangesIt) {
            Q_ASSERT(range.first <= LAST_CODE_POINT);
            Q_ASSERT(range.second <= LAST_CODE_POINT);
            currentLut.append(Var(Var::Map {{QStringLiteral("first"), range.first}, {QStringLiteral("last"), range.second}}));
        }
    }
    data[QStringLiteral("ranges-lut-list")].vec.append(Var::Map {
        {QStringLiteral("width"),   widthsSortOrder[lastWidthId].width()},
        {QStringLiteral("name"),    QStringLiteral("nullptr")},
        {QStringLiteral("size"),    1},
    });
    data[QStringLiteral("ranges-lut-list-size")] = mergedRanges.size();

    Template t(templateText);
    t.parse();
    out << t.generate(data);

    return true;
}

bool list(QTextStream &out, const QVector<CharacterProperties> &props, const QVector<CharacterWidth> &widths,
          const QMap<QString, QString> &args) {
    Q_UNUSED(props);

    out << QStringLiteral("# generated with: ") << args.value(QStringLiteral("cmdline")) << QStringLiteral("\n");
    for(uint cp = 1; cp <= LAST_CODE_POINT; ++cp) {
        out << QString::asprintf("%06X ; %2d\n", cp, int(widths[cp]));
    }

    return true;
}

bool ranges(QTextStream &out, const QVector<CharacterProperties> &props, const QVector<CharacterWidth> &widths,
            const QMap<QString, QString> &args) {
    Q_UNUSED(props);
    const auto ranges = rangesFromWidths(widths);

    out << QStringLiteral("# generated with: ") << args.value(QStringLiteral("cmdline")) << QStringLiteral("\n");
    for(const WidthsRange &range: ranges) {
        if(range.cp.first != range.cp.last)
            out << QString::asprintf("%06X..%06X ; %2d\n", range.cp.first, range.cp.last, int(range.width));
        else
            out << QString::asprintf("%06X         ; %2d\n", range.cp.first, int(range.width));
    }

    return true;
}

bool compactRanges(QTextStream &out, const QVector<CharacterProperties> &props, const QVector<CharacterWidth> &widths,
                   const QMap<QString, QString> &args) {
    Q_UNUSED(props);
    static const QVector<CharacterWidth> widthsSortOrder = {CharacterWidth::NonPrintable, 2, CharacterWidth::Ambiguous, 0, 1};
    const auto mergedRanges = mergedRangesFromWidths(widths, widthsSortOrder);

    out << QStringLiteral("# generated with: ") << args.value(QStringLiteral("cmdline")) << QStringLiteral("\n");
    for(const int width: qAsConst(widthsSortOrder)) {
        const auto currentMergedRangesIt = mergedRanges.find(width);
        if(currentMergedRangesIt == mergedRanges.end() || currentMergedRangesIt.value().isEmpty())
            continue;
        for(const auto &range: currentMergedRangesIt.value()) {
            if(range.first != range.second)
                out << QString::asprintf("%06X..%06X ; %2d\n", range.first, range.second, int(width));
            else
                out << QString::asprintf("%06X         ; %2d\n", range.first, int(width));
        }
    }

    return true;
}

bool details(QTextStream &out, const QVector<CharacterProperties> &props, const QVector<CharacterWidth> &widths,
             const QMap<QString, QString> &args) {
    out.setFieldAlignment(QTextStream::AlignLeft);

    out << QStringLiteral("# generated with: ") << args.value(QStringLiteral("cmdline")) << QStringLiteral("\n");
    out << QString::asprintf("#%-5s ; %-4s ; %-8s ; %-3s ; %-2s ; %-4s ; %-4s\n",
                             "CP", "Wdth", "Cat", "EAW", "EM", "CstW", "Rule");
    QMap<CharacterWidth, uint> widthStats;
    for(uint cp = 0; cp <= LAST_CODE_POINT; ++cp) {
        out << QString::asprintf("%06X ; %4d ; %08X ;  %02X ; %02X ; %4d ; %d\n", cp,
                                 int8_t(widths[cp]), uint32_t(props[cp].category), uint8_t(props[cp].eastAsianWidth),
                                 uint8_t(props[cp].emoji), int8_t(props[cp].customWidth), props[cp].widthFromPropsRule);
        if(!widthStats.contains(widths[cp]))
            widthStats.insert(widths[cp], 0);
        widthStats[widths[cp]]++;
    }
    QMap<CharacterWidth, uint> rangesStats;
    const auto ranges = rangesFromWidths(widths);
    for(const auto &range: ranges) {
        if(!rangesStats.contains(range.width))
            rangesStats.insert(range.width, 0);
        rangesStats[range.width]++;
    }
    out << QStringLiteral("# STATS") << endl;
    out << QStringLiteral("#") << endl;
    out << QStringLiteral("# Characters count for each width:") << endl;
    for(auto wi = widthStats.constBegin(); wi != widthStats.constEnd(); ++wi) {
        out << QString::asprintf("# %2d: %7d\n", int(wi.key()), widthStats[wi.key()]);
    }
    out << QStringLiteral("#") << endl;
    out << QStringLiteral("# Ranges count for each width:") << endl;
    int howmany = 0;
    for(auto wi = rangesStats.constBegin(); wi != rangesStats.constEnd(); ++wi) {
        if(howmany >= 20) break;
        howmany++;
        out << QString::asprintf("# %2d: %7d\n", int(wi.key()), rangesStats[wi.key()]);
    }

    return true;
}
} // namespace generators



template <class EntryType>
static void processInputFiles(QVector<CharacterProperties> &props, const QStringList &files, const QString &fileTypeName,
                              void (*cb)(CharacterProperties &prop, const EntryType &entry)) {
    static const QRegularExpression PROTOCOL_RE(QStringLiteral(R"#(^[a-z]+://)#"), QRegularExpression::OptimizeOnFirstUsageOption);
    for(const QString &fileName: files) {
        qInfo().noquote() << QStringLiteral("Parsing as %1: %2").arg(fileTypeName).arg(fileName);
        QSharedPointer<QIODevice> source = nullptr;
        if(PROTOCOL_RE.match(fileName).hasMatch()) {
            source.reset(new KIODevice(QUrl(fileName)));
        } else {
            source.reset(new QFile(fileName));
        }

        if(!source->open(QIODevice::ReadOnly)) {
            qCritical() << QStringLiteral("Could not open %1: %2").arg(fileName).arg(source->errorString());
            exit(1);
        }
        UcdParser<EntryType> p(source.data());
        while(p.hasNext()) {
            const auto &e = p.next();
            for(uint cp = e.cp.first; cp <= e.cp.last; ++cp) {
                cb(props[cp], e);
            }
        }
    }
}

static const QString escapeCmdline(const QStringList &args) {
    static QString cmdline = QString();
    if(!cmdline.isEmpty())
        return cmdline;

    QTextStream stream(&cmdline, QIODevice::WriteOnly);

    // basename for command name
    stream << QFileInfo(args[0]).baseName();
    for(auto it = args.begin() + 1;  it != args.end(); ++it) {
        if(!it->startsWith(QLatin1Char('-')))
            stream << QStringLiteral(" \"") << QString(*it).replace(QRegularExpression(QStringLiteral(R"(["`$\\])")), QStringLiteral(R"(\\\1)")) << '"';
        else
            stream << ' ' << *it;
    }
    stream.flush();
    return cmdline;
}

enum ConvertOptions {
    AmbiguousWidthOpt       = 0,
    EmojiOpt                = 1,
};

// Character width assignment
//
// Rules (from highest to lowest priority):
//
// * Local overlay
// * (not implemented) Character unique properties described in The Unicode Standard, Version 10.0
// * Unicode category Cc, Cs: -1
// * Emoji: 2
// * Unicode category Mn, Me, Cf: 0
// * East Asian Width W, F: 2
// * East Asian Width H, N, Na: 1
// * East Asian Width A: (varies)
// * Unassigned/Undefined/Private Use: 1
//
// The list is loosely based on character width implementations in Vim 8.1
// and glibc 2.27. There are a few cases which could look better
// (decomposed Hangul, emoji with modifiers, etc) with different widths,
// but interactive terminal programs (at least vim, zsh, everything based
// on glibc's wcwidth) would see their width as it is implemented now.
static inline CharacterWidth widthFromProps(const CharacterProperties &props, uint cp, const QMap<ConvertOptions, int> &convertOpts) {
    CharacterWidth cw;
    auto &widthFromPropsRule = const_cast<uint8_t &>(props.widthFromPropsRule);
    if(props.customWidth.isValid()) {
        widthFromPropsRule = 1;
        cw = props.customWidth;

    } else if((CategoryProperty::Control | CategoryProperty::Surrogate) & props.category) {
        widthFromPropsRule = 2;
        cw = CharacterWidth::NonPrintable;

    } else if(convertOpts[EmojiOpt] & props.emoji && !(EmojiProperty::EmojiComponent & props.emoji)) {
        widthFromPropsRule = 3;
        cw = 2;

    } else if((CategoryProperty::NonspacingMark | CategoryProperty::EnclosingMark | CategoryProperty::Format) & props.category) {
        widthFromPropsRule = 4;
        cw = 0;

    } else if((EastAsianWidthProperty::Wide | EastAsianWidthProperty::Fullwidth) & props.eastAsianWidth) {
        widthFromPropsRule = 5;
        cw = 2;

    } else if((EastAsianWidthProperty::Halfwidth | EastAsianWidthProperty::Neutral | EastAsianWidthProperty::Narrow) & props.eastAsianWidth) {
        widthFromPropsRule = 6;
        cw = 1;

    } else if((CategoryProperty::Unassigned | CategoryProperty::PrivateUse) & props.category) {
        widthFromPropsRule = 7;
        cw = CharacterWidth::Unassigned;

    } else if((EastAsianWidthProperty::Ambiguous) & props.eastAsianWidth) {
        widthFromPropsRule = 8;
        cw = convertOpts[AmbiguousWidthOpt];

    } else if(!props.category.isValid()) {
        widthFromPropsRule = 9;
        qWarning() << QStringLiteral("Code point U+%1 has invalid category - this should not happen. Assuming \"unassigned\"")
                      .arg(cp, 4, 16, QLatin1Char('0'));
        cw = CharacterWidth::Unassigned;

    } else {
        widthFromPropsRule = 10;
        qWarning() << QStringLiteral("Code point U+%1 not classified - this should not happen. Assuming non-printable character")
                      .arg(cp, 4, 16, QLatin1Char('0'));
        cw = CharacterWidth::NonPrintable;
    }

    return cw;
}

int main(int argc, char *argv[]) {
    static const QMap<QString, generators::GeneratorFunc> GENERATOR_FUNCS_MAP = {
        {QStringLiteral("code"),            generators::code},
        {QStringLiteral("compact-ranges"),  generators::compactRanges},
        {QStringLiteral("ranges"),          generators::ranges},
        {QStringLiteral("list"),            generators::list},
        {QStringLiteral("details"),         generators::details},
        {QStringLiteral("dummy"),           [](QTextStream &, const QVector<CharacterProperties> &, const QVector<CharacterWidth> &,
                               const QMap<QString, QString> &)->bool {return true;}},
    };
    qSetMessagePattern(QStringLiteral("%{message}"));

    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("\nUCD files to characters widths converter.\n")
    );
    parser.addHelpOption();
    parser.addOptions({
        {{QStringLiteral("U"), QStringLiteral("unicode-data")},
            QStringLiteral("Path or URL to UnicodeData.txt."),
            QStringLiteral("URL|file")},
        {{QStringLiteral("A"), QStringLiteral("east-asian-width")},
            QStringLiteral("Path or URL to EastAsianWidth.txt."),
            QStringLiteral("URL|file")},
        {{QStringLiteral("E"), QStringLiteral("emoji-data")},
            QStringLiteral("Path or URL to emoji-data.txt."),
            QStringLiteral("URL|file")},
        {{QStringLiteral("W"), QStringLiteral("generic-width")},
            QStringLiteral("Path or URL to generic file with width data. Accepts output from compact-ranges, ranges, list and details generator."),
            QStringLiteral("URL|file")},

        {QStringLiteral("ambiguous-width"),
            QStringLiteral("Ambiguous characters width."),
            QStringLiteral("separate|1|2"), QString(QStringLiteral("%1")).arg(CharacterWidth::Ambiguous)},
        {QStringLiteral("emoji"),
            QStringLiteral("Which emoji emoji subset is treated as emoji."),
            QStringLiteral("all|presentation"), QStringLiteral("presentation")},

        {{QStringLiteral("g"), QStringLiteral("generator")},
            QStringLiteral("Output generator (use \"-\" to list available generators). The code generator requires path to a template file."),
            QStringLiteral("generator[:template]"), QStringLiteral("details")},
    });
    parser.addPositionalArgument(QStringLiteral("output"), QStringLiteral("Output file (leave empty for stdout)."));
    parser.process(app);

    const QStringList unicodeDataFiles      = parser.values(QStringLiteral("unicode-data"));
    const QStringList eastAsianWidthFiles   = parser.values(QStringLiteral("east-asian-width"));
    const QStringList emojiDataFiles        = parser.values(QStringLiteral("emoji-data"));
    const QStringList genericWidthFiles     = parser.values(QStringLiteral("generic-width"));
    const QString     ambiguousWidthStr     = parser.value(QStringLiteral("ambiguous-width"));
    const QString     emojiStr              = parser.value(QStringLiteral("emoji"));
    const QString     generator             = parser.value(QStringLiteral("generator"));
    const QString     outputFileName        = parser.positionalArguments().value(0);

    QTextStream eout(stderr, QIODevice::WriteOnly);
    if(unicodeDataFiles.isEmpty() && eastAsianWidthFiles.isEmpty() && emojiDataFiles.isEmpty() && genericWidthFiles.isEmpty()) {
        eout << QStringLiteral("Input files not specified.") << endl << endl;
        parser.showHelp(1);
    }

    static QMap<ConvertOptions, int> convertOpts = {
        {AmbiguousWidthOpt, CharacterWidth::Ambiguous},
        {EmojiOpt,          EmojiProperty::EmojiPresentation},
    };

    if(emojiStr == QLatin1String("presentation"))
        convertOpts[EmojiOpt] = EmojiProperty::EmojiPresentation;
    else if(emojiStr == QLatin1String("all"))
        convertOpts[EmojiOpt] = EmojiProperty::Emoji;
    else {
        convertOpts[EmojiOpt] = EmojiProperty::EmojiPresentation;
        qWarning() << QStringLiteral("invalid emoji option value: %1. Assuming \"presentation\".").arg(emojiStr);
    }

    if(ambiguousWidthStr == QLatin1String("separate"))
        convertOpts[AmbiguousWidthOpt] = CharacterWidth::Ambiguous;
    else if(ambiguousWidthStr == QLatin1Char('1'))
        convertOpts[AmbiguousWidthOpt] = 1;
    else if(ambiguousWidthStr == QLatin1Char('2'))
        convertOpts[AmbiguousWidthOpt] = 2;
    else {
        convertOpts[AmbiguousWidthOpt] = CharacterWidth::Ambiguous;
        qWarning() << QStringLiteral("Invalid ambiguous-width option value: %1. Assuming \"separate\".").arg(emojiStr);
    }

    const int sepPos            = generator.indexOf(QLatin1Char(':'));
    const auto generatorName    = generator.left(sepPos);
    const auto generatorParam   = sepPos >= 0 ? generator.mid(sepPos + 1) : QString();

    if(!GENERATOR_FUNCS_MAP.contains(generatorName)) {
        int status = 0;
        if(generatorName != QLatin1String("-")) {
            status = 1;
            eout << QStringLiteral("Invalid output generator. Available generators:") << endl;
        }

        for(auto it = GENERATOR_FUNCS_MAP.constBegin(); it != GENERATOR_FUNCS_MAP.constEnd(); ++it) {
            eout << it.key() << endl;
        }
        exit(status);
    }
    auto generatorFunc = GENERATOR_FUNCS_MAP[generatorName];

    QFile outFile;
    if(!outputFileName.isEmpty()) {
        outFile.setFileName(outputFileName);
        if(!outFile.open(QIODevice::WriteOnly)) {
            eout << QStringLiteral("Could not open file ") << outputFileName << QStringLiteral(": ") << outFile.errorString() << endl;
            exit(1);
        }
    } else {
        outFile.open(stdout, QIODevice::WriteOnly);
    }
    QTextStream out(&outFile);

    QVector<CharacterProperties> props(CODE_POINTS_NUM);

    processInputFiles<UnicodeDataEntry>(
                props, unicodeDataFiles, QStringLiteral("UnicodeData.txt"),
                [](CharacterProperties &prop, const UnicodeDataEntry &entry) { prop.category = entry.category(); });

    processInputFiles<EastAsianWidthEntry>(
                props, eastAsianWidthFiles, QStringLiteral("EastAsianWidth.txt"),
                [](CharacterProperties &prop, const EastAsianWidthEntry &entry) { prop.eastAsianWidth = entry.eastAsianWidth(); });

    processInputFiles<EmojiDataEntry>(
                props, emojiDataFiles, QStringLiteral("emoji-data.txt"),
                [](CharacterProperties &prop, const EmojiDataEntry &entry) { prop.emoji |= entry.emoji(); });

    processInputFiles<GenericWidthEntry>(
                props, genericWidthFiles, QStringLiteral("generic width data"),
                [](CharacterProperties &prop, const GenericWidthEntry &entry) { prop.customWidth = entry.width(); });

    qInfo() << "Generating character width data";
    QVector<CharacterWidth> widths(CODE_POINTS_NUM);
    widths[0] = 0; // NULL character always has width 0
    for(uint cp = 1; cp <= LAST_CODE_POINT; ++cp) {
        widths[cp] = widthFromProps(props[cp], cp, convertOpts);
    }

    const QMap<QString, QString> generatorArgs = {
        {QStringLiteral("cmdline"), escapeCmdline(app.arguments())},
        {QStringLiteral("param"), generatorParam},
        {QStringLiteral("output"), outputFileName.isEmpty() ? QStringLiteral("<stdout>") : outputFileName},
    };

    qInfo() << "Generating output";
    if(!generatorFunc(out, props, widths, generatorArgs)) {
        parser.showHelp(1);
    }

    return 0;
}
