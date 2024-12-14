//
// Created by xiaoyifang on 2024/12/10.
//

#include "XapianIdx.h"
void XapianIdx::buildIdx( const std::string & path, std::map< std::string, std::string > & indexedWords )
{
  Xapian::WritableDatabase db( path, Xapian::DB_CREATE_OR_OPEN );

  Xapian::TermGenerator indexer;
  //  Xapian::Stem stemmer("english");
  //  indexer.set_stemmer(stemmer);
  //  indexer.set_stemming_strategy(indexer.STEM_SOME_FULL_POS);
  indexer.set_flags( Xapian::TermGenerator::FLAG_CJK_NGRAM );

  for ( auto const & address : indexedWords ) {
    Xapian::Document doc;

    indexer.set_document( doc );

    indexer.index_text( address.first );

    doc.set_data( address.second );
    // Add the document to the database.
    db.add_document( doc );
  }

  db.commit();

  //  db.compact( dict->ftsIndexName() );

  db.close();
}
