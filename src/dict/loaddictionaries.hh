/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "initializing.hh"
#include "config.hh"
#include "dict/dictionary.hh"

#include <QThread>
#include <QNetworkAccessManager>
#include <QStringList>

/// Use loadDictionaries() function below -- this is a helper thread class
class LoadDictionaries: public QThread, public Dictionary::Initializing
{
  Q_OBJECT

  QStringList nameFilters;
  Config::Paths const & paths;
  Config::SoundDirs const & soundDirs;
  Config::Hunspell const & hunspell;
  Config::Transliteration const & transliteration;
  std::vector< sptr< Dictionary::Class > > dictionaries;
  QStringList exceptionTexts;
  unsigned int maxHeadwordSize;
  unsigned int maxHeadwordToExpand;

public:

  LoadDictionaries( Config::Class const & cfg );

  virtual void run();

  std::vector< sptr< Dictionary::Class > > const & getDictionaries() const
  {
    return dictionaries;
  }

  /// Empty string means to exception occurred
  std::string const & getExceptionText() const
  {
    return exceptionTexts.join( "\n" ).toStdString();
  }


public:

  virtual void indexingDictionary( std::string const & dictionaryName ) noexcept;
  virtual void loadingDictionary( std::string const & dictionaryName ) noexcept;

private:

  void handlePath( Config::Path const & );

  // Helper function that will add a vector of dictionary::Class to the dictionary list
  void addDicts( const std::vector< sptr< Dictionary::Class > > & dicts );

signals:
  void indexingDictionarySignal( QString const & dictionaryName );
  void loadingDictionarySignal( QString const & dictionaryName );
};

/// Loads all dictionaries mentioned in the configuration passed, into the
/// supplied array. When necessary, a window would pop up describing the process.
/// If showInitially is passed as true, the window will always popup.
/// If doDeferredInit is true (default), doDeferredInit() is done on all
/// dictionaries at the end.
void loadDictionaries( QWidget * parent,
                       Config::Class const & cfg,
                       std::vector< sptr< Dictionary::Class > > &,
                       QNetworkAccessManager & dictNetMgr,
                       bool doDeferredInit = true );

/// Runs deferredInit() on all the given dictionaries. Useful when
/// loadDictionaries() was previously called with doDeferredInit = false.
void doDeferredInit( std::vector< sptr< Dictionary::Class > > & );
