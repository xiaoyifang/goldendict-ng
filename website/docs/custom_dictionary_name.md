You can customize the dictionary name by the metadata.toml which is also used in [metadata grouping](manage_groups.md)

## About the configuration of metadata.toml

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

