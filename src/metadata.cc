#include "metadata.hh"
#include "toml++/toml.hpp"
#include <QDebug>
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
