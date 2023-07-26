## About the configuration of metadata.toml

A `metadata.toml`, which uses [toml](https://toml.io) format, can be placed on each dictionary's root folder to customize or override some properties. It is also used in [auto grouping by metadata](manage_groups.md)

## Customize the name of the dictionary

```toml
[metadata]
name = "New Name"
```

This `New Name` will be appeared as the dictionary name.

The `metadata.toml` should be placed beside dictionary files. One `metadata.toml` for each dictionary.

The metadata file uses [TOML](https://toml.io) format.

For example,

```
.
├── Cambridge
│    ├── metadata.toml     (A)
│    ├── Cambridge.idx
│    ├── Cambridge.info
│    ├── Cambridge.syn
│    └── Cambridge.dict.dz    
└── Collins
     ├── metadata.toml
     ├── res.zip
     └── Collins.dsl       (B)  

```

## Disable full-text search for certain dictionary

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

you can check the full-text search status on each dictionary's info dialog.

![](img/dictionary-info-fullindex.png)
