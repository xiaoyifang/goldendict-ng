#include "metadata.hh"
#include <toml++/toml.hpp>
#include <QDebug>
#include <QDir>
#include <QSaveFile>
#include <QFile>
#include <sstream>


namespace {

std::string combinePath( std::string_view folder, std::string_view fileName )
{
  QDir dir( QString::fromStdString( std::string{ folder } ) );
  return dir.filePath( QString::fromStdString( std::string{ fileName } ) ).toStdString();
}

std::string legacyFilePath( std::string_view folder )
{
  return combinePath( folder, "metadata.toml" );
}

bool fileExists( std::string_view filepath )
{
  return QFile::exists( QString::fromStdString( std::string{ filepath } ) );
}

toml::table parseFile( std::string_view filepath )
{
  try {
    return toml::parse_file( filepath );
  }
  catch ( toml::parse_error & e ) {
    qWarning() << "Failed to load metadata: " << QString::fromUtf8( filepath.data(), filepath.size() )
               << "Reason:" << e.what();
    return {};
  }
}

/**
 * Table to be modified before writing to the per-dictionary file.
 */
toml::table loadTableForSave( std::string_view folder, std::string_view dictId )
{
  const auto perDictPath = Metadata::dictionaryFilePath( folder, dictId );
  if ( fileExists( perDictPath ) ) {
    return parseFile( perDictPath );
  }

  return {};
}

void writeFile( std::string_view filepath, const toml::table & tbl )
{
  QSaveFile file( QString::fromStdString( std::string{ filepath } ) );
  if ( file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
    std::stringstream ss;
    ss << tbl;
    file.write( ss.str().c_str() );
    file.commit();
  }
}

} // namespace


std::string Metadata::dictionaryFilePath( std::string_view folder, std::string_view dictId )
{
  return combinePath( folder, std::string{ dictId } + ".toml" );
}

std::optional< Metadata::result > Metadata::loadForDictionary( std::string_view folder, std::string_view dictId )
{
  const auto perDictPath = dictionaryFilePath( folder, dictId );
  if ( fileExists( perDictPath ) ) {
    return load( perDictPath );
  }

  const auto legacyPath = legacyFilePath( folder );
  if ( fileExists( legacyPath ) ) {
    return load( legacyPath );
  }

  return std::nullopt;
}

void Metadata::migrateLegacyFile( std::string_view folder, std::string_view dictId )
{
  const auto legacyPath = legacyFilePath( folder );
  if ( !fileExists( legacyPath ) ) {
    return;
  }

  const auto perDictPath = dictionaryFilePath( folder, dictId );
  if ( fileExists( perDictPath ) ) {
    // The per-dictionary file already exists; leave the legacy file in place
    // so that the other dictionaries in the folder can still fall back to it.
    return;
  }

  if ( !QFile::rename( QString::fromStdString( legacyPath ), QString::fromStdString( perDictPath ) ) ) {
    qWarning() << "Failed to migrate metadata file to:" << QString::fromStdString( perDictPath );
  }
}

std::optional< Metadata::result > Metadata::load( std::string_view filepath )
{
  if ( !fileExists( filepath ) ) {
    return std::nullopt;
  }

  // by default, the optional will be initialized as std::nullopt
  Metadata::result result{};
  toml::table tbl;

  try {
    tbl = toml::parse_file( filepath );
  }
  catch ( toml::parse_error & e ) {
    qWarning() << "Failed to load metadata: " << QString::fromUtf8( filepath.data(), filepath.size() )
               << "Reason:" << e.what();

    return std::nullopt;
  }

  if ( toml::array * categories = tbl.get_as< toml::array >( "categories" ) ) {
    // result.categories is an optional, it exists, but the vector does not, so we have to create one here
    result.categories.emplace();
    for ( auto & el : *categories ) {
      if ( el.is_string() ) {
        result.categories.value().emplace_back( std::move( *el.value_exact< std::string >() ) );
      }
    }
  }

  result.name = tbl[ "metadata" ][ "name" ].value_exact< std::string >();

  const auto fullindex = tbl[ "fts" ];
  if ( fullindex.as_string() ) {
    const auto value = fullindex.as_string()->get();
    result.fullindex = value == "1" || value == "on" || value == "true";
  }
  else if ( fullindex.as_boolean() ) {
    auto value       = fullindex.as_boolean()->get();
    result.fullindex = value;
  }
  else if ( fullindex.as_integer() ) {
    const auto value = fullindex.as_integer()->get();
    result.fullindex = value > 0;
  }
  return result;
}

void Metadata::saveDisplayName( std::string_view folder, std::string_view dictId, std::string_view name )
{
  toml::table tbl = loadTableForSave( folder, dictId );

  auto * metadata = tbl[ "metadata" ].as_table();
  if ( !metadata ) {
    tbl.insert_or_assign( "metadata", toml::table{} );
    metadata = tbl[ "metadata" ].as_table();
  }

  if ( name.empty() ) {
    metadata->erase( "name" );
  }
  else {
    metadata->insert_or_assign( "name", name );
  }

  writeFile( dictionaryFilePath( folder, dictId ), tbl );
}

void Metadata::saveFullIndex( std::string_view folder, std::string_view dictId, bool enabled )
{
  toml::table tbl = loadTableForSave( folder, dictId );

  tbl.insert_or_assign( "fts", enabled );

  writeFile( dictionaryFilePath( folder, dictId ), tbl );
}
