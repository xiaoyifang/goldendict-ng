/*
 * Copyright (C) 2020 Matthieu Gautier <mgautier@kymeria.fr>
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

#ifndef ZIM_WRITER_CONTENTPROVIDER_H
#define ZIM_WRITER_CONTENTPROVIDER_H

#include <stdexcept>
#include <zim/blob.h>
#include <zim/zim.h>
#include <string>

namespace zim {
#ifdef _WIN32
  #define DEFAULTFD zim::windows::FD
namespace windows {
#else
  #define DEFAULTFD zim::unix::FD
namespace unix {
#endif
class FD;
}
namespace writer {
/**
     * `ContentProvider` is an abstract class in charge of providing the content to
     * add in the archive to the creator.
     */
class LIBZIM_API ContentProvider
{
public:
  virtual ~ContentProvider() = default;
  /**
         * The size of the content to add into the archive.
         *
         * @return the total size of the content.
         */
  virtual zim::size_type getSize() const = 0;

  /**
         * Return a blob to add to the archive.
         *
         * The returned blob doesn't have to represent the whole content.
         * The feed method can return the whole content chunk by chunk or in
         * one step.
         * When the whole content has been returned, feed must return an empty blob
         * (size == 0).
         *
         * This method will be called several times (at least twice) for
         * each content to add.
         *
         * It is up to the implementation to manage correctly the data pointed by
         * the returned blob.
         * It may (re)use the same buffer between calls (rewriting its content),
         * create a new buffer each time or make the blob point to a new region of
         * a big buffer.
         * It is up to the implementation to free any allocated memory.
         *
         * The data pointed by the blob must stay valid until the next call to feed.
         * A call to feed ensure that the data returned by a previous call will not
         * be used anymore.
         */
  virtual Blob feed() = 0;
};

/**
     * StringProvider provide the content stored in a string.
     */
class LIBZIM_API StringProvider: public ContentProvider
{
public:
  /**
         * Create a provider using a string as content.
         * The string content is copied and the reference don't have to be "keep" alive.
         *
         * @param content the content to serve.
         */
  explicit StringProvider( const std::string & content ):
    content( content ),
    feeded( false )
  {
  }
  zim::size_type getSize() const
  {
    return content.size();
  }
  Blob feed();

protected:
  std::string content;
  bool feeded;
};

/**
     * SharedStringProvider provide the content stored in a shared string.
     *
     * It is mostly the same thing that `StringProvider` but use a shared_ptr
     * to avoid copy.
     */
class LIBZIM_API SharedStringProvider: public ContentProvider
{
public:
  /**
         * Create a provider using a string as content.
         * The string content is not copied.
         *
         * @param content the content to serve.
         */
  explicit SharedStringProvider( std::shared_ptr< const std::string > content ):
    content( content ),
    feeded( false )
  {
  }
  zim::size_type getSize() const
  {
    return content->size();
  }
  Blob feed();

protected:
  std::shared_ptr< const std::string > content;
  bool feeded;
};

/**
     * FileProvider provide the content stored in file.
     */
class LIBZIM_API FileProvider: public ContentProvider
{
public:
  /**
         * Create a provider using file as content.
         *
         * @param filepath the path to the file to serve.
         */
  explicit FileProvider( const std::string & filepath );
  ~FileProvider();
  zim::size_type getSize() const
  {
    return size;
  }
  Blob feed();

protected:
  std::string filepath;
  zim::size_type size;

private:
  std::unique_ptr< char[] > buffer;
  std::unique_ptr< DEFAULTFD > fd;
  zim::offset_type offset;
};

} // namespace writer
} // namespace zim

#undef DEFAULTFD

#endif // ZIM_WRITER_CONTENTPROVIDER_H
