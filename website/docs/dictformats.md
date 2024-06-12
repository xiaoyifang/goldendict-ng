Popular dictionary formats are all supported.

## Local Dictionaries Sources

* [MDict](https://www.mdict.cn/) dictionaries (.mdx/.mdd)
* [StarDict](http://www.huzheng.org/stardict/) dictionaries (.ifo/.dict./.idx/.syn)
* [DSL](https://lingvoboard.ru/store/html/DSLReference_HTML/index.html) dictionaries (ABBYY Lingvo source files .dsl(.dz))
* [Xdxf](https://github.com/soshial/xdxf_makedict) dictionaries (.xdxf(.dz))
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
* Any sites which allow set target word in address line

## Other Sources

Various special "dictionaries" can be added, such as Programs, TTS, Morphology, Transliteration, etc... Their doc located at [Sources Management](manage_sources.md)

## Additional info

### Converting between formats

Goldendict does not provide any dictionary modification functionality.

To convert between formats, try tools like [pyglossary](https://github.com/ilius/pyglossary).

### Individual Dictionary Icons

Every local dictionary can have individual icon. BMP, PNG, JPG or ICO files can be used for this icon.

For Babylon, StarDict, DictD, ABBYY Lingvo, AardDictionary, SDictionary, Zim, MDict, Lsa, Zips, Slob, Gls dictionaries such graphics file must be named by main dictionary file name and places beside one. That is if main file of your dictionary, for example, named "My_best_dictionary.dsl" therefore icon file must be named "My_best_dictionary.bmp" (.png, .jpg etc.).

For Xdxf dictionaries icon file must be named "icon16.png" (for 16х16 images) or "icon32.png" (for 32х32 images) and placed into dictionary folder.

For Epwing dictionaries icon file must be named by name of folder with dictionary data beside "catalogs" file (a few folders can be presented, every folder is separate dictionary) and placed beside "catalogs" file.

If individual icon is not presented the default icon for this type of dictionaries will be used.

### Stardict

Main file of Stardict dictionary (.dict) can be compressed by Dictzip program to reduce its size.

Additional dictionary resources (images, style sheets, etc.) placed in "res" folder also can be compressed into zip archive. This archive must be named "res.zip" and placed beside other dictionary files or inside "res" folder.

### ABBYY Lingvo (.dsl)

Main file of ABBYY Lingvo dictionary (.dsl) can be compressed by Dictzip program to reduce its size.

Additional dictionary resources (images, sound files, etc.) also can be compressed into zip archive. This archive must be named on main dictionary file name (include extension) with adding ".files.zip" and placed beside other dictionary files. If main file of your dictionary, for example, named "My_best_dictionary.dsl" therefore archive with resources must be named "My_best_dictionary.dsl.files.zip".

GoldenDict supports the "#SOUND_DICTIONARY" directive. Sounds missing in the resources of the dictionary will be searched first in the dictionary specified in this directive.

### DictD, Xdxf

Main file of DictD dictionary (.dict) or Xdxf dictionary (.xdxf) can be compressed by Dictzip program to reduce its size.

### Slob

GoldenDict can render formulas in TeX format with the built-in web engine.

### GLS

Main file of Babylon source dictionary must be in UTF-8 (or UTF-16 with BOM) encoding. It can be compressed by Dictzip program to reduce its size. Golden Dict read from dictionary header "### Glossary title:", "### Author:", "### Description:", "### Source language:" and "### Target language:" fields only. Dictionary header must be concluded with "### Glossary section:" mark.

### ABBYY Lingvo sound archives

ABBYY Lingvo sound archives are set of sound files packed into one file of some specific format. These files can be called directly from matches list or from dictionary articles.

### Sound files in separate folders

The separate folder with sound files can be added into GoldenDict dictionaries list. GoldenDict will handle this folders like ABBYY Lingvo sound archives (inapplicable in portable mode).

### Zips sound packs

Zips sound pack is zip archive with extension ".zips" contains set of sound files. To create suck pack it needs to compress sound files into zip archive and change extension of this archive to ".zips". GoldenDict will handle such sound packs like ABBYY Lingvo sound archives.

### General notes

At every launch GoldenDict scan folders with dictionaries to create dictionaries list. The more files in these folder is the more time for scanning. Therefore it is recommended to compress dictionary resources into zip archives and to use Zips sound packs instead of sound files in separate folders.