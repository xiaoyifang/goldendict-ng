# Sound Dir Index: Chunk Record Format

This document describes the on-disk format of the index produced for a sound directory
(a directory added under **Sound Dirs**), with a focus on the records kept in the
chunked storage. It is meant for developers who need to read, extend or debug that
format.

Relevant code: `src/dict/sounddir.cc` (reader/writer, index building),
`src/dict/chunkedstorage.cc` (chunked storage), `src/dict/btreeidx.cc` (word index).

## 1. Index File Layout

A sound directory is turned into a B-tree dictionary. The index file starts with a
fixed header:

| Field | Type | Meaning |
| :--- | :--- | :--- |
| `signature` | `uint32` | `0x58524453` (`SDRX` when written on a little-endian machine) |
| `formatVersion` | `uint32` | `CurrentFormatVersion` (`5 + BtreeIndexing::FormatVersion + Folding::Version`) |
| `soundsCount` | `uint32` | Number of indexed sounds, for informational purposes |
| `chunksOffset` | `uint32` | Offset of the chunked storage |
| `dirTimestampsOffset` | `uint32` | Offset of the directory timestamps map |
| `indexBtreeMaxElements` | `uint32` | Part of `BtreeIndexing::IndexInfo` |
| `indexRootOffset` | `uint32` | Root node offset of the word B-tree |

The word B-tree maps a **folded word** to the **address** of its record. For a plain
sound file the word is the file name without its extension; for an entry of a Lingvo
sound archive (`.lsa`) it is the entry name (also without extension). Several records
may share the same word (for example `hello.wav` and an `.lsa` entry named `hello`);
`getArticle()` then offers all of them.

## 2. Chunked Storage Mechanics

Records are appended through `ChunkedStorage::Writer`:

- `startNewBlock()` marks the beginning of a record and returns its **address**.
- `addToBlock( data, size )` appends the record bytes.
- Blocks are accumulated in a buffer; when the buffer is at least `ChunkMaxSize`
  (65536) bytes at the start of a new block, the buffer is compressed with zlib and
  flushed as one **chunk**.

Address encoding (see `Writer::startNewBlock`):

```text
bits  0..15 : offset of the record inside its chunk
bits 16..31 : index of the chunk
```

Consequences:

- At most 65536 chunks can exist.
- A chunk is never larger than `65536 + (size of the last block)`; a new chunk only
  starts on a block boundary, therefore **a record is never split between chunks**.
- A record must fit comfortably into a chunk, i.e. it must be far smaller than 64 KiB.

On disk each chunk is stored as:

```text
[ uint32 uncompressedSize ][ uint32 compressedSize ][ compressedSize bytes of zlib data ]
```

The chunk offsets table at the end of the chunked storage holds the file offset of
every chunk. `Reader::getBlock( address, chunk )` decompresses the whole chunk into
the caller-provided `std::vector<char>` and returns a pointer to the requested record
inside that local copy. Several records therefore share one decompressed buffer, and
the pointer returned is only valid while that vector lives.

## 3. Record Format

All records share one unified binary layout:

```text
[ uint32 type ][ uint32 payloadSize ][ payload ]
```

`payloadSize` counts the bytes of the payload only. The payload is a sequence of
fields whose order and count are defined by `type`:

| `type` | Name | Payload fields |
| :--- | :--- | :--- |
| `0` | `SoundFile` | `str path` |
| `1` | `LsaEntry` | `u32 vorbisOffset`, `u32 entryOffset`, `str name`, `str path` |

Field encodings:

| Field | Encoding |
| :--- | :--- |
| `u32` | 4 bytes, host byte order |
| `str` | `[ uint32 size ][ size bytes of UTF-8 ]` |

Because strings are length-prefixed, no terminator is needed, embedded NUL characters
are harmless, and a record is fully delimited by its own header. Readers never look at
the bytes of the following record.

### 3.1 `SoundFile` record

| Field | Meaning |
| :--- | :--- |
| `path` | Path of the sound file relative to the sound directory, e.g. `sub/hello.wav` |

Byte example for `sub/hello.wav`:

```text
00 00 00 00  11 00 00 00  0D 00 00 00  73 75 62 2F 68 65 6C 6C 6F 2E 77 61 76
type=0       payload=17   len=13       "sub/hello.wav"
```

### 3.2 `LsaEntry` record

| Field | Meaning |
| :--- | :--- |
| `vorbisOffset` | Offset of the Ogg Vorbis stream inside the `.lsa` archive (shared by all its entries) |
| `entryOffset` | Offset of this entry's record inside the `.lsa` archive |
| `name` | Entry name with the extension stripped, e.g. `hello` |
| `path` | Path of the `.lsa` archive relative to the sound directory, e.g. `dict.lsa` |

Byte example for entry `你好` (6 UTF-8 bytes) with `vorbisOffset = 5000`,
`entryOffset = 220` and archive `dict.lsa`:

```text
01 00 00 00  1E 00 00 00                      type=1, payload=30
88 13 00 00  DC 00 00 00                      vorbis=5000, entry=220
06 00 00 00  E4 BD A0 E5 A5 BD                len=6, "你好"
08 00 00 00  64 69 63 74 2E 6C 73 61          len=8, "dict.lsa"
```

At playback time `Lsa::loadLsaSound()` seeks to `entryOffset` inside the archive,
re-parses the entry record to obtain its sample range, and decodes the requested
samples from the Ogg stream that starts at `vorbisOffset`.

## 4. Reading Path

| Function | Purpose | Fields used |
| :--- | :--- | :--- |
| `SoundDirDictionary::get_file_name()` | Returns the display name and the suffix of the `gdau://` URL: the relative path for `SoundFile`, the entry name for `LsaEntry` | `path` / `name` |
| `SoundDirDictionary::getArticle()` | Builds the article page listing all sounds of a word; queries `get_file_name()` for the records it lists | via `get_file_name()` |
| `SoundDirDictionary::getResource()` | Serves the actual audio bytes | all |

`getResource()` accepts two forms of resource names:

- `<address>` or `<address>/<name>` — the internal form used by the article page
  (`address` is the record address, `<name>` is informative only).
- A plain file or entry name, e.g. `hello.wav` or `hello`. This form is used when
  another dictionary requests a shared sound (a DSL fallback or the
  `#SOUND_DICTIONARY` directive); the name is resolved through the word B-tree.

Both forms end up reading one record via `getBlock()`, parsing the header with
`openRecord()` and then the payload with `RecordReader`.

## 5. Writing Path

| Function | Record type | Indexed word |
| :--- | :--- | :--- |
| `addDir()` | `SoundFile` | File name without extension |
| `addLsaArchives()` | `LsaEntry` | Entry name returned by `Lsa::parseLsaEntries()` (extension already stripped) |

`addLsaArchives()` walks the sound directory recursively, parses every `.lsa` archive
found there, and indexes its entries. Only *universal* archives (entries named after
real words) are usable this way; archives naming their sounds with internal ids belong
beside a specific DSL dictionary and must not be placed in a sound directory.

## 6. Invariants

- The header and the payload are written and read in **host byte order**, like the
  rest of the index file. A foreign-endianness index fails the `signature` check and is
  rebuilt, so no cross-platform index reuse is possible.
- `payloadSize` must not exceed the remaining bytes of the record's chunk; a truncated
  record makes `openRecord()` fail, and such a record is treated as "no such resource".
- A record address is `uint32` and encodes at most 65536 chunks with a 64 KiB offset
  inside each.
- `str` sizes are `uint32`, so field sizes are effectively unbounded for realistic
  paths and entry names.
- Words are indexed folded (`Text::toUtf32` + the B-tree folding), so lookups are
  case- and diacritics-insensitive according to the user settings.

## 7. Versioning

`CurrentFormatVersion` is part of the index header. When the record format changes,
the constant is incremented and `indexIsOldOrBad()` rejects the old index, forcing a
rebuild on the next start. The format described here is version `5` of the sound dir
record layout.

## 8. Historical Note

Earlier versions stored records as NUL-separated text fields (one field for a plain
file, five fields with an `"lsa"` marker for an archive entry). Record boundaries had
to be inferred from a convention rather than from the data, and a reader that started
at the wrong offset silently decoded a neighbouring entry, which produced wrong or
truncated audio. The unified header + `payloadSize` layout described above removes
that class of bugs: a record is delimited by its own header and can be parsed without
any knowledge of the surrounding records.
