#pragma once
#include <QStringView>
#include <string_view>
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
  std::optional< bool > fullindex;
};

[[nodiscard]] std::optional< Metadata::result > load( std::string_view filepath );
void saveDisplayName( std::string_view filepath, std::string_view name );

} // namespace Metadata
