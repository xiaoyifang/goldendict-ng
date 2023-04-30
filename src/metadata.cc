#include "metadata.hh"
#include "toml.hpp"
#include <QDebug>

std::optional< Metadata::result > Metadata::load( std::string_view filepath )
{
  // by default, the optional will be initialized as std::nullopt
  Metadata::result result{};
  toml::table tbl;

  try {
    tbl = toml::parse_file( filepath );
  }
  catch ( toml::parse_error & e ) {
    qWarning()<< "Failed to load metadata: " << QString::fromUtf8(filepath.data(),filepath.size())
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
  return result;
}
