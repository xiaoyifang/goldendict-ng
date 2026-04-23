# Customizing Stopwords for Full-Text Search

Stopwords are common words (like "the", "is", "at") that are excluded from full-text search indexing to improve search quality and reduce index size.

GoldenDict-NG comes with built-in stopwords for multiple languages (English, Chinese, Japanese, Korean), but you can customize them according to your needs.

## Configuration File Location

Create a file named `stopwords.txt` in your GoldenDict configuration directory:

| Platform | Location |
|----------|----------|
| **Windows** | `%APPDATA%\GoldenDict\stopwords.txt` |
| **Linux/Unix (legacy)** | `~/.goldendict/stopwords.txt` |
| **Linux/Unix (XDG)** | `~/.config/goldendict/stopwords.txt` |
| **macOS** | `~/.goldendict/stopwords.txt` |
| **Portable** | `<program_directory>/portable/stopwords.txt` |

!!! tip "Finding Your Config Directory"
    You can open your configuration directory from GoldenDict menu: **Edit → Preferences → Advanced → Open Config Folder**

## File Syntax

The `stopwords.txt` file uses a simple text format:

- **Lines starting with `#`** are comments (ignored)
- **Empty lines** are ignored
- **Regular words** (one per line) are ADDED to the stopwords list
- **Words prefixed with `-`** (minus sign) are REMOVED from built-in stopwords

### Example Configuration

```text
# My custom stopwords configuration

# Add domain-specific stopwords
example
demo
test
tutorial

# Remove built-in stopwords I want to search
-the
-is
-at

# Add technical terms that appear too frequently
api
sdk
framework
```

## Use Cases

### Adding Custom Stopwords

If you're working with specialized dictionaries (medical, technical, etc.), you may want to exclude domain-specific common words:

```text
# Medical terminology stopwords
patient
symptom
treatment
diagnosis
```

### Removing Built-in Stopwords

Sometimes you may want to search for words that are normally stopwords. For example, searching for "the" in grammar dictionaries:

```text
# I want to search these words
-the
-a
-an
-be
```

### Mixed Configuration

You can combine both additions and removals:

```text
# Remove some built-in English stopwords
-not
-but

# Add Chinese stopwords not in built-in list
例如
比如
所谓

# Add Japanese stopwords
例えば
つまり
```

## Applying Changes

After modifying `stopwords.txt`, you need to rebuild your full-text search index:

1. Go to **Edit → Preferences → Full-text search**
2. Select dictionaries you want to re-index
3. Click **Remove** to delete old index
4. Restart GoldenDict to rebuild index automatically

!!! warning "Index Rebuild Required"
    Changes to stopwords only affect NEW indexes. You must rebuild existing indexes for changes to take effect.

## Built-in Stopwords

GoldenDict-NG includes stopwords for multiple languages:

**English**: Common words like "the", "is", "and", "or", "not", etc. (49 words)

**CJK Languages** (Chinese, Japanese, Korean):
- **Single-character stopwords**: Included
  - May work for isolated characters
  - Included for potential benefit in some contexts
  - Users can remove them if they cause issues (use `-的` syntax in custom config)
- **2-character stopwords**: Guaranteed to work reliably (recommended)

!!! note "CJK Stopwords Strategy"
    The built-in list includes both single-character and 2-character CJK stopwords. While only 2-character stopwords are guaranteed to work due to N-GRAM indexing, single-character stopwords are also included as they may provide some benefit in certain contexts (e.g., isolated punctuation or standalone characters). You can selectively remove single-character stopwords using the `-word` syntax in your custom configuration if needed.

To see the complete list, check: `src/data/stopwords.txt` in the source code.

## Technical Details

### How It Works

1. GoldenDict loads built-in stopwords from internal resources
2. Then loads your custom `stopwords.txt` from config directory
3. Adds new words and removes words marked with `-`
4. Uses the final list when indexing dictionaries

### CJK N-GRAM Indexing and Stopwords

!!! warning "Important: CJK Stopwords Limitation"
    GoldenDict uses Xapian's N-GRAM mode for Chinese/Japanese/Korean text indexing, which splits CJK text into **2-character bigrams** before applying stopword filtering. This affects how stopwords work:

**Basic Principle:**
- Text is split into 2-character bigrams (e.g., "因为所以" → "因为", "为所", "所以")
- Stopword checker compares each bigram against the stopword list
- Matching bigrams are filtered out

**For Non-CJK Text:**
- ✅ **English stopwords work normally** (tokenized by spaces/punctuation)
- ✅ **Punctuation stopwords work normally**

**Best Practices:**
- Use common 2-character word pairs from your dictionaries
- Focus on frequently occurring 2-character combinations
- Avoid phrases longer than 2 characters

### Performance Impact

- **Smaller index**: Reduces index size by removing common words
- **Faster indexing**: Fewer words to process
- **Better relevance**: Search results focus on meaningful terms
- **Language mixing**: Works well for multilingual dictionaries

## See Also

- [Full-Text Search](ui_fulltextsearch.md) - Using full-text search feature
- [Dictionary Formats](dictformats.md) - Supported dictionary formats
- [User Styles](topic_userstyle.md) - Customizing article appearance
