#include "headwordsmodel.hh"

HeadwordListModel::HeadwordListModel( QObject * parent ):
  QAbstractListModel( parent ),
  filtering( false ),
  totalSize( 0 ),
  finished( false ),
  index( 0 ),
  ptr( nullptr ),
  cacheMode( false )
{
}

int HeadwordListModel::rowCount( const QModelIndex & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  if ( cacheMode ) {
    // Matches mode: regex/wildcard results capped at maxFilterResults
    if ( !filterMatches.isEmpty() ) {
      return filterMatches.size();
    }
    // filterBegin != filterEnd means we have an active prefix filter
    if ( filterBegin != filterEnd ) {
      return static_cast< int >( filterEnd - filterBegin );
    }
    return static_cast< int >( hwCache.count() );
  }
  return words.size();
}

int HeadwordListModel::totalCount() const
{
  return totalSize;
}

int HeadwordListModel::wordCount() const
{
  if ( cacheMode ) {
    return static_cast< int >( hwCache.count() );
  }
  return words.size();
}

bool HeadwordListModel::isFinish() const
{
  if ( cacheMode ) {
    return true;
  }
  return finished || ( words.size() >= totalSize );
}

// export headword
QString HeadwordListModel::getRow( int row )
{
  if ( cacheMode ) {
    return hwCache.at( static_cast< uint32_t >( row ) );
  }
  if ( fileSortedList.empty() ) {
    fileSortedList << words;
    fileSortedList.sort();
  }
  return fileSortedList.at( row );
}

void HeadwordListModel::setFilter( const QRegularExpression & reg )
{
  // ===== Cache mode: fast prefix filtering via binary search =====
  if ( cacheMode ) {
    filtering = !reg.pattern().isEmpty();

    // No filter text -- show all
    if ( reg.pattern().isEmpty() ) {
      // If there was an active filter (range or match list), clear it
      if ( filterBegin != filterEnd || !filterMatches.isEmpty() ) {
        filterBegin = filterEnd = 0; // 0,0 = "show all"
        filterMatches.clear();
        beginResetModel();
        endResetModel();
      }
      emit numberPopulated( static_cast< int >( hwCache.count() ) );
      return;
    }

    // Extract the literal prefix from the regex pattern.
    // DictHeadwords sets up patterns as follows (see dictheadwords.cc getFilterRegex()):
    //   Text  → QRegularExpression::escape(text)      -> "\Qtext\E" (pure literal)
    //   Wildcard → wildcardsToRegexp(text)            -> may start with "\Q"
    //   Regex → user input as-is
    QString prefixText;
    if ( reg.pattern().startsWith( "\\Q" ) ) {
      // QRegularExpression::escape produces \Q...\E; grab literal inside
      int end = reg.pattern().indexOf( "\\E" );
      if ( end > 3 ) {
        prefixText = reg.pattern().mid( 2, end - 2 );
      }
    }

    // Pure literal ("\Q...\E" wrapping the whole pattern, i.e. Text mode)
    // → contiguous range via binary search.
    // Anything else (wildcard/regex) → linear scan with early exit.
    const bool pureLiteral =
      !prefixText.isEmpty() && reg.pattern() == ( "\\Q" + prefixText + "\\E" );

    const uint32_t n = hwCache.count();

    // Skip the alphabetically smaller part of the cache when a literal
    // prefix is known (works for Text mode and wildcard with leading literal).
    uint32_t lb = 0;
    if ( !prefixText.isEmpty() ) {
      uint32_t lo = 0, hi = n;
      while ( lo < hi ) {
        uint32_t mid = lo + ( hi - lo ) / 2;
        if ( QString::compare( hwCache.at( mid ), prefixText, Qt::CaseInsensitive ) < 0 ) {
          lo = mid + 1;
        }
        else {
          hi = mid;
        }
      }
      lb = lo;
    }

    beginResetModel();
    filterMatches.clear();

    if ( !reg.isValid() ) {
      // Invalid regex -- behave like an empty filter.
      filterBegin = filterEnd = 0;
    }
    else if ( pureLiteral ) {
      // upper_bound -- search for prefixText+U+FFFF (anything starting with
      // the prefix is strictly less than this sentinel)
      const QString sentinel = prefixText + QChar( 0xFFFF );
      uint32_t lo = 0, hi = n;
      while ( lo < hi ) {
        uint32_t mid = lo + ( hi - lo ) / 2;
        if ( QString::compare( hwCache.at( mid ), sentinel, Qt::CaseInsensitive ) < 0 ) {
          lo = mid + 1;
        }
        else {
          hi = mid;
        }
      }
      uint32_t ub = lo;

      // Limit the range, matching the legacy prefixMatch behaviour
      // ("filtered (limited)").
      if ( maxFilterResults > 0 && ub - lb > static_cast< uint32_t >( maxFilterResults ) ) {
        ub = lb + static_cast< uint32_t >( maxFilterResults );
      }

      filterBegin = lb;
      filterEnd   = ub;
    }
    else {
      // Wildcard / Regex: scan the cache from lb, collect matching rows into
      // filterMatches and stop as soon as maxFilterResults is reached.
      filterBegin = filterEnd = 0;
      for ( uint32_t i = lb; i < n; ++i ) {
        if ( maxFilterResults > 0 && filterMatches.size() >= maxFilterResults ) {
          break;
        }
        if ( reg.match( hwCache.at( i ) ).hasMatch() ) {
          filterMatches.append( i );
        }
      }
    }

    endResetModel();
    emit numberPopulated( static_cast< int >( hwCache.count() ) );
    return;
  }

  // ===== Legacy btree mode (unchanged) =====
  // if the headword is already finished loaded, do nothing。
  if ( finished ) {
    return;
  }
  //back to normal state ,restore the original model;
  if ( reg.pattern().isEmpty() ) {
    QMutexLocker _( &lock );
    //race condition.
    if ( !filtering ) {
      return;
    }
    filtering = false;

    //reset to previous models
    beginResetModel();
    words = QStringList( original_words );
    endResetModel();

    return;
  }
  else {
    QMutexLocker _( &lock );
    //the first time to enter filtering mode.
    if ( !filtering ) {
      filtering      = true;
      original_words = QStringList( words );
    }
  }
  filterWords.clear();
  auto sr = _dict->prefixMatch( Text::removeTrailingZero( reg.pattern() ), maxFilterResults );
  connect( sr.get(), &Dictionary::Request::finished, this, [ this, sr ]() {
    requestFinished( sr );
  } );
}

void HeadwordListModel::appendWord( const QString & word )
{
  hashedWords.insert( word );
  words.append( word );
}

void HeadwordListModel::requestFinished( const sptr< Dictionary::WordSearchRequest > & request )
{
  if ( request->isFinished() ) {
    if ( !request->getErrorString().isEmpty() ) {
      qDebug() << "error:" << request->getErrorString();
    }
    else if ( request->matchesCount() ) {
      auto allmatches = request->getAllMatches();
      for ( auto & match : allmatches ) {
        filterWords.append( QString::fromStdU32String( match.word ) );
      }
    }
  }


  if ( filterWords.isEmpty() ) {
    return;
  }
  beginResetModel();
  words = QStringList( filterWords );
  endResetModel();
  emit numberPopulated( words.size() );
}

QVariant HeadwordListModel::data( const QModelIndex & index, int role ) const
{
  if ( !index.isValid() ) {
    return {};
  }

  if ( role == Qt::DisplayRole ) {
    if ( cacheMode ) {
      uint32_t row;
      if ( !filterMatches.isEmpty() ) {
        // matches mode -- row indexes into the collected match list
        if ( index.row() < 0 || index.row() >= filterMatches.size() ) {
          return {};
        }
        row = filterMatches.at( index.row() );
      }
      else if ( filterBegin != filterEnd ) {
        // filtered mode -- row is relative to the filtered range
        if ( index.row() >= static_cast< int >( filterEnd - filterBegin ) || index.row() < 0 ) {
          return {};
        }
        row = filterBegin + static_cast< uint32_t >( index.row() );
      }
      else {
        // unfiltered mode
        if ( index.row() < 0 || static_cast< uint32_t >( index.row() ) >= hwCache.count() ) {
          return {};
        }
        row = static_cast< uint32_t >( index.row() );
      }
      return hwCache.at( row );
    }
    if ( index.row() >= totalSize || index.row() < 0 || index.row() >= words.size() ) {
      return {};
    }
    return words.at( index.row() );
  }
  return {};
}

bool HeadwordListModel::canFetchMore( const QModelIndex & parent ) const
{
  if ( parent.isValid() ) {
    return false;
  }
  // Cache has all headwords available via mmap -- no fetching needed.
  if ( cacheMode || filtering || finished ) {
    return false;
  }
  return ( words.size() < totalSize );
}

void HeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if ( parent.isValid() || cacheMode || filtering || finished ) {
    return;
  }

  //arbitrary number
  if ( totalSize < HEADWORDS_MAX_LIMIT ) {
    finished = true;

    // getHeadwords() may deduplicate headwords (e.g. via a QSet) and clear the
    // passed-in list, so the actually inserted count can be less than
    // totalSize. Fetch into a temp list first, then announce the real count,
    // otherwise QSortFilterProxyModel creates phantom rows that render blank.
    QStringList fresh;
    _dict->getHeadwords( fresh );
    if ( !fresh.isEmpty() ) {
      beginInsertRows( QModelIndex(), words.size(), words.size() + fresh.size() - 1 );
      words += fresh;
      endInsertRows();
    }
    // getHeadwords() returns deduplicated words, which can be fewer than the
    // index-reported getWordCount(). Sync totalSize to the actual count so
    // totalCount() no longer over-reports.
    totalSize = words.size();
    emit numberPopulated( words.size() );
    return;
  }


  QSet< QString > headword;
  QMutexLocker _( &lock );

  _dict->findHeadWordsWithLenth( index, &headword, 1000 );
  if ( headword.isEmpty() ) {
    // All index nodes have been consumed. words (deduplicated per batch) may
    // never reach totalSize (which counts raw index entries, duplicates
    // included), so mark loading as finished to stop the view from calling
    // fetchMore (and thus a full findNodes() tree walk) over and over.
    finished = true;
    return;
  }

  beginInsertRows( QModelIndex(), words.size(), words.size() + headword.count() - 1 );
  for ( const auto & word : std::as_const( headword ) ) {
    words << word;
  }
  endInsertRows();

  emit numberPopulated( words.size() );
}

int HeadwordListModel::getCurrentIndex() const
{
  return index;
}

bool HeadwordListModel::containWord( const QString & word )
{
  if ( cacheMode ) {
    // hwCache is sorted case-insensitively; binary search
    uint32_t lo = 0, hi = hwCache.count();
    while ( lo < hi ) {
      uint32_t mid = lo + ( hi - lo ) / 2;
      int cmp      = QString::compare( hwCache.at( mid ), word, Qt::CaseInsensitive );
      if ( cmp < 0 ) {
        lo = mid + 1;
      }
      else if ( cmp > 0 ) {
        hi = mid;
      }
      else {
        return true;
      }
    }
    return false;
  }
  return hashedWords.contains( word );
}

void HeadwordListModel::setMaxFilterResults( int _maxFilterResults )
{
  this->maxFilterResults = _maxFilterResults;
}

QSet< QString > HeadwordListModel::getRemainRows( int & nodeIndex )
{
  if ( cacheMode ) {
    // Cache is fully loaded -- return everything on first call, empty on next
    if ( nodeIndex > 0 ) {
      return {};
    }
    nodeIndex = 1; // mark as "done"
    QSet< QString > all;
    hwCache.forEach( [ &all ]( const QString & hw ) {
      all.insert( hw );
    } );
    return all;
  }

  QSet< QString > headword;
  QMutexLocker _( &lock );
  _dict->findHeadWordsWithLenth( nodeIndex, &headword, 10000 );

  QSet< QString > filtered;
  for ( const auto & word : std::as_const( headword ) ) {
    if ( !containWord( word ) ) {
      filtered.insert( word );
    }
  }
  return filtered;
}

void HeadwordListModel::setDict( Dictionary::Class * dict )
{
  _dict     = dict;
  totalSize = _dict->getWordCount();

  // Try to open the headword cache file (fast path)
  cacheMode = false;
  words.clear();
  fileSortedList.clear();
  hashedWords.clear();
  filtering   = false;
  finished    = false;
  index       = 0;
  filterBegin = filterEnd = 0; // reset filter range
  filterMatches.clear();

  if ( dict->supportsHeadwordsCache() ) {
    const auto path = dict->headwordsCachePath();
    if ( !path.empty() && hwCache.open( path ) ) {
      cacheMode = true;
      emit numberPopulated( static_cast< int >( hwCache.count() ) );
    }
  }
}

void HeadwordListModel::refreshCache()
{
  if ( !_dict || !_dict->supportsHeadwordsCache() ) {
    return;
  }

  const auto path = _dict->headwordsCachePath();
  if ( path.empty() ) {
    return;
  }

  // Already in cache mode -- nothing to do
  if ( cacheMode && hwCache.isValid() ) {
    return;
  }

  // Try to open -- if successful, switch to cache mode
  HeadwordCache::HeadwordCache newCache;
  if ( newCache.open( path ) ) {
    beginResetModel();
    hwCache   = std::move( newCache );
    cacheMode = true;
    // Free legacy data
    words.clear();
    fileSortedList.clear();
    hashedWords.clear();
    // Reset filter -- caller (DictHeadwords::refreshModelCache) will re-apply
    filterBegin = filterEnd = 0;
    filterMatches.clear();
    filtering               = false;
    endResetModel();
    emit numberPopulated( static_cast< int >( hwCache.count() ) );
  }
}
