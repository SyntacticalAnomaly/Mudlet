#ifndef MUDLET_TBUFFER_H
#define MUDLET_TBUFFER_H

/***************************************************************************
 *   Copyright (C) 2008-2013 by Heiko Koehn - KoehnHeiko@googlemail.com    *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *   Copyright (C) 2015, 2017-2018, 2020, 2022-2023 by Stephen Lyons       *
 *                                               - slysven@virginmedia.com *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "TTextCodec.h"

#include "pre_guard.h"
#include <QApplication>
#include <QChar>
#include <QColor>
#include <QDebug>
#include <QMap>
#include <QQueue>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QTime>
#include <QVector>
#include "post_guard.h"
#include "TEncodingTable.h"
#include "TLinkStore.h"
#include "TMxpMudlet.h"
#include "TMxpProcessor.h"

#include <deque>
#include <string>

class Host;
class QTextCodec;
class TConsole;

class TChar
{
    friend class TBuffer;

public:
    enum AttributeFlag {
        None = 0x0,
        // Replaces TCHAR_BOLD 2
        Bold = 0x1,
        // Replaces TCHAR_ITALICS 1
        Italic = 0x2,
        // Replaces TCHAR_UNDERLINE 4
        Underline = 0x4,
        // New, TCHAR_OVERLINE had not been previous done, is
        // ANSI CSI SGR Overline (53 on, 55 off)
        Overline = 0x8,
        // Replaces TCHAR_STRIKEOUT 32
        StrikeOut = 0x10,
        // NOT a replacement for TCHAR_INVERSE, that is now covered by the
        // separate isSelected bool but they must be EX-ORed at the point of
        // painting the Character
        Reverse = 0x20,
        // The attributes that are currently user settable and what should be
        // consider in HTML generation:
        TestMask = 0x3f,
        // Has been found in a search operation (currently Main Console only)
        // and has been given a highlight to indicate that:
        Found = 0x40,
        // Replaces TCHAR_ECHO 16
        Echo = 0x100
    };
    Q_DECLARE_FLAGS(AttributeFlags, AttributeFlag)

    // Not a default constructor - the defaulted argument means it could have
    // been used if supplied with no arguments, but the 'explicit' prevents
    // this:
    explicit TChar(TConsole* pC = nullptr);
    // Another non-default constructor:
    TChar(const QColor& fg, const QColor& bg, const TChar::AttributeFlags flags = TChar::None, const int linkIndex = 0);
    // User defined copy-constructor:
    TChar(const TChar&);
    // Under the rule of three, because we have a user defined copy-constructor,
    // we should also have a destructor and an assignment operator but they can,
    // in this case, be default ones:
    TChar& operator=(const TChar&) = default;
    ~TChar() = default;

    bool operator==(const TChar&);
    void setColors(const QColor& newForeGroundColor, const QColor& newBackGroundColor) {
        mFgColor = newForeGroundColor;
        mBgColor = newBackGroundColor;
    }
    // Only considers the following flags: Bold, Italic, Overline, Reverse,
    // Strikeout, Underline, does not consider Echo:
    void setAllDisplayAttributes(const AttributeFlags newDisplayAttributes) { mFlags = (mFlags & ~TestMask) | (newDisplayAttributes & TestMask); }
    void setForeground(const QColor& newColor) { mFgColor = newColor; }
    void setBackground(const QColor& newColor) { mBgColor = newColor; }
    void setTextFormat(const QColor& newFgColor, const QColor& newBgColor, const AttributeFlags newDisplayAttributes) {
        setColors(newFgColor, newBgColor);
        setAllDisplayAttributes(newDisplayAttributes);
    }

    const QColor& foreground() const { return mFgColor; }
    const QColor& background() const { return mBgColor; }
    AttributeFlags allDisplayAttributes() const { return mFlags & TestMask; }
    void select() { mIsSelected = true; }
    void deselect() { mIsSelected = false; }
    bool isSelected() const { return mIsSelected; }
    int linkIndex () const { return mLinkIndex; }
    bool isBold() const { return mFlags & Bold; }
    bool isItalic() const { return mFlags & Italic; }
    bool isUnderlined() const { return mFlags & Underline; }
    bool isOverlined() const { return mFlags & Overline; }
    bool isStruckOut() const { return mFlags & StrikeOut; }
    bool isReversed() const { return mFlags & Reverse; }
    bool isFound() const { return mFlags & Found; }

private:
    QColor mFgColor;
    QColor mBgColor;
    AttributeFlags mFlags = None;
    // Kept as a separate flag because it must often be handled separately
    bool mIsSelected = false;
    int mLinkIndex = 0;

};
Q_DECLARE_OPERATORS_FOR_FLAGS(TChar::AttributeFlags)




class TBuffer
{
    inline static const TEncodingTable &csmEncodingTable = TEncodingTable::csmDefaultInstance;

    inline static const int TCHAR_IN_BYTES = sizeof(TChar);

    // limit on how many characters a single echo can accept for performance reasons
    inline static const int MAX_CHARACTERS_PER_ECHO = 1000000;

public:
    explicit TBuffer(Host* pH, TConsole* pConsole = nullptr);
    QPoint insert(QPoint&, const QString& text, int, int, int, int, int, int, bool bold, bool italics, bool underline, bool strikeout);
    bool insertInLine(QPoint& cursor, const QString& what, const TChar& format);
    void expandLine(int y, int count, TChar&);
    int wrapLine(int startLine, int screenWidth, int indentSize, TChar& format);
    void log(int, int);
    int skipSpacesAtBeginOfLine(const int row, const int column);
    void addLink(bool, const QString& text, QStringList& command, QStringList& hint, TChar format, QVector<int> luaReference = QVector<int>());
    QString bufferToHtml(const bool showTimeStamp = false, const int row = -1, const int endColumn = -1, const int startColumn = 0,  int spacePadding = 0);
    int size() { return static_cast<int>(buffer.size()); }
    bool isEmpty() const { return buffer.size() == 0; }
    QString& line(int n);
    int find(int line, const QString& what, int pos);
    int wrap(int);
    QStringList split(int line, const QString& splitter);
    QStringList split(int line, const QRegularExpression& splitter);
    bool replaceInLine(QPoint& start, QPoint& end, const QString& with, TChar& format);
    bool deleteLine(int);
    bool deleteLines(int from, int to);
    bool applyAttribute(const QPoint& P_begin, const QPoint& P_end, const TChar::AttributeFlags attributes, const bool state);
    bool applyLink(const QPoint& P_begin, const QPoint& P_end, const QStringList& linkFunction, const QStringList& linkHist, QVector<int> luaReference = QVector<int>());
    bool applyFgColor(const QPoint&, const QPoint&, const QColor&);
    bool applyBgColor(const QPoint&, const QPoint&, const QColor&);
    void appendBuffer(const TBuffer& chunk);
    bool moveCursor(QPoint& where);
    int getLastLineNumber();
    QStringList getEndLines(int);
    void clear();
    QPoint getEndPos();
    void translateToPlainText(std::string& s, bool isFromServer = false);
    void append(const QString& chunk, int sub_start, int sub_end, const QColor& fg, const QColor& bg, const TChar::AttributeFlags flags = TChar::None, const int linkID = 0);
    // Only the bits within TChar::TestMask are considered for formatting:
    void append(const QString& chunk, const int sub_start, const int sub_end, const TChar format, const int linkID = 0);
    void appendLine(const QString& chunk, const int sub_start, const int sub_end, const QColor& fg, const QColor& bg, TChar::AttributeFlags flags = TChar::None, const int linkID = 0);
    void setWrapAt(int i) { mWrapAt = i; }
    void setWrapIndent(int i) { mWrapIndent = i; }
    void updateColors();
    TBuffer copy(QPoint&, QPoint&);
    TBuffer cut(QPoint&, QPoint&);
    void paste(QPoint&, const TBuffer&);
    void setBufferSize(int requestedLinesLimit, int batch);
    int getMaxBufferSize();
    static const QList<QByteArray> getEncodingNames();
    void logRemainingOutput();
    // It would have been nice to do this with Qt's signals and slots but that
    // is apparently incompatible with using a default constructor - sigh!
    void encodingChanged(const QByteArray &);
    void clearSearchHighlights();

    static int lengthInGraphemes(const QString& text);


    std::deque<TChar> bufferLine;
    // stores the text attributes (TChars) that make up each line of text in the buffer
    std::deque<std::deque<TChar>> buffer;
    // stores the actual content of lines
    QStringList lineBuffer;
    // stores timestamps associated with lines
    QStringList timeBuffer;
    // stores a boolean whenever the line is a prompt one
    QList<bool> promptBuffer;
    TLinkStore mLinkStore;
    int mLinesLimit = 10000;
    int mBatchDeleteSize = 1000;
    int mWrapAt = 99999999;
    int mWrapIndent = 0;
    int mCursorY = 0;
    bool mEchoingText = false;


private:
    void shrinkBuffer();
    int calculateWrapPosition(int lineNumber, int begin, int end);
    void handleNewLine();
    bool processUtf8Sequence(const std::string&, bool, size_t, size_t&, bool&);
    bool processGBSequence(const std::string&, bool, bool, size_t, size_t&, bool&);
    bool processBig5Sequence(const std::string&, bool, size_t, size_t&, bool&);
    bool processEUC_KRSequence(const std::string&, bool, size_t, size_t&, bool&);
    void decodeSGR(const QString&);
    void decodeSGR38(const QStringList&, bool isColonSeparated = true);
    void decodeSGR48(const QStringList&, bool isColonSeparated = true);
    void decodeOSC(const QString&);
    void resetColors();


    QPointer<TConsole> mpConsole;

    // First stage in decoding SGR/OCS sequences - set true when we see the
    // ASCII ESC character:
    bool mGotESC = false;
    // Second stage in decoding SGR sequences - set true when we see the ASCII
    // ESC character followed by the '[' one:
    bool mGotCSI = false;
    // Second stage in decoding OSC sequences - set true when we see the ASCII
    // ESC character followed by the ']' one:
    bool mGotOSC = false;
    bool mIsDefaultColor = true;


    QColor mBlack;
    QColor mLightBlack;
    QColor mRed;
    QColor mLightRed;
    QColor mLightGreen;
    QColor mGreen;
    QColor mLightBlue;
    QColor mBlue;
    QColor mLightYellow;
    QColor mYellow;
    QColor mLightCyan;
    QColor mCyan;
    QColor mLightMagenta;
    QColor mMagenta;
    QColor mLightWhite;
    QColor mWhite;
    // These three replace three sets of three integers that were used to hold
    // colour components during the parsing of SGR sequences, they were called:
    // fgColor{R|G|B}, fgColorLight{R|G|B} and bgColor{R|G|B} apart from
    // anything else, the first and last sets had the same names as arguments
    // to several of the methods which meant the latter shadowed and masked
    // them off!
    QColor mForeGroundColor;
    QColor mForeGroundColorLight;
    QColor mBackGroundColor;

    QPointer<Host> mpHost;

    bool mBold = false;
    bool mItalics = false;
    bool mOverline = false;
    bool mReverse = false;
    bool mStrikeOut = false;
    bool mUnderline = false;
    bool mItalicBeforeBlink = false;

    QString mMudLine;
    std::deque<TChar> mMudBuffer;
    // Used to hold the unprocessed bytes that could be left at the end of a
    // packet if we detect that there should be more - will be prepended to the
    // next chunk of data - PROVIDED it is flagged as coming from the MUD Server
    // and is not generated locally {because both pass through
    // translateToPlainText()}:
    std::string mIncompleteSequenceBytes;

    // keeps track of the previously logged buffer lines to ensure no log duplication
    // happens when you enter a command
    int lastLoggedFromLine = 0;
    int lastloggedToLine = 0;
    QString lastTextToLog;

    QByteArray mEncoding;
    QTextCodec* mMainIncomingCodec = nullptr;
};

#ifndef QT_NO_DEBUG_STREAM
// Dumper for the TChar::AttributeFlags - so that qDebug gives a detailed broken
// down results when presented with the value rather than just a hex value.
// Note "inline" is REQUIRED:
inline QDebug& operator<<(QDebug& debug, const TChar::AttributeFlags& attributes)
{
    QDebugStateSaver saver(debug);
    QString result = QLatin1String("TChar::AttributeFlags(");
    QStringList presentAttributes;
    if (attributes & TChar::Bold) {
        presentAttributes << QLatin1String("Bold (0x01)");
    }
    if (attributes & TChar::Italic) {
        presentAttributes << QLatin1String("Italic (0x02)");
    }
    if (attributes & TChar::Underline) {
        presentAttributes << QLatin1String("Underline (0x04)");
    }
    if (attributes & TChar::Overline) {
        presentAttributes << QLatin1String("Overline (0x08)");
    }
    if (attributes & TChar::StrikeOut) {
        presentAttributes << QLatin1String("StrikeOut (0x10)");
    }
    if (attributes & TChar::Reverse) {
        presentAttributes << QLatin1String("Reverse (0x20)");
    }
    if (attributes & TChar::Echo) {
        presentAttributes << QLatin1String("Echo (0x100)");
    }
    result.append(presentAttributes.join(QLatin1String(", ")));
    result.append(QLatin1String(")"));
    debug.nospace() << result;
    return debug;
}
#endif // QT_NO_DEBUG_STREAM

#endif // MUDLET_TBUFFER_H
