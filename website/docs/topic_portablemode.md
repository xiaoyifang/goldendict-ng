By default, GoldenDict saves program configurations and indexes in the user's home directory.
To save them in a single portable location, you can to create a `portable` folder next to the main program file.
Once this folder exists, GoldenDict will launch in portable mode.

In portable mode, the configurations and indexes are stored in the `portable` folder.
All dictionaries must be placed in or inside the subfolders of `content` folder, which located besides the main program file.

Folder structure:
```
.
├── goldendict.exe
├── portable/
├── content/
└── ...
```
