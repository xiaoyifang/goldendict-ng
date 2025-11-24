# Favorites Write-Ahead Log (WAL) Format

## Overview

GoldenDict-ng uses a Write-Ahead Log (WAL) to ensure no favorites data is lost in case of application crashes or unexpected shutdowns. All favorites operations (add, remove, move) are immediately logged to a WAL file before being applied to the in-memory data structure.

## WAL File Location

The WAL file is located alongside the main favorites file:
- **Main favorites file**: `favorites.xml`
- **WAL file**: `favorites.xml.wal`

## File Format

The WAL uses **JSONL** (JSON Lines) format - one JSON object per line. Each line represents a single operation.

### Common Fields

All WAL entries contain these fields:

| Field | Type | Description |
|-------|------|-------------|
| `op` | string | Operation type: `"add"`, `"remove"`, or `"move"` |
| `ts` | integer | Unix timestamp (seconds since epoch) |

### Add Operation

Records the addition of a word or folder to favorites.

**Format:**
```json
{"op":"add","ts":1700000000,"path":["folder1","folder2","item"]}
```

**Fields:**
- `path` (array of strings): Full path from root to the item
  - For a word: `["English", "Grammar", "hello"]`
  - For a folder: `["English", "Grammar"]`

**Examples:**
```json
{"op":"add","ts":1732435200,"path":["English","hello"]}
{"op":"add","ts":1732435201,"path":["English","Grammar"]}
{"op":"add","ts":1732435202,"path":["English","Grammar","syntax"]}
```

### Remove Operation

Records the removal of a word or folder from favorites.

**Format:**
```json
{"op":"remove","ts":1700000001,"path":["folder1","folder2","item"]}
```

**Fields:**
- `path` (array of strings): Full path to the removed item

**Examples:**
```json
{"op":"remove","ts":1732435203,"path":["English","hello"]}
{"op":"remove","ts":1732435204,"path":["English","Grammar"]}
```

**Note:** When a folder is removed, only the folder itself is logged. Child items are not individually logged.

### Move Operation

Records moving a word from one location to another (typically via drag-and-drop).

**Format:**
```json
{"op":"move","ts":1700000002,"from":["folder1","word"],"to":["folder2","word"]}
```

**Fields:**
- `from` (array of strings): Original full path
- `to` (array of strings): Destination full path

**Examples:**
```json
{"op":"move","ts":1732435205,"from":["English","hello"],"to":["French","hello"]}
{"op":"move","ts":1732435206,"from":["Temp","word1"],"to":["English","Grammar","word1"]}
```

## WAL Lifecycle

### 1. Operation Logging

When a user performs an operation:
1. Operation is **immediately** logged to the WAL file
2. Operation is applied to the in-memory data structure
3. Main favorites file is marked as "dirty"

### 2. Compaction

Every **10 minutes** (fixed interval), the WAL is compacted:
1. In-memory data is saved to the main `favorites.xml` file
2. WAL file is **cleared** (deleted)
3. New operations start logging to a fresh WAL

Compaction also occurs when the application exits normally.

### 3. Recovery (Replay)

On application startup:
1. Main `favorites.xml` is loaded
2. If a WAL file exists and has content:
   - All operations are **replayed** in order
   - Operations are applied to the loaded data
   - Data is marked as dirty for next compaction
3. Normal operation continues

## Implementation Details

### Atomic Writes

WAL uses `QSaveFile` for atomic writes to prevent corruption from partial writes:
1. Write to temporary file
2. Atomic rename to target file
3. Ensures WAL is never in a corrupted state

### Duplicate Detection

During replay, the system checks if items already exist before adding them:
- Prevents duplicate entries
- Handles folders created automatically by `forceFolder`

### Folder Handling

Folders are treated specially:
- **Explicit folder creation** is logged to WAL
- **Implicit folder creation** (via `forceFolder` during word addition) is not logged
- During replay, `forceFolder` automatically recreates folder hierarchies

## Example WAL File

```jsonl
{"op":"add","ts":1732435200,"path":["English"]}
{"op":"add","ts":1732435201,"path":["English","hello"]}
{"op":"add","ts":1732435202,"path":["English","world"]}
{"op":"add","ts":1732435203,"path":["French"]}
{"op":"move","ts":1732435204,"from":["English","hello"],"to":["French","hello"]}
{"op":"remove","ts":1732435205,"path":["English","world"]}
```

**Interpretation:**
1. Create "English" folder
2. Add "hello" to "English"
3. Add "world" to "English"
4. Create "French" folder
5. Move "hello" from "English" to "French"
6. Remove "world" from "English"

## Error Handling

### Corrupted WAL Entries

- Invalid JSON lines are logged as warnings and skipped
- Replay continues with remaining valid entries
- Ensures maximum data recovery

### Partial Replay

If replay fails mid-way:
- Successfully replayed operations are kept
- Data is marked as dirty
- Next compaction will save the partial state
- WAL will be cleared on successful compaction

## Performance Considerations

### Write Performance

- WAL writes are **append-only** (fast)
- Each operation requires one file write
- Uses atomic writes for safety

### Compaction Overhead

- Occurs every 10 minutes (low frequency)
- Only saves if data is dirty
- WAL clear is a simple file deletion

### Recovery Performance

- Replay is sequential, O(n) where n = number of operations
- Typically fast (< 1 second for hundreds of operations)
- Only occurs on startup after crash

## Configuration

The WAL system has **no user-configurable settings**:
- Compaction interval: **10 minutes** (hardcoded)
- WAL file location: **automatic** (same as favorites file)
- Replay: **automatic** on startup

This design prioritizes data safety and simplicity over configurability.

## Related Files

- **Implementation**: `src/ui/favoriteswal.cc`, `src/ui/favoriteswal.hh`
- **Integration**: `src/ui/favoritespanewidget.cc`
- **Main favorites**: `favorites.xml` (in config directory)

## See Also

- [Favorites UI Documentation](ui_favorites.md)
- [Architecture Overview](architecture.md)
