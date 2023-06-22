#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <vector>
#include "common/wstring.hh"
#include "common/ex.hh"

namespace Request {

using wstring = gd::wstring;

DEF_EX( Ex, "Dictionary Request Exception", std::exception )
DEF_EX( exRequestUnfinished, "The request hasn't yet finished", Ex )
DEF_EX( exIndexOutOfRange, "The supplied index is out of range", Ex )
DEF_EX( exSliceOutOfRange, "The requested data slice is out of range", Ex )

/// When you request a search to be performed in a dictionary, you get
/// this structure in return. It accumulates search results over time.
/// The finished() signal is emitted when the search has finished and there's
/// no more matches to be expected. Note that before connecting to it, check
/// the result of isFinished() -- if it's 'true', the search was instantaneous.
/// Destroy the object when you are not interested in results anymore.
///
/// Creating, destroying and calling member functions of the requests is done
/// in the GUI thread, however. Therefore, it is important to make sure those
/// operations are fast (this is most important for word searches, where
/// new requests are created and old ones deleted immediately upon a user
/// changing query).
class Base: public QObject
{
  Q_OBJECT

public:
  explicit Base( QObject * parent = nullptr ):
    QObject( parent )
  {
  }
  /// Returns whether the request has been processed in full and finished.
  /// This means that the data accumulated is final and won't change anymore.
  bool isFinished();

  /// Either returns an empty string in case there was no error processing
  /// the request, or otherwise a human-readable string describing the problem.
  /// Note that an empty result, such as a lack of word or of an article isn't
  /// an error -- but any kind of failure to connect to, or read the dictionary
  /// is.
  QString getErrorString();

  /// Cancels the ongoing request. This may make Request destruct faster some
  /// time in the future, Use this in preparation to destruct many Requests,
  /// so that they'd be cancelling in parallel. When the request was fully
  /// cancelled, it must emit the finished() signal, either as a result of an
  /// actual finish which has happened just before the cancellation, or solely as
  /// a result of a request being cancelled (in the latter case, the actual
  /// request result may be empty or incomplete). That is, finish() must be
  /// called by a derivative at least once if cancel() was called, either after
  /// or before it was called.
  virtual void cancel() = 0;

  virtual ~Base() {}

signals:

  /// This signal is emitted when more data becomes available. Local
  /// dictionaries typically don't call this, since it is preferred that all
  /// data would be available from them at once, but network dictionaries
  /// might call that.
  void updated();

  /// This signal is emitted when the request has been processed in full and
  /// finished. That is, it's emitted when isFinished() turns true.
  void finished();

  void matchCount( int );

protected:

  /// Called by derivatives to signal update().
  void update();

  /// Called by derivatives to set isFinished() flag and signal finished().
  void finish();

  /// Sets the error string to be returned by getErrorString().
  void setErrorString( QString const & );

private:

  QAtomicInt isFinishedFlag;

  QMutex errorStringMutex;
  QString errorString;
};

/// This structure represents the word found. In addition to holding the
/// word itself, it also holds its weight. It is 0 by default. Negative
/// values should be used to store distance from Levenstein-like matching
/// algorithms. Positive values are used by morphology matches.
struct WordMatch
{
  wstring word;
  int weight;

  WordMatch():
    weight( 0 )
  {
  }
  WordMatch( wstring const & word_ ):
    word( word_ ),
    weight( 0 )
  {
  }
  WordMatch( wstring const & word_, int weight_ ):
    word( word_ ),
    weight( weight_ )
  {
  }
};

/// This request type corresponds to all types of word searching operations.
class WordSearch: public Base
{
  Q_OBJECT

public:

  WordSearch():
    uncertain( false )
  {
  }

  /// Returns the number of matches found. The value can grow over time
  /// unless isFinished() is true.
  size_t matchesCount();

  /// Returns the match with the given zero-based index, which should be less
  /// than matchesCount().
  WordMatch operator[]( size_t index );

  /// Returns all the matches found. Since no further locking can or would be
  /// done, this can only be called after the request has finished.
  std::vector< WordMatch > & getAllMatches();

  /// Returns true if the match was uncertain -- that is, there may be more
  /// results in the dictionary itself, the dictionary index isn't good enough
  /// to tell that.
  bool isUncertain() const
  {
    return uncertain;
  }

  /// Add match if one is not presented in matches list
  void addMatch( WordMatch const & match );

protected:

  // Subclasses should be filling up the 'matches' array, locking the mutex when
  // whey work with it.
  QMutex dataMutex;

  std::vector< WordMatch > matches;
  bool uncertain;
};

/// A helper class for synchronous word search implementations.
class WordSearchInstant: public WordSearch
{
public:

  WordSearchInstant()
  {
    finish();
  }

  void cancel() override {}

  std::vector< WordMatch > & getMatches()
  {
    return matches;
  }

  void setUncertain( bool value )
  {
    uncertain = value;
  }
};

class Dict: public Base
{
  Q_OBJECT

public:
  explicit Dict( QObject * parent = nullptr ):
    Base( parent ){};


  /// Returns all the data read. Since no further locking can or would be
  /// done, this can only be called after the request has finished.
  virtual std::vector< char > & getFullData() = 0;

  /// Returns the number of bytes read, with a -1 meaning that so far it's
  /// uncertain whether resource even exists or not, and any non-negative value
  /// meaning that that amount of bytes is not available.
  /// If -1 is still being returned after the request has finished, that means
  /// the resource wasn't found.
  virtual long dataSize() = 0;

  virtual void getDataSlice( size_t offset, size_t size, void * buffer ) = 0;

protected:
  bool hasAnyData; // With this being false, dataSize() always returns -1
};

/// This request type corresponds to any kinds of data responses where a
/// single large blob of binary data is returned. It currently used of article
/// bodies and resources.
class Blob: public Dict
{
  Q_OBJECT

public:

  /// Writes "size" bytes starting from "offset" of the data read to the given
  /// buffer. "size + offset" must be <= than dataSize().
  void getDataSlice( size_t offset, size_t size, void * buffer ) override;
  void appendDataSlice( const void * buffer, size_t size );

  /// Returns all the data read. Since no further locking can or would be
  /// done, this can only be called after the request has finished.
  std::vector< char > & getFullData() override;

  /// Returns the number of bytes read, with a -1 meaning that so far it's
  /// uncertain whether resource even exists or not, and any non-negative value
  /// meaning that that amount of bytes is not available.
  /// If -1 is still being returned after the request has finished, that means
  /// the resource wasn't found.
  long dataSize() override;

  Blob( QObject * parent = 0 ):
    Dict( parent ),
    hasAnyData( false )
  {
  }

protected:
  // Subclasses should be filling up the 'data' array, locking the mutex when
  // they work with it.
  QMutex dataMutex;

  bool hasAnyData; // With this being false, dataSize() always returns -1
  std::vector< char > data;
};

/// A helper class for synchronous data read implementations.
class BlobInstant: public Blob
{
public:

  BlobInstant( bool succeeded )
  {
    hasAnyData = succeeded;
    finish();
  }

  BlobInstant( QString const & errorString )
  {
    setErrorString( errorString );
    finish();
  }

  virtual void cancel() {}

  std::vector< char > & getData()
  {
    return data;
  }
};


/// This request type corresponds to any kinds of data responses where a
/// single large blob of binary data is returned. It currently used of article
/// bodies and resources.
class Article: public Dict
{
  Q_OBJECT

public:

  explicit Article( QObject * parent = 0 ):
    Dict( parent ),
    hasAnyData( false )
  {
  }

  std::vector< char > & getFullData() override;

  /// Returns the number of bytes read, with a -1 meaning that so far it's
  /// uncertain whether resource even exists or not, and any non-negative value
  /// meaning that that amount of bytes is not available.
  /// If -1 is still being returned after the request has finished, that means
  /// the resource wasn't found.
  long dataSize() override;

  void getDataSlice( size_t offset, size_t size, void * buffer ) override;


  void appendStrToData( const QStringView & );
  void appendStrToData( const std::string_view & );

protected:
  // Subclasses should be filling up the 'data' array, locking the mutex when
  // they work with it.
  QMutex dataMutex;
  bool hasAnyData; // With this being false, dataSize() always returns -1
  std::string data;
};

/// A helper class for synchronous data read implementations.
class ArticleInstant: public Article
{
public:

  ArticleInstant( bool succeeded )
  {
    hasAnyData = succeeded;
    finish();
  }

  ArticleInstant( QString const & errorString )
  {
    setErrorString( errorString );
    finish();
  }

  virtual void cancel() {}

  std::vector< char > & getData()
  {
    auto * result = new std::vector< char >( data.begin(), data.end() );
    return *result;
  }
};


} // namespace Request
