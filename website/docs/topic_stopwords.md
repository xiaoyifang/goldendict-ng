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
- **Single-character stopwords**: Included, but effectiveness is uncertain
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
    GoldenDict uses Xapian's CJK N-GRAM mode for Chinese/Japanese/Korean text indexing, which splits CJK text into **2-character bigrams** before applying stopword filtering. This affects how stopwords work:

**How It Works:**

1. Text is split into 2-character bigrams (e.g., "因为所以" → "因为", "为所", "所以")
2. Stopper checks each generated bigram against the stopword list
3. Matching bigrams are filtered out

**Important Note on Single Characters:**

Xapian's `FLAG_CJK_NGRAM` primarily generates **bigrams only**. Single CJK characters are typically NOT indexed as independent terms unless they appear isolated (without adjacent CJK characters). This behavior is similar to Lucene's CJK bigram filter with `output_unigrams=false`.

**For CJK Text:**

- ✅ **2-character stopwords work correctly** (e.g., "一个", "这个", "那个", "因为", "所以")
  - These exactly match the generated bigrams
- ⚠️ **Single-character stopwords effectiveness is unclear**
  - May only work for isolated characters (e.g., single punctuation marks)
  - Most CJK single characters within text are NOT indexed independently
  - Testing shows mixed results - depends on Xapian version and context
- ❌ **Multi-character stopwords (>2) do NOT work** (e.g., "因为所以" with 4 characters)
  - Split into multiple bigrams ("因为", "为所", "所以")
  - The original 4-character term never exists as a single unit
  - Stopper cannot match the complete phrase

**For Non-CJK Text:**

- ✅ **English stopwords work normally** (tokenized by spaces/punctuation, not affected by N-GRAM)
- ✅ **Punctuation stopwords work normally**

**Best Practices:**

For effective CJK stopword filtering:
- ✅ Use 2-character word pairs that commonly appear in your dictionaries
- ✅ Focus on frequently occurring 2-character combinations
- ⚠️ Single characters: **Effectiveness uncertain** - may only work for isolated characters
- ❌ Avoid phrases longer than 2 characters (will be split)

**Example:**

```text
# ✅ Effective: 2-character Chinese stopwords (guaranteed to work)
一个  # Will filter the bigram "一个"
这个  # Will filter the bigram "这个"
因为  # Will filter the bigram "因为"
所以  # Will filter the bigram "所以"

# ⚠️ Uncertain: Single characters (included in built-in list)
的    # Effectiveness unclear - may work for isolated characters
了    # May only work if appears as standalone character
# Note: These are included in the built-in stopwords.txt
# You can remove them with: -的  -了  (in your custom config)

# ❌ Ineffective: Multi-character phrases
因为所以  # Splits into "因为", "为所", "所以" - cannot match as a whole
```

**Technical Note:**

If you want to filter "因为所以" as a phrase, you would need to add both component bigrams:
```text
因为
所以
```
This will remove both bigrams individually, though "为所" would still be indexed.

### Performance Impact

- **Smaller index**: Removing common words reduces index size
- **Faster indexing**: Fewer words to process
- **Better relevance**: Search results focus on meaningful terms
- **Language mixing**: Works well for multilingual dictionaries

## See Also

- [Full-Text Search](ui_fulltextsearch.md) - Using full-text search feature
- [Dictionary Formats](dictformats.md) - Supported dictionary formats
- [User Styles](topic_userstyle.md) - Customizing article appearance
