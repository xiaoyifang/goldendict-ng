Popular dictionary formats are all supported.

## Local Dictionaries Sources

* [MDict](https://www.mdict.cn/) dictionaries (.mdx/.mdd)
* [StarDict](http://www.huzheng.org/stardict/) dictionaries (.ifo/.dict./.idx/.syn)
* [DSL](https://lingvoboard.ru/store/html/DSLReference_HTML/index.html) dictionaries (ABBYY Lingvo source files .dsl(.dz))
* [XDXF](https://github.com/soshial/xdxf_makedict) dictionaries (.xdxf(.dz))
* [Zim](https://wiki.openzim.org/wiki/OpenZIM) dictionaries (.zim)
* [Slob (Aard 2)](https://aarddict.org/) dictionaries (.slob)
* [DictD](https://en.wikipedia.org/wiki/DICT#Dict_file_format) dictionaries (.index/.dict(.dz))
* [Epwing](<https://ja.wikipedia.org/wiki/EPWING>) dictionaries
* Aard Dictionary dictionaries, outdated predecessor of Slob (.aar)
* [SDictionary](http://swaj.net/sdict/index.html) dictionaries (.dct)
* [Babylon glossary builder](https://www.babylon-software.com/glossary-builder/) source files (.gls(.dz))
* Babylon dictionaries, complete support with images and resources (.BGL)
* ABBYY Lingvo sound archives (.lsa/.dat)

* Sound files in separate folders. File names are used as word
* Zipped sound pack. Sound files zipped, but with extension changed from .zip to (.zips)

## Network Sources

* Wikipedia and Wiktionary
* [DICT](https://en.wikipedia.org/wiki/DICT) protocol
* LinguaLibre/Forvo pronunciations
* Any sites which allow set target word in the address line

## Other Sources

Various special "dictionaries" can be added, such as Programs, TTS, Morphology, Transliteration, etc... Their doc is located at [Sources Management](manage_sources.md)

## Additional info

### Converting between formats

GoldenDict-ng does not provide any dictionary modification functionality.

To convert between formats, try tools like [pyglossary](https://github.com/ilius/pyglossary).

### Custom Dictionary Icon

Every local dictionary can have a custom icon.
Supported formats are PNG, JPEG, GIF, SVG, WebP, ICO and BMP.

For most dictionary formats, a dictionary's icon file should have the same basename as the main dictionary file and be placed beside the main dictionary file. For example:

```
.
├── My_best_dictionary.mdx <- the main dictionary file
└── My_best_dictionary.svg <- the icon
```

For XDXF, GD will prioritize to use `icon16.png`, `icon32.png` or `dict.bmp` in the dictionary folder as icon.

For Epwing, the icon file should be placed beside the `catalogs` file and have the same name as the parent folder that contains `catalogs` file.


### Stardict

Main file of Stardict dictionary (.dict) can be compressed by Dictzip program to reduce its size.

[Resource storage](https://github.com/huzheng001/stardict-3/blob/96b96d89eab5f0ad9246c2569a807d6d7982aa84/dict/doc/StarDictFileFormat#L465-L469) folder (images, style sheets, etc.) should be named as `res` beside other dictionary files.

The `res` folder can be compressed into `res.zip` or `my_name.res.zip` (for main dict filename `my_name.ifo/dict`). This zip file should also be placed beside other dictionary files. The internal structure of the zip file should not have a top level folder.

### ABBYY Lingvo (.dsl)

Main file of ABBYY Lingvo dictionary (.dsl) can be compressed by Dictzip program to reduce its size.

Additional dictionary resources (images, sound files, etc.) also can be compressed into zip archive. This archive must be named on main dictionary file name (include extension) with adding ".files.zip" and placed beside other dictionary files. If main file of your dictionary, for example, named "My_best_dictionary.dsl" therefore archive with resources must be named "My_best_dictionary.dsl.files.zip".

GoldenDict supports the "#SOUND_DICTIONARY" directive. Sounds missing in the resources of the dictionary will be searched first in the dictionary specified in this directive. The dictionary name in this directive must match the display name of a sound dir or a Lingvo sound archive that is already added to GoldenDict.

### DictD and XDXF compression

Main file of DictD dictionary (.dict) or XDXF dictionary (.xdxf) can be compressed into `*.dict.dz` or `*.xdxf.dz` by Dictzip program to reduce its size.

### Slob

GoldenDict can render TeX formulas when a slob dictionary bundles MathJax.

### GLS

Main file of Babylon source dictionary must be in UTF-8 (or UTF-16 with BOM) encoding. It can be compressed by Dictzip program to reduce its size. Golden Dict read from dictionary header "### Glossary title:", "### Author:", "### Description:", "### Source language:" and "### Target language:" fields only. Dictionary header must be concluded with "### Glossary section:" mark.

### ABBYY Lingvo sound archives

ABBYY Lingvo sound archives are set of sound files packed into one file of some specific format. These files can be called directly from matches list or from dictionary articles.

There are two kinds of such archives:

- **Universal archives**: every entry is named after the real word it pronounces (e.g. an entry named `hello` holds the sound for the word "hello"). These archives can be shared between different dictionaries and can be placed inside a [sound dir](#sound-files-in-separate-folders).
- **Dictionary-specific archives**: the DSL dictionary references its sounds by internal ids (e.g. the article contains `1.wav` while the archive has an entry named `1`). The mapping between the ids and the words exists only inside that specific dictionary, so such an archive must be placed in the same folder as the DSL dictionary it was made for. Putting it into a sound dir will not work and may even play wrong sounds for other dictionaries.

An easy way to tell the difference: add the archive to GoldenDict as a standalone dictionary. If it can be queried directly with real words, it is a universal one; if it only works together with a specific DSL dictionary, it is a dictionary-specific one.

### Sound files in separate folders

The separate folder with sound files can be added into GoldenDict dictionaries list. GoldenDict will handle this folders like ABBYY Lingvo sound archives (inapplicable in portable mode).

Lingvo sound archives (.lsa) placed inside the folder are supported as well: their entries are indexed like ordinary sound files. Only [universal archives](#abbyy-lingvo-sound-archives) are usable this way — dictionary-specific archives that name their sounds with internal ids must stay beside their DSL dictionary instead. Sounds from the sound dir (plain files as well as universal .lsa archives) are also used by DSL dictionaries as a fallback when a sound cannot be found in the dictionary's own resources.

### Zips sound packs

Zips sound pack is zip archive with extension ".zips" contains set of sound files. To create suck pack it needs to compress sound files into zip archive and change extension of this archive to ".zips". GoldenDict will handle such sound packs like ABBYY Lingvo sound archives.

### General notes

At every launch GoldenDict scan folders with dictionaries to create dictionaries list. The more files in these folder is the more time for scanning. Therefore it is recommended to compress dictionary resources into zip archives and to use Zips sound packs instead of sound files in separate folders.
