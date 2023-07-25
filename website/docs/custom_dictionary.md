You can customize the dictionary by the metadata.toml which is also used in [metadata grouping](manage_groups.md)

## About the configuration of metadata.toml



the metadata.toml use toml format for configuration.

## Customize the name of the dictionary

```toml
[metadata]
name = "New Name"
```

this `New Name` will be appeared as the dictionary name.


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

## Disable the fullindex of the dictionary

```toml
fts=false

[metadata]
name="New Name"

```

this `fullindex` field's value can be on/off, 1/0 ,true/false etc.

```
fullindex=false
```
will disable the current dictionary's fullindex feature.


you can check the fullindex feature on each dictionary info dialog.

![](img/dictionary-info-fullindex.png)