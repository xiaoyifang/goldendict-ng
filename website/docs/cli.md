## Command Line Interface

GoldenDict-ng supports various command line arguments to control its behavior at startup.

### Basic Usage

```bash
goldendict [options] [word]
```

### Options

| Option | Short | Description |
|--------|-------|-------------|
| `--help` | `-h` | Show help message and exit |
| `--version` | `-v` | Print version and diagnosis info |
| `--log-to-file` | `-l` | Save debug messages to `gd_log.txt` in the config folder |
| `--reset-window-state` | `-r` | Reset window state to defaults |
| `--no-tts` | | Disable Text-to-Speech |
| `--group-name <name>` | `-g` | Change the dictionary group for main window |
| `--popup-group-name <name>` | `-p` | Change the dictionary group for popup window |
| `--scanpopup` | `-s` | Force word translation in popup window |
| `--popup` | | Alias for `--scanpopup` |
| `--main-window` | `-m` | Force word translation in main window |
| `--toggle-popup` | `-t` | Toggle the scan popup window |

### Positional Arguments

| Argument | Description |
|----------|-------------|
| `word` | Word or sentence to query at startup |

### Examples

#### Query a word in main window

```bash
goldendict hello
```

#### Query a word in popup window

```bash
goldendict --popup hello
goldendict -s hello
```

#### Switch dictionary group

```bash
goldendict -g "English Dictionaries"
goldendict --group-name "Chinese Dictionaries"
```

#### Reset window state

```bash
goldendict --reset-window-state
```

#### Enable debug logging

```bash
goldendict --log-to-file
```

#### Toggle popup window

```bash
goldendict --toggle-popup
```

### URL Scheme

GoldenDict-ng also supports URL scheme for integration with other applications:

```
goldendict://<word>?target=<window>
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| `word` | URL-encoded word | The word to translate |
| `target` | `popup` or `main` | Target window (defaults to `main`) |

**Example:**

```
goldendict://hello?target=popup
```

### Environment Variables

| Variable | Platform | Description |
|----------|----------|-------------|
| `GOLDENDICT_FORCE_XCB` | Linux | Force using X11 (Xcb) platform |
| `GOLDENDICT_FORCE_WAYLAND` | Linux | Force using native Wayland platform |

### Notes

- If another instance of GoldenDict-ng is already running, command line arguments will be forwarded to the running instance via IPC.
- When both `--popup` and `--main-window` are specified, the last one takes precedence.
- The `--no-tts` option clears all voice engines from the configuration.