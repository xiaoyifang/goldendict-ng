Here you can create and edit dictionary groups. To add dictionary into group just drag it into groups window, to remove it drag it back to dictionaries list. Also you can press "Auto groups button" to automatically create groups for all presented in dictionaries list language directions. Via context menu of such automatically created groups you can execute additional dictionaries grouping by source or target language and combine dictionaries in more large groups.

## Auto groups by folders

Click the "Auto groups by folders" will group your dicts based on folder structure.

Two dictionaries will be in the same group if their container folder's direct parent are the same.

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