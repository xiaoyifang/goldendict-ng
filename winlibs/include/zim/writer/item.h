/*
 * Copyright (C) 2020-2021 Matthieu Gautier <mgautier@kymeria.fr>
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

#ifndef ZIM_WRITER_ITEM_H
#define ZIM_WRITER_ITEM_H

#include <stdexcept>
#include <zim/blob.h>
#include <zim/zim.h>
#include <zim/uuid.h>
#include <string>

#include <map>

namespace zim {
namespace writer {
enum HintKeys {
  COMPRESS,
  FRONT_ARTICLE,
};
using Hints = std::map< HintKeys, uint64_t >;

class ContentProvider;

/**
     * IndexData represent data of an Item to be indexed in the archive.
     *
     * This is a abstract class the user need to implement.
     * (But default `Item::getIndexData` returns a default implementation
     * for IndexData which works for html content.)
     */
class LIBZIM_API IndexData
{
public:
  using GeoPosition    = std::tuple< bool, double, double >;
  virtual ~IndexData() = default;

  /**
         * If the IndexData actually has data to index.
         *
         * It can be used to create `IndexData` for all your content
         * but discard some indexation based on some criteria.
         *
         * @return true if the item associated to this IndexData must be indexed.
         */
  virtual bool hasIndexData() const = 0;

  /**
         * The title to use when indexing the item.
         *
         * May be different than `Item::getTitle()`, even if most of the time
         * it will be the same.
         *
         * @return the title to use.
         */
  virtual std::string getTitle() const = 0;

  /**
         * The content to use when indexing the item.
         *
         * This is probably the most important method of `IndexData`.
         * Most item's contents are not applicable for a direct indexation.
         * We don't want to index html tags or menu/footer of an article.
         * This method allow you to return a currated plain text to indexe.
         *
         * @return the content to use.
         */
  virtual std::string getContent() const = 0;

  /**
         * The keywords to use when indexing the item.
         *
         * Return a set of keywords, separated by space for the content.
         * Keywords are indexed using a higher score than text in `getContent`
         *
         * @return a string containing keywords separated by space.
         */
  virtual std::string getKeywords() const = 0;

  /**
         * The number of words in the content.
         *
         * This value is not directly used to index the content but it
         * is stored in the xapian database, which may be used later to query
         * articles.
         *
         * @return the number of words in the item.
         */
  virtual uint32_t getWordCount() const = 0;

  /**
         * The Geographical position of the subject covered by the item.
         * (When applicable)
         *
         * @return a 3 tuple (true, latitude, longitude) if the item is
         *         about a geo positioned thing.
         *         a 3 tuple (false, _, _) if having a GeoPosition is not
         *         relevant.
         */
  virtual GeoPosition getGeoPosition() const = 0;
};

/**
     * Item represent data to be added to the archive.
     *
     * This is a abstract class the user need to implement.
     * libzim provides `BasicItem`, `StringItem` and `FileItem`
     * to simplify (or avoid) this reimplementation.
     */
class LIBZIM_API Item
{
public:
  /**
         * The path of the item.
         *
         * The path must be absolute.
         * Path must be unique.
         *
         * @return the path of the item.
         */
  virtual std::string getPath() const = 0;

  /**
         * The title of the item.
         *
         * Item's title is indexed and is used for the suggestion system.
         * Title don't have to be unique.
         *
         * @return the title of the item.
         */
  virtual std::string getTitle() const = 0;

  /**
         * The mimetype of the item.
         *
         * Mimetype is store within the content.
         * It is also used to detect if the content must be compressed or not.
         *
         * @return the mimetype of the item.
         */
  virtual std::string getMimeType() const = 0;

  /**
         * The content provider of the item.
         *
         * The content provider is responsible to provide the content to the creator.
         * The returned content provider must stay valid even after creator release
         * its reference to the item.
         *
         * This method will be called once by libzim, in the main thread
         * (but will be used in a different thread).
         * The default IndexData will also call this method once (more)
         * in the main thread (and use it in another thread).
         *
         * @return the contentProvider of the item.
         */
  virtual std::unique_ptr< ContentProvider > getContentProvider() const = 0;

  /**
         * The index data of the item.
         *
         * The index data is the data to index. (May be different from the content
         * to store).
         * The returned index data must stay valid even after creator release
         * its reference to the item.
         * This method will be called once by libzim if it is compiled with xapian
         * (and is configured to index data).
         *
         * The returned IndexData will be used as source to index the item.
         * If you don't want the item to be indexed, you can return a nullptr here
         * or return a valid IndexData pointer which will return false to `hasIndexData`.
         *
         * If you don't implement this method, a default implementation will be used.
         * The default implementation first checks for the mimetype and if the mimetype
         * contains `text/html` it will use a contentProvider to get the content to index.
         * The contentProvider will be created in the main thread but the data reading and
         * parsing will occur in a different thread.
         *
         * All methods of `IndexData` will be called in a different (same) thread.
         *
         * @return the indexData of the item.
         *         May return a nullptr if there is no indexData.
         */
  virtual std::shared_ptr< IndexData > getIndexData() const;

  /**
         * Hints to help the creator takes decision about the item.
         *
         * For now two hints are supported:
         * - COMPRESS: Can be used to force the creator to put the item content
         *   in a compressed cluster (if true) or not (if false).
         *   If the hint is not provided, the decision is taken based on the
         *   mimetype (textual or binary content ?)
         * - FRONT_ARTICLE: Can (Should) be used to specify if the item is
         *   a front article or not.
         *   If the hint is not provided, the decision is taken based on the
         *   mimetype (html or not ?)
         *
         * @return A list of hints.
         */
  virtual Hints getHints() const;

  /**
         * Returns the getHints() amended with default values based on mimetypes.
         */
  Hints getAmendedHints() const;
  virtual ~Item() = default;
};

/**
     * A BasicItem is a partial implementation of a Item.
     *
     * `BasicItem` provides a basic implementation for everything about an `Item`
     * but the actual content of the item.
     */
class LIBZIM_API BasicItem: public Item
{
public:
  /**
         * Create a BasicItem with the given path, mimetype and title.
         *
         * @param path the path of the item.
         * @param mimetype the mimetype of the item.
         * @param title the title of the item.
         */
  BasicItem( const std::string & path, const std::string & mimetype, const std::string & title, Hints hints ):
    path( path ),
    mimetype( mimetype ),
    title( title ),
    hints( hints )
  {
  }

  std::string getPath() const
  {
    return path;
  }
  std::string getTitle() const
  {
    return title;
  }
  std::string getMimeType() const
  {
    return mimetype;
  }
  Hints getHints() const
  {
    return hints;
  }

protected:
  std::string path;
  std::string mimetype;
  std::string title;
  Hints hints;
};

/**
     * A `StringItem` is a full implemented item where the content is stored in a string.
     */
class LIBZIM_API StringItem: public BasicItem, public std::enable_shared_from_this< StringItem >
{
public:
  /**
         * Create a StringItem with the given path, mimetype, title and content.
         *
         * The parameters are the ones of the private constructor.
         *
         * @param path the path of the item.
         * @param mimetype the mimetype of the item.
         * @param title the title of the item.
         * @param content the content of the item.
         */
  template< typename... Ts >
  static std::shared_ptr< StringItem > create( Ts &&... params )
  {
    return std::shared_ptr< StringItem >( new StringItem( std::forward< Ts >( params )... ) );
  }

  std::unique_ptr< ContentProvider > getContentProvider() const;

protected:
  std::string content;

private:
  StringItem( const std::string & path,
              const std::string & mimetype,
              const std::string & title,
              Hints hints,
              const std::string & content ):
    BasicItem( path, mimetype, title, hints ),
    content( content )
  {
  }
};

/**
     * A `FileItem` is a full implemented item where the content is file.
     */
class LIBZIM_API FileItem: public BasicItem
{
public:
  /**
         * Create a FileItem with the given path, mimetype, title and filenpath.
         *
         * @param path the path of the item.
         * @param mimetype the mimetype of the item.
         * @param title the title of the item.
         * @param filepath the path of the file in the filesystem.
         */
  FileItem( const std::string & path,
            const std::string & mimetype,
            const std::string & title,
            Hints hints,
            const std::string & filepath ):
    BasicItem( path, mimetype, title, hints ),
    filepath( filepath )
  {
  }

  std::unique_ptr< ContentProvider > getContentProvider() const;

protected:
  std::string filepath;
};

} // namespace writer
} // namespace zim

#endif // ZIM_WRITER_ITEM_H
