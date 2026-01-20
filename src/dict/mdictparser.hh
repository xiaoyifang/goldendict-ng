// https://bitbucket.org/xwang/mdict-analysis
// https://github.com/zhansliu/writemdict/blob/master/fileformat.md
// Octopus MDict Dictionary File (.mdx) and Resource File (.mdd) Analyser
//
// Copyright (C) 2012, 2013 Xiaoqiang Wang <xiaoqiangwang AT gmail DOT com>
// Copyright (C) 2013 Timon Wong <timon86.wang AT gmail DOT com>
// Copyright (C) 2015 Zhe Wang <0x1998 AT gmail DOT com>
//
// This program is a free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// You can get a copy of GNU General Public License along this program
// But you can always get it from http://www.gnu.org/licenses/gpl.txt
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

#pragma once

#include <string>
#include <vector>
#include <map>
#include <utility>

#include <QPointer>
#include <QFile>

namespace Mdict {

using std::string;
using std::vector;
using std::pair;
using std::map;

// A helper class to handle memory map for QFile
class ScopedMemMap
{
  QFile & file;
  uchar * address;

public:
  ScopedMemMap( QFile & file, qint64 offset, qint64 size ):
    file( file ),
    address( nullptr )
  {
    // Check if offset and size are valid
    if ( offset < 0 || size < 0 ) {
      address = nullptr;
      return;
    }

    qint64 fileSize = file.size();
    if ( offset > fileSize ) {
      address = nullptr; // Start offset exceeds file size
      return;
    }

    qint64 maxSafeSize = fileSize - offset;
    if ( size > maxSafeSize ) {
      size = maxSafeSize; // Automatically adjust to the maximum allowed size
    }

    address = file.map( offset, size );
  }

  ~ScopedMemMap()
  {
    if ( address )
      file.unmap( address );
  }

  inline uchar * startAddress()
  {
    return address;
  }
};

// V3 format structures
namespace V3 {

// Key block index entry for v3 format
struct KeyBlockIndex
{
  quint64 entryCount;
  QString firstKey;
  QString lastKey;
  quint64 blockCompressedSize;
  quint64 blockDecompressedSize;
  quint64 blockOffsetInKeyData;
  qint64 firstEntryNo;
};

// Content block index entry for v3 format
struct ContentBlockIndex
{
  quint64 blockCompressedSize;
  quint64 blockDecompressedSize;
  quint64 blockOffsetInContentData;
  quint64 blockOffsetInDecompressed;
};

} // namespace V3

class MdictParser
{
public:

  enum {
    kParserVersion = 0x000000e  // Bumped for v3 support
  };

  struct RecordIndex
  {
    qint64 startPos;
    qint64 endPos;
    qint64 shadowStartPos;
    qint64 shadowEndPos;
    qint64 compressedSize;
    qint64 decompressedSize;

    inline bool operator==( qint64 rhs ) const
    {
      return ( shadowStartPos <= rhs ) && ( rhs < shadowEndPos );
    }

    inline bool operator<( qint64 rhs ) const
    {
      return shadowEndPos <= rhs;
    }

    inline bool operator>( qint64 rhs ) const
    {
      return shadowStartPos > rhs;
    }

    static size_t bsearch( const vector< RecordIndex > & offsets, qint64 val );
  };

  struct RecordInfo
  {
    qint64 compressedBlockPos;
    qint64 recordOffset;

    qint64 decompressedBlockSize;
    qint64 compressedBlockSize;
    qint64 recordSize;
  };

  class RecordHandler
  {
  public:
    virtual void handleRecord( const QString & name, const RecordInfo & recordInfo ) = 0;
    virtual ~RecordHandler() = default;
  };

  using BlockInfoVector = vector< pair< qint64, qint64 > >;
  using HeadWordIndex   = vector< pair< qint64, QString > >;
  using StyleSheets     = map< qint32, pair< QString, QString > >;

  inline const QString & title() const
  {
    return title_;
  }

  inline const QString & description() const
  {
    return description_;
  }

  inline const StyleSheets & styleSheets() const
  {
    return styleSheets_;
  }

  inline quint32 wordCount() const
  {
    return wordCount_;
  }

  inline const QString & encoding() const
  {
    return encoding_;
  }

  inline const QString & filename() const
  {
    return filename_;
  }

  inline bool isRightToLeft() const
  {
    return rtl_;
  }

  // Check if this is v3 (ZDB) format
  inline bool isV3() const
  {
    return version_ >= 3.0;
  }

  // Get format version (1.x, 2.x, or 3.x)
  inline double formatVersion() const
  {
    return version_;
  }

  MdictParser();
  ~MdictParser();

  bool open( const char * filename );
  bool readNextHeadWordIndex( HeadWordIndex & headWordIndex );
  bool readRecordBlock( HeadWordIndex & headWordIndex, RecordHandler & recordHandler );

  // helpers
  static QString toUtf16( const char * fromCode, const char * from, size_t fromSize );
  static inline QString toUtf16( const QString & fromCode, const char * from, size_t fromSize )
  {
    return toUtf16( fromCode.toLatin1().constData(), from, fromSize );
  }
  static bool parseCompressedBlock( qint64 compressedBlockSize,
                                    const char * compressedBlockPtr,
                                    qint64 decompressedBlockSize,
                                    QByteArray & decompressedBlock );
  // V3 storage block decompression (handles encryption + compression)
  static bool parseCompressedBlockV3( const QByteArray & blockData,
                                      const QByteArray & cryptoKey,
                                      quint32 decompressedSize,
                                      QByteArray & decompressedBlock );
  static QString & substituteStylesheet( QString & article, const StyleSheets & styleSheets );
  static inline string substituteStylesheet( const string & article, const StyleSheets & styleSheets )
  {
    QString s = QString::fromUtf8( article.c_str() );
    substituteStylesheet( s, styleSheets );
    return s.toStdString();
  }

protected:
  qint64 readNumber( QDataStream & in );
  static quint32 readU8OrU16( QDataStream & in, bool isU16 );
  static bool checkAdler32( const char * buffer, unsigned int len, quint32 checksum );
  static bool decryptHeadWordIndex( char * buffer, qint64 len );
  bool readHeader( QDataStream & in );
  bool readHeadWordBlockInfos( QDataStream & in );
  bool readRecordBlockInfos();
  BlockInfoVector decodeHeadWordBlockInfo( const QByteArray & headWordBlockInfo );
  HeadWordIndex splitHeadWordBlock( const QByteArray & block );

  // V3 specific methods
  bool openV3();
  bool readHeaderV3( QDataStream & in );
  bool readContentUnitV3( QDataStream & in );
  bool readContentBlockIndexUnitV3( QDataStream & in );
  bool readKeyUnitV3( QDataStream & in );
  bool readKeyBlockIndexUnitV3( QDataStream & in );
  bool readNextHeadWordIndexV3( HeadWordIndex & headWordIndex );
  bool readRecordBlockV3( HeadWordIndex & headWordIndex, RecordHandler & recordHandler );
  QByteArray readStorageBlockV3( QDataStream & in );

protected:
  QString filename_;
  QPointer< QFile > file_;
  StyleSheets styleSheets_;
  BlockInfoVector headWordBlockInfos_;
  BlockInfoVector::iterator headWordBlockInfosIter_;
  vector< RecordIndex > recordBlockInfos_;

  QString encoding_;
  QString title_;
  QString description_;

  double version_;
  qint64 numHeadWordBlocks_;
  qint64 headWordBlockInfoSize_;
  qint64 headWordBlockSize_;
  qint64 headWordBlockInfoPos_;
  qint64 headWordPos_;
  qint64 totalRecordsSize_;
  qint64 recordPos_;

  quint32 wordCount_;
  int numberTypeSize_;
  int encrypted_;
  bool rtl_;

  // V3 specific members
  QByteArray cryptoKey_;
  QString uuid_;
  bool isUtf16Encoding_;

  // V3 unit positions and sizes
  qint64 contentDataOffset_;
  qint64 contentDataSize_;
  quint32 contentBlockCount_;
  qint64 keyDataOffset_;
  qint64 keyDataSize_;

  // V3 block indexes
  vector< V3::KeyBlockIndex > keyBlockIndexesV3_;
  vector< V3::ContentBlockIndex > contentBlockIndexesV3_;
  quint64 totalContentDecompressedSize_;

  // V3 iteration state
  size_t currentKeyBlockIndex_;
  qint64 currentEntryNo_;
};

} // namespace Mdict
