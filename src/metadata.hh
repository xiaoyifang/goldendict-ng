#pragma once
#include <QStringView>
#include <optional>
#include <vector>

namespace Metadata {

/**
 * Represent the metadata.toml beside the dictionary files
 */
struct result
{
  std::optional< std::vector< std::string > > categories;
  std::optional< std::string > name;
};

[[nodiscard]] std::optional< Metadata::result > load( std::string_view filepath );

} // namespace Metadata
