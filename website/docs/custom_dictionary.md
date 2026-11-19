A metadata file, which uses [toml](https://toml.io) format, can be used to customize or override some properties of a certain dictionary. It is also used in [auto grouping by metadata](manage_groups.md)

The metadata file should be placed on each dictionary's root folder (beside dictionary files). It is named after the dictionary's main file (with its extension) plus a `.toml` suffix — for example `Cambridge.ifo.toml` for a StarDict dictionary, or `Collins.dsl.toml` for a DSL dictionary.

```
.
├── Cambridge
│    ├── Cambridge.ifo.toml < here
│    ├── Cambridge.ifo
│    ├── Cambridge.idx
│    ├── Cambridge.info
│    ├── Cambridge.syn
│    └── Cambridge.dict.dz
└── Collins
     ├── Collins.dsl.toml < here
     ├── res.zip
     └── Collins.dsl
```

For backward compatibility, the legacy `metadata.toml` is read when the per-dictionary file does not exist. It is migrated automatically on load by renaming it to the `<main-file-name>.toml` of the first dictionary in the folder; the other dictionaries in that folder are no longer affected by it. This requires a writable dictionary folder; otherwise `metadata.toml` keeps being read as a fallback.

## Override display name

Some dictionary formats' display is embedded inside the dictionary, which cannot be easily changed, but you can add the following to override it.

```toml
[metadata]
name = "New Name"
```

This `New Name` will be appeared as the dictionary name.

## Disable full-text search

```toml
fts=false

[metadata]
name="New Name"
```

The `fts` field's value can be `on/off`, `1/0` ,`true/false` etc.

```
fts=false
```

will disable the current dictionary's full-text search.

You can check the full-text search status on each dictionary's info dialog.

![](img/dictionary-info-fullindex.png)

Note that it is possible to enable full text for a single dictionary by disabling full-text search in the Preferences dialog, and set `fts=true` for that dictionary.
