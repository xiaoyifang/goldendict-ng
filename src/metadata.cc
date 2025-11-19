#include "metadata.hh"
#include "toml++/toml.hpp"
#include <QDebug>
#include <QSaveFile>
#include <QFile>

std::optional< Metadata::result > Metadata::load( std::string_view filepath )
{
  if ( !QFile::exists( QString::fromStdString( std::string{ filepath } ) ) ) {
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

void Metadata::saveDisplayName( std::string_view filepath, std::string_view name )
{
  toml::table tbl;
  if ( QFile::exists( QString::fromStdString( std::string{ filepath } ) ) ) {
    try {
      tbl = toml::parse_file( filepath );
    }
    catch ( toml::parse_error & e ) {
      qWarning() << "Failed to load metadata: " << QString::fromUtf8( filepath.data(), filepath.size() )
                 << "Reason:" << e.what();
      // Continue with an empty table
    }
  }

  if ( !tbl.contains( "metadata" ) ) {
    tbl.emplace( "metadata", toml::table{} );
  }

  tbl[ "metadata" ].as_table()->insert_or_assign( "name", name );

  QSaveFile file( QString::fromStdString( std::string{ filepath } ) );
  if ( file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
    std::stringstream ss;
    ss << tbl;
    file.write( ss.str().c_str() );
    file.commit();
  }
}
