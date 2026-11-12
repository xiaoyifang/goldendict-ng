/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"

/// Support for LSA (.dat, .lsa) audio archives of the Lingo program.
namespace Lsa {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > >
makeDictionaries( const vector< string > & fileNames, const string & indicesDir, Dictionary::Initializing & );

struct LsaEntryInfo
{
  string name;    // UTF-8 entry name, with the ".wav" extension stripped
  uint32_t offset; // Byte offset of the entry record inside the archive
};

/// Parses a Lingvo sound archive (.lsa/.dat).
/// Returns false if the file is not a valid LSA archive. On success fills
/// \p entries and sets \p vorbisOffset to the offset of the embedded
/// Ogg stream which contains all of the sounds.
bool parseLsaEntries( const string & lsaFilePath,
                      std::vector< LsaEntryInfo > & entries,
                      uint32_t & vorbisOffset );

/// Decodes the sound stored at \p entryOffset of the archive into WAV data
/// (a PCM16 header followed by the samples).
/// Throws Dictionary::Ex-derived exceptions on failure.
void loadLsaSound( const string & lsaFilePath,
                   uint32_t entryOffset,
                   uint32_t vorbisOffset,
                   std::vector< char > & data );

} // namespace Lsa
