#pragma once
#include <QStringView>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace Metadata {

/**
 * Represent the metadata file beside the dictionary files.
 *
 * The per-dictionary file is named `<main-file-name>.toml`, where
 * `<main-file-name>` is the base name (with extension) of the dictionary's
 * main file, e.g. `longman.mdx.toml`. The legacy `metadata.toml`, which is
 * shared by all dictionaries in the same folder, is still read as a fallback
 * for backward compatibility.
 */
struct result
{
  std::optional< std::vector< std::string > > categories;
  std::optional< std::string > name;
  std::optional< bool > fullindex;
};

[[nodiscard]] std::optional< Metadata::result > load( std::string_view filepath );

/// Path of the per-dictionary metadata file: `<folder>/<fileStem>.toml`.
[[nodiscard]] std::string dictionaryFilePath( std::string_view folder, std::string_view fileStem );

/// Loads `<fileStem>.toml` if it exists, otherwise falls back to the legacy
/// `<folder>/metadata.toml`.
[[nodiscard]] std::optional< Metadata::result > loadForDictionary( std::string_view folder,
                                                                  std::string_view fileStem );

/// One-time migration of the legacy shared `metadata.toml`: renames it to
/// `<fileStem>.toml` of the first dictionary in `folder`. The shared file
/// couldn't represent several dictionaries anyway.
void migrateLegacyFile( std::string_view folder, std::string_view fileStem );

void saveDisplayName( std::string_view folder, std::string_view fileStem, std::string_view name );
void saveFullIndex( std::string_view folder, std::string_view fileStem, bool enabled );

} // namespace Metadata
