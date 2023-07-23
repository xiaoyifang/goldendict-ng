At `Edit` -> `Dictioanries` -> `Groups`, you can create and edit dictionary groups.

To add a dictionary into a group, just drag it from the dictionary list into the group window on the right. To remove a dict, just drag it back to the dictionary list. Hold `Shift` to select a range of dictionaries or hold `Ctrl` to select multiple dictionaries.

Additionally, multiple strategies of automatic grouping are provided:

* based on the language info embedded within dictionary files
* based on the folder structure
* based on customizable metadata files

## Auto groups by dictionary language

When group by dictionary language, the language is taken from dictionary's built-in metadata which has been embed when creating dictionary.

If the language is not present in the dictionary, it will try to detect the language from the dictionary file name.

Then use the founded language to create dictionary groups.

Groups created in this method also include a context menu when rich click the group name, in which you can do additional dictionaries grouping by source or target language and combine dictionaries in more large groups.

## Auto groups by folders

Click the "Group by folders" will group your dicts based on folder structure.

Two dictionaries will be in the same group if their container folder's direct parent is the same.

![Auto Group By Folder](img/autoGroupByFolder.svg)

For example, the structure below will be auto grouped into two groups:

* `English<>Chinese` with `Cambridge`, `Collins` and `Oxford`
* `English<>Russian` with `dictA`, `dictB`, `Wikipedia_ru` and `Wikipedia`

```
.
├── English<>Chinese
│   ├── Cambridge
│   │   ├── Cambridge.dict.dz
│   │   ├── Cambridge.idx
│   │   ├── Cambridge.info
│   │   └── Cambridge.syn
│   ├── Collins
│   │   ├── Collins.dsl
│   │   └── res.zip
│   └── Oxford
│       ├── Oxford.css
│       ├── Oxford.mdd
│       └── Oxford.mdx
└── English<>Russian
    ├── dsl
    │   ├── dictA.dsl
    │   └── dictB.dsl
    └── zim
        ├── Wikipedia_ru.slob
        └── Wikipedia.zim
```

Note that if two groups share the same name but in different folder, then upper level's folder name will be prepended with group name. The Example below will be grouped into `epistularum/Japanese` and `Mastameta/Japanese`.

More levels of folder nesting are not supported.

```
.
├─epistularum
│   └─Japanese   <- Group
│       └─DictA  <- Dict Files's container folder
|          └─ DictA Files
├─Mastameta
│   └─Japanese   <- Group
|       └─DictB  <- Dict Files's container folder
|          └─ DictB Files  
```

## Auto groups by `metadata.toml`

Click the "group by metadata" will group your dicts based on `metadata.toml`.

The `metadata.toml` should be placed beside dictionary files. One `metadata.toml` for each dictionary.

The metadata file uses [TOML](https://toml.io) format.

```toml
categories = [ "English", "Russian", "Chinese" ]

# the following `langfrom` , `langto` fields have not been supported yet.
[metadata]
name = "New Name"
langfrom = "English"
langto = "Russian"
```

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

The content of the metadata `(A)` is
```toml
categories = ["en-zh", "汉英词典"]
```

The content of the metadata `(B)` is
```toml
categories = ["图片词典", "en-zh", "汉英词典"]
```

The structure above will be auto grouped into three groups:

* `en-zh` with `Cambridge`, `Collins`
* `图片词典` with `Collins`
* `汉英词典` with `Cambridge`,`Collins`

Note: Dictionaries without `metadata.toml` won't be auto-grouped.
