Dictionaries management dialog can be opened via menu `Edit` -> `Dictionaries`.

To use local dictionaries, add them via `Sources` -> `Files`.

To inspect or disable individual dictionaries, go `Edit` -> `Dictionaries` -> `Dictionaries`.

If you have too many dictionaries, consider use `Groups` to manage them.

## Files

![Dict File Tab](img/dict_file_tab.webp)


Here you can add local dictionaries.

Press the "Add" button and select folders that includes your local dictionaries. To search every subfolders, enable the "Recursive".

GoldenDict will scan these folders and add found dictionaries into dictionaries list.

"Rescan" button start forced scan of all folders in list.


## Sound Dirs

Similar to Files, you can either add a folder which contains sound files or a `.zip` archive which contains the sound files.

Goldendict will search through the sound file names when querying words.

## Morphology

Here you can turn on/off morphology dictionaries. 

You can specify a path that includes Hunspell format data files (.aff + .dic).

GoldenDict scan this folder and create list of available dictionaries. To turn on dictionary just set mark in corresponding column.

## Websites

Here you can add any website which allow to set target word in url. To add such site you should set it url with target word template, name for dictionaries list and set mark in "Enabled" column. In the "Icon" column you can set custom icon for this site. If you add icon file name without path GoldenDict will search this file in configuration folder. "As link" column define method of article insertion into common page. If this option is set article will be inserted as link inside `<iframe>` tag (preferable mode). If articles are not loaded in this mode turn this option off, then articles will be inserted as html-code.

Target word can be inserted into url in next encodings::

| Target word template   | Encoding                                |
|------------------------|-----------------------------------------|
| %GDWORD%               | UTF-8                                   |
| %GD1251%               | Windows-1251                            |
| %GDISO1% ... %GDISO16% | ISO 8859-1 ... ISO 8859-16 respectively |
| %GDBIG5%               | Big-5                                   |
| %GDBIG5HKSCS%          | Big5-HKSCS                              |
| %GDGBK%                | GBK and GB 18030                        |
| %GDSHIFTJIS%           | Shift-JIS                               |

## DICT servers

Here you can add servers which uses DICT protocol. To add such server you should set its url, name for dictionaries list, server bases list, search strategies list and set mark in "Enabled" column. If bases list is empty GoldenDict will use all server bases. If search strategies list is empty GoldenDict will use "prefix" strategy (comparing the first part of the word).

In the "Icon" column you can set custom icon for every server. If you add icon file name without path GoldenDict will search this file in configuration folder.

## Programs

Here you can add external applications. To add such application you should set command line for its launch, name for dictionaries list and application type. The `%GDWORD%` template in command line will be replaced by word from search line. If command line don't contains such template the word will be fed into standard input stream in 8-bit current locale encoding.

| Application type | The purpose of application |
|---|---|
| Audio | Application play sound. |
| Text | Application output some plain text in 8-bit current locale encoding into standard output stream. This text will be shown as separate article. |
| Html | Application output some html code into standard output stream. This code will be shown as separate article. |
| Prefix | Application output some word list into standard output stream. This list will be added in common matches list. |

In the "Icon" column you can set custom icon for every application. If you add icon file name without path GoldenDict will search this file in configuration folder.

## Transliteration

Here you can add transliteration algorithms. To add algorithm into dictionaries list just set mark beside it. When such dictionary added into current dictionaries group GoldenDict will search word in the input line as well as result of its handling by corresponding transliteration algorithm.
