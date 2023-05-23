/*
 * Copyright (C) 2020-2021 Matthieu Gautier <mgautier@kymeria.fr>
 * Copyright (C) 2021 Maneesh P M <manu.pm55@gmail.com>
 * Copyright (C) 2020 Veloman Yunkan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * is provided AS IS, WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, and
 * NON-INFRINGEMENT.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifndef ZIM_ARCHIVE_H
#define ZIM_ARCHIVE_H

#include "zim.h"
#include "entry.h"
#include "uuid.h"

#include <string>
#include <vector>
#include <memory>
#include <bitset>
#include <set>

namespace zim
{
  class FileImpl;

  enum class EntryOrder {
    pathOrder,
    titleOrder,
    efficientOrder
  };

  /**
   * The Archive class to access content in a zim file.
   *
   * The `Archive` is the main class to access content in a zim file.
   * `Archive` are lightweight object and can be copied easily.
   *
   * An `Archive` is read-only, and internal states (as caches) are protected
   * from race-condition. Therefore, all methods of `Archive` are threadsafe.
   *
   * All methods of archive may throw an `ZimFileFormatError` if the file is invalid.
   */
  class LIBZIM_API Archive
  {
    public:
      template<EntryOrder order> class EntryRange;
      template<EntryOrder order> class iterator;

      /** Archive constructor.
       *
       *  Construct an archive from a filename.
       *  The file is open readonly.
       *
       *  The filename is the "logical" path.
       *  So if you want to open a split zim file (foo.zimaa, foo.zimab, ...)
       *  you must pass the `foo.zim` path.
       *
       *  @param fname The filename to the file to open (utf8 encoded)
       */
      explicit Archive(const std::string& fname);

#ifndef _WIN32
      /** Archive constructor.
       *
       *  Construct an archive from a file descriptor.
       *
       *  Note: This function is not available under Windows.
       *
       *  @param fd The descriptor of a seekable file representing a ZIM archive
       */
      explicit Archive(int fd);

      /** Archive constructor.
       *
       *  Construct an archive from a descriptor of a file with an embedded ZIM
       *  archive inside.
       *
       *  Note: This function is not available under Windows.
       *
       *  @param fd The descriptor of a seekable file with a continuous segment
       *  representing a complete ZIM archive.
       *  @param offset The offset of the ZIM archive relative to the beginning
       *  of the file (rather than the current position associated with fd).
       *  @param size The size of the ZIM archive.
       */
      Archive(int fd, offset_type offset, size_type size);
#endif

      /** Return the filename of the zim file.
       *
       *  Return the filename as passed to the constructor
       *  (So foo.zim).
       *
       *  @return The logical filename of the archive.
       */
      const std::string& getFilename() const;

      /** Return the logical archive size.
       *
       *  Return the size of the full archive, not the size of the file on the fs.
       *  If the zim is split, return the sum of the size of the parts.
       *
       *  @return The logical size of the archive.
       */
      size_type getFilesize() const;

      /** Return the number of entries in the archive.
       *
       * Return the total number of entries in the archive, including
       * internal entries created by libzim itself, metadata, indexes, ...
       *
       *  @return the number of all entries in the archive.
       */
      entry_index_type getAllEntryCount() const;

      /** Return the number of user entries in the archive.
       *
       * If the notion of "user entries" doesn't exist in the zim archive,
       * returns `getAllEntryCount()`.
       *
       *  @return the number of user entries in the archive.
       */
      entry_index_type getEntryCount() const;

      /** Return the number of articles in the archive.
       *
       *  The definition of "article" depends of the zim archive.
       *  On recent archives, this correspond to all entries marked as "FRONT_ARTICLE"
       *  at creaton time.
       *  On old archives, this corresponds to all "text/html*" entries.
       *
       *  @return the number of articles in the archive.
       */
      entry_index_type getArticleCount() const;

      /** Return the number of media in the archive.
       *
       * This definition of "media" is based on the mimetype.
       *
       * @return the number of media in the archive.
       */
      entry_index_type getMediaCount() const;

      /** The uuid of the archive.
       *
       *  @return the uuid of the archive.
       */
      Uuid getUuid() const;

      /** Get a specific metadata content.
       *
       *  Get the content of a metadata stored in the archive.
       *
       *  @param name The name of the metadata.
       *  @return The content of the metadata.
       *  @exception EntryNotFound If the metadata is not in the arcthive.
       */
      std::string getMetadata(const std::string& name) const;

      /** Get a specific metadata item.
       *
       *  Get the item associated to a metadata stored in the archive.
       *
       *  @param name The name of the metadata.
       *  @return The item associated to the metadata.
       *  @exception EntryNotFound If the metadata in not in the archive.
       */
      Item getMetadataItem(const std::string& name) const;

      /** Get the list of metadata stored in the archive.
       *
       *  @return The list of metadata in the archive.
       */
      std::vector<std::string> getMetadataKeys() const;

      /** Get the illustration item of the archive.
       *
       *  Illustration is a icon for the archive that can be used in catalog and so to illustrate the archive.
       *
       *  @param size The size (width and height) of the illustration to get. Default to 48 (48x48px icon)
       *  @return The illustration item.
       *  @exception EntryNotFound If no illustration item can be found.
       */
      Item getIllustrationItem(unsigned int size=48) const;

      /** Return a list of available sizes (width) for the illustations in the archive.
       *
       * Illustration is an icon for the archive that can be used in catalog and elsewehere to illustrate the archive.
       * An Archive may contains several illustrations with different size.
       * This method allows to know which illustration are in the archive (by size: width)
       *
       * @return A set of size.
       */
      std::set<unsigned int> getIllustrationSizes() const;


      /** Get an entry using its "path" index.
       *
       *  Use the index of the entry to get the idx'th entry
       *  (entry being sorted by path).
       *
       *  @param idx The index of the entry.
       *  @return The Entry.
       *  @exception std::out_of_range If idx is greater than the number of entry.
       */
      Entry getEntryByPath(entry_index_type idx) const;

      /** Get an entry using a path.
       *
       *  Get an entry using its path.
       *  The path must contains the namespace.
       *
       *  @param path The entry's path.
       *  @return The Entry.
       *  @exception EntryNotFound If no entry has the asked path.
       */
      Entry getEntryByPath(const std::string& path) const;

      /** Get an entry using its "title" index.
       *
       *  Use the index of the entry to get the idx'th entry
       *  (entry being sorted by title).
       *
       *  @param idx The index of the entry.
       *  @return The Entry.
       *  @exception std::out_of_range If idx is greater than the number of entry.
       */
      Entry getEntryByTitle(entry_index_type idx) const;

      /** Get an entry using a title.
       *
       *  Get an entry using its path.
       *
       *  @param title The entry's title.
       *  @return The Entry.
       *  @exception EntryNotFound If no entry has the asked title.
       */
      Entry getEntryByTitle(const std::string& title) const;

      /** Get an entry using its "cluster" index.
       *
       *  Use the index of the entry to get the idx'th entry
       *  The actual order of the entry is not really specified.
       *  It is infered from the internal way the entry are stored.
       *
       *  This method is probably not relevent and is provided for completeness.
       *  You should probably use a iterator using the `efficientOrder`.
       *
       *  @param idx The index of the entry.
       *  @return The Entry.
       *  @exception std::out_of_range If idx is greater than the number of entry.
       */
      Entry getEntryByClusterOrder(entry_index_type idx) const;

      /** Get the main entry of the archive.
       *
       *  @return The Main entry.
       *  @exception EntryNotFound If no main entry has been specified in the archive.
       */
      Entry getMainEntry() const;

      /** Get a random entry.
       *
       * The entry is picked randomly from the front artice list.
       *
       * @return A random entry.
       * @exception EntryNotFound If no valid random entry can be found.
       */
      Entry getRandomEntry() const;

      /** Check in an entry has path in the archive.
       *
       *  @param path The entry's path.
       *  @return True if the path in the archive, false else.
       */
      bool hasEntryByPath(const std::string& path) const {
        try{
          getEntryByPath(path);
          return true;
        } catch(...) { return false; }
      }

      /** Check in an entry has title in the archive.
       *
       *  @param title The entry's title.
       *  @return True if the title in the archive, false else.
       */
      bool hasEntryByTitle(const std::string& title) const {
        try{
          getEntryByTitle(title);
          return true;
        } catch(...) { return false; }
      }

      /** Check if archive has a main entry
       *
       * @return True if the archive has a main entry.
       */
      bool hasMainEntry() const;

      /** Check if archive has a favicon entry
       *
       * @param size The size (width and height) of the illustration to check. Default to 48 (48x48px icon)
       * @return True if the archive has a corresponding illustration entry.
       *         (Always True if the archive has no illustration, but a favicon)
       */
      bool hasIllustration(unsigned int size=48) const;

      /** Check if the archive has a fulltext index.
       *
       * @return True if the archive has a fulltext index
       */
      bool hasFulltextIndex() const;

      /** Check if the archive has a title index.
       *
       * @return True if the archive has a title index
       */
      bool hasTitleIndex() const;


      /** Get a "iterable" by path order.
       *
       *  This method allow to iterate on all user entries using a path order.
       *  If the notion of "user entries" doesn't exists (for old zim archive),
       *  this iterate on all entries in the zim file.
       *
       *  ```
       *  for(auto& entry:archive.iterByPath()) {
       *     ...
       *  }
       *  ```
       *
       *  @return A range on all the entries, in path order.
       */
      EntryRange<EntryOrder::pathOrder> iterByPath() const;

      /** Get a "iterable" by title order.
       *
       *  This method allow to iterate on all articles using a title order.
       *  The definition of "article" depends of the zim archive.
       *  On recent archives, this correspond to all entries marked as "FRONT_ARTICLE"
       *  at creaton time.
       *  On old archives, this correspond to all entries in 'A' namespace.
       *  Few archives may have been created without namespace but also without specific
       *  article listing. In this case, this iterate on all user entries.
       *
       *  ```
       *  for(auto& entry:archive.iterByTitle()) {
       *     ...
       *  }
       *  ```
       *
       *  @return A range on all the entries, in title order.
       */
      EntryRange<EntryOrder::titleOrder> iterByTitle() const;

      /** Get a "iterable" by a efficient order.
       *
       *  This method allow to iterate on all user entries using a effictient order.
       *  If the notion of "user entries" doesn't exists (for old zim archive),
       *  this iterate on all entries in the zim file.
       *
       *  ```
       *  for(auto& entry:archive.iterEfficient()) {
       *     ...
       *  }
       *  ```
       *
       *  @return A range on all the entries, in efficitent order.
       */
      EntryRange<EntryOrder::efficientOrder> iterEfficient() const;

      /** Find a range of entries starting with path.
       *
       * The path is the "long path". (Ie, with the namespace)
       *
       * @param path The path prefix to search for.
       * @return A range starting from the first entry starting with path
       *         and ending past the last entry.
       *         If no entry starts with `path`, begin == end.
       */
      EntryRange<EntryOrder::pathOrder>  findByPath(std::string path) const;

      /** Find a range of entry starting with title.
       *
       * The entry title is search in `A` namespace.
       *
       * @param title The title prefix to search for.
       * @return A range starting from the first entry starting with title
       *         and ending past the last entry.
       *         If no entry starts with `title`, begin == end.
       */
      EntryRange<EntryOrder::titleOrder> findByTitle(std::string title) const;

      /** hasChecksum.
       *
       * The checksum is not the checksum of the file.
       * It is an internal checksum stored in the zim file.
       *
       * @return True if the archive has a checksum.
       */
      bool hasChecksum() const;

      /** getChecksum.
       *
       * @return the checksum stored in the archive.
       *         If the archive has no checksum return an empty string.
       */
      std::string getChecksum() const;

      /** Check that the zim file is valid (in regard to its checksum).
       *
       *  If the zim file has no checksum return false.
       *
       *  @return True if the file is valid.
       */
      bool check() const;

      /** Check the integrity of the zim file.
       *
       * Run different type of checks to verify the zim file is valid
       * (in regard to the zim format).
       * This may be time consuming.
       *
       * @return True if the file is valid.
       */
      bool checkIntegrity(IntegrityCheck checkType);

      /** Check if the file is split in the filesystem.
       *
       *  @return True if the archive is split in different file (foo.zimaa, foo.zimbb).
       */
      bool isMultiPart() const;

      /** Get if the zim archive uses the new namespace scheme.
       *
       * Recent zim file use the new namespace scheme.
       *
       * On user perspective, it means that :
       * - On old namespace scheme :
       *  . All entries are accessible, either using `getEntryByPath` with a specific namespace
       *    or simply iterating over the entries (with `iter*` methods).
       *  . Entry's path has namespace included ("A/foo.html")
       * - On new namespace scheme :
       *  . Only the "user" entries are accessible with `getEntryByPath` and `iter*` methods.
       *    To access metadatas, use `getMetadata` method.
       *  . Entry's path do not contains namespace ("foo.html")
       */
      bool hasNewNamespaceScheme() const;

      /** Get a shared ptr on the FileImpl
       *
       *  @internal
       *  @return The shared_ptr
       */
      std::shared_ptr<FileImpl> getImpl() const { return m_impl; }

#ifdef ZIM_PRIVATE
      cluster_index_type getClusterCount() const;
      offset_type getClusterOffset(cluster_index_type idx) const;
      entry_index_type getMainEntryIndex() const;
#endif

    private:
      std::shared_ptr<FileImpl> m_impl;
  };

  template<EntryOrder order>
  entry_index_type _toPathOrder(const FileImpl& file, entry_index_type idx);

  template<>
  entry_index_type _toPathOrder<EntryOrder::pathOrder>(const FileImpl& file, entry_index_type idx);
  template<>
  entry_index_type _toPathOrder<EntryOrder::titleOrder>(const FileImpl& file, entry_index_type idx);
  template<>
  entry_index_type _toPathOrder<EntryOrder::efficientOrder>(const FileImpl& file, entry_index_type idx);


  /**
   * A range of entries in an `Archive`.
   *
   * `EntryRange` represents a range of entries in a specific order.
   *
   * An `EntryRange` can't be modified is consequently threadsafe.
   */
  template<EntryOrder order>
  class Archive::EntryRange {
    public:
      explicit EntryRange(const std::shared_ptr<FileImpl> file, entry_index_type begin, entry_index_type end)
        : m_file(file),
          m_begin(begin),
          m_end(end)
      {}

      iterator<order> begin() const
        { return iterator<order>(m_file, entry_index_type(m_begin)); }
      iterator<order> end() const
        { return iterator<order>(m_file, entry_index_type(m_end)); }
      int size() const
        { return m_end - m_begin; }

      EntryRange<order> offset(int start, int maxResults) const
      {
        auto begin = m_begin + start;
        if (begin > m_end) {
          begin = m_end;
        }
        auto end = m_end;
        if (begin + maxResults < end) {
          end = begin + maxResults;
        }
        return EntryRange<order>(m_file, begin, end);
      }

private:
      std::shared_ptr<FileImpl> m_file;
      entry_index_type m_begin;
      entry_index_type m_end;
  };

  /**
   * An iterator on an `Archive`.
   *
   * `Archive::iterator` stores an internal state which is not protected
   * from race-condition. It is not threadsafe.
   *
   * An `EntryRange` can't be modified and is consequently threadsafe.
   */
  template<EntryOrder order>
  class Archive::iterator : public std::iterator<std::bidirectional_iterator_tag, Entry>
  {
    public:
      explicit iterator(const std::shared_ptr<FileImpl> file, entry_index_type idx)
        : m_file(file),
          m_idx(idx),
          m_entry(nullptr)
      {}

      iterator(const iterator<order>& other)
        : m_file(other.m_file),
          m_idx(other.m_idx),
          m_entry(other.m_entry?new Entry(*other.m_entry):nullptr)
      {}

      bool operator== (const iterator<order>& it) const
        { return m_file == it.m_file && m_idx == it.m_idx; }
      bool operator!= (const iterator<order>& it) const
        { return !operator==(it); }

      iterator<order>& operator=(iterator<order>&& it) = default;

      iterator<order>& operator=(iterator<order>& it)
      {
        m_entry.reset();
        m_idx = it.m_idx;
        m_file = it.m_file;
        return *this;
      }

      iterator<order>& operator++()
      {
        ++m_idx;
        m_entry.reset();
        return *this;
      }

      iterator<order> operator++(int)
      {
        auto it = *this;
        operator++();
        return it;
      }

      iterator<order>& operator--()
      {
        --m_idx;
        m_entry.reset();
        return *this;
      }

      iterator<order> operator--(int)
      {
        auto it = *this;
        operator--();
        return it;
      }

      const Entry& operator*() const
      {
        if (!m_entry) {
          m_entry.reset(new Entry(m_file, _toPathOrder<order>(*m_file, m_idx)));
        }
        return *m_entry;
      }

      const Entry* operator->() const
      {
        operator*();
        return m_entry.get();
      }

    private:
      std::shared_ptr<FileImpl> m_file;
      entry_index_type m_idx;
      mutable std::unique_ptr<Entry> m_entry;
  };

  /**
   * The set of the integrity checks to be performed by `zim::validate()`.
   */
  typedef std::bitset<size_t(IntegrityCheck::COUNT)> IntegrityCheckList;

  /** Check the integrity of the zim file.
   *
   * Run the specified checks to verify the zim file is valid
   * (with regard to the zim format). Some checks can be quite slow.
   *
   * @param zimPath The path of the ZIM archive to be checked.
   * @param checksToRun The set of checks to perform.
   * @return False if any check fails, true otherwise.
   */
  bool validate(const std::string& zimPath, IntegrityCheckList checksToRun);
}

#endif // ZIM_ARCHIVE_H

