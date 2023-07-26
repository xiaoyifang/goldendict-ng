A `metadata.toml`, which uses [toml](https://toml.io) format, can be used to customize or override some properties of a certain dictionary. It is also used in [auto grouping by metadata](manage_groups.md)

The `metadata.toml` should be placed on each dictionary's root folder (beside dictionary files) like

```
.
├── Cambridge
│    ├── metadata.toml < here
│    ├── Cambridge.idx
│    ├── Cambridge.info
│    ├── Cambridge.syn
│    └── Cambridge.dict.dz    
└── Collins
     ├── metadata.toml < here
     ├── res.zip
     └── Collins.dsl 
```

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
