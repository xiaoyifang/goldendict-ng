/*
 * Copyright (C) 2017-2021 Matthieu Gautier <mgautier@kymeria.fr>
 * Copyright (C) 2020 Veloman Yunkan
 * Copyright (C) 2009 Tommi Maekitalo
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

#ifndef ZIM_WRITER_CREATOR_H
#define ZIM_WRITER_CREATOR_H

#include <memory>
#include <zim/zim.h>
#include <zim/writer/item.h>

namespace zim
{
  class Fileheader;
  namespace writer
  {
    class CreatorData;

    /**
     * The `Creator` is responsible to create a zim file.
     *
     * Once the `Creator` is instantiated, it can be configured with the
     * `config*` methods.
     * Then the creation process must be started with `startZimCreation`.
     * Elements of the zim file can be added using the `add*` methods.
     * The final steps is to call `finishZimCreation`.
     *
     * During the creation of the zim file (and before the call to `finishZimCreation`),
     * some values must be set using the `set*` methods.
     *
     * All `add*` methods and `finishZimCreation` can throw a exception.
     * (most of the time zim::CreatorError child but not limited to)
     * It is up to the user to catch this exception and handle the error.
     * The current (documented) conditions when a exception is thrown are:
     * - When a entry cannot be added (mainly because a entry with the same path has already been added)
     *    A `zim::InvalidEntry` will be thrown. The creator will still be in a valid state and the creation can continue.
     * - An exception has been thrown in a worker thread.
     *    This exception will be catch and rethrown through a `zim::AsyncError`.
     *    The creator will be set in a invalid state and creation cannot continue.
     * - The creator is in error state.
     *    A `zim::CreatorStateError` will be thrown.
     * - Any exception thrown by user implementation itself.
     *    Note that this exception may be thrown in a worker thread and so being "catch" by a AsyncError.
     * - Any other exception thrown for unknown reason.
     * By default, creator status is not changed by thrown exception and creation should stop.
     */
    class LIBZIM_API Creator
    {
      public:
        /**
         * Creator constructor.
         *
         * @param verbose If the creator print verbose information.
         * @param comptype The compression algorithm to use.
         */
        Creator();
        virtual ~Creator();

        /**
         * Configure the verbosity of the creator
         *
         * @param verbose if the creator print verbose information.
         * @return a reference to itself.
         */
        Creator& configVerbose(bool verbose);

        /**
         * Configure the compression algorithm to use.
         *
         * @param comptype the compression algorithm to use.
         * @return a reference to itself.
         */
        Creator& configCompression(Compression compression);

        /**
         * Set the size of the created clusters.
         *
         * The creator will try to create cluster with (uncompressed) size
         * as close as possible to targetSize without exceeding that limit.
         * If not possible, the only such case being an item larger than targetSize,
         * a separated cluster will be allocated for that oversized item.
         *
         * Be carefull with this value.
         * Bigger value means more content put together, so a better compression ratio.
         * But it means also that more decompression has to be made when reading a blob.
         * If you don't know which value to put, don't use this method and let libzim
         * use the default value.
         *
         * @param targetSize The target size of a cluster (in byte).
         * @return a reference to itself.
         */
        Creator& configClusterSize(zim::size_type targetSize);

        /**
         * Configure the fulltext indexing feature.
         *
         * @param indexing True if we must fulltext index the content.
         * @param language Language to use for the indexation.
         * @return a reference to itself.
         */
        Creator& configIndexing(bool indexing, const std::string& language);

        /**
         * Set the number of thread to use for the internal worker.
         *
         * @param nbWorkers The number of workers to use.
         * @return a reference to itself.
         */
        Creator& configNbWorkers(unsigned nbWorkers);

        /**
         * Start the zim creation.
         *
         * The creator must have been configured before calling this method.
         *
         * @param filepath the path of the zim file to create.
         */
        void startZimCreation(const std::string& filepath);

        /**
         * Add a item to the archive.
         *
         * @param item The item to add.
         */
        void addItem(std::shared_ptr<Item> item);

        /**
         * Add a metadata to the archive.
         *
         * @param name the name of the metadata
         * @param content the content of the metadata
         * @param mimetype the mimetype of the metadata.
         *                 Only used to detect if the metadata must be compressed or not.
         */
        void addMetadata(const std::string& name, const std::string& content, const std::string& mimetype = "text/plain;charset=utf-8");

        /**
         * Add a metadata to the archive using a contentProvider instead of plain string.
         *
         * @param name the name of the metadata.
         * @param provider the provider of the content of the metadata.
         * @param mimetype the mimetype of the metadata.
         *                 Only used to detect if the metadata must be compressed.
         */
        void addMetadata(const std::string& name, std::unique_ptr<ContentProvider> provider, const std::string& mimetype = "text/plain;charset=utf-8");

        /**
         * Add illustration to the archive.
         *
         * @param size the size (width and height) of the illustration.
         * @param content the content of the illustration (must be a png content)
         */
        void addIllustration(unsigned int size, const std::string& content);

        /**
         * Add illustration to the archive.
         *
         * @param size the size (width and height) of the illustration.
         * @param provider the provider of the content of the illustration (must be a png content)
         */
        void addIllustration(unsigned int size, std::unique_ptr<ContentProvider> provider);

        /**
         * Add a redirection to the archive.
         *
         * Hints (especially FRONT_ARTICLE) can be used to put the redirection
         * in the front articles list.
         * By default, redirections are not front article.
         *
         * @param path the path of the redirection.
         * @param title the title of the redirection.
         * @param targetpath the path of the target of the redirection.
         * @param hints hints associated to the redirection.
         */
        void addRedirection(
            const std::string& path,
            const std::string& title,
            const std::string& targetpath,
            const Hints& hints = Hints());

        /**
         * Finalize the zim creation.
         */
        void finishZimCreation();

        /**
         * Set the path of the main page.
         *
         * @param mainPath The path of the main page.
         */
        void setMainPath(const std::string& mainPath) { m_mainPath = mainPath; }

        /**
         * Set the uuid of the the archive.
         *
         * @param uuid The uuid of the archive.
         */
        void setUuid(const zim::Uuid& uuid) { m_uuid = uuid; }

      private:
        std::unique_ptr<CreatorData> data;

        // configuration
        bool m_verbose = false;
        Compression m_compression = Compression::Zstd;
        bool m_withIndex = false;
        size_t m_clusterSize;
        std::string m_indexingLanguage;
        unsigned m_nbWorkers = 4;

        // zim data
        std::string m_mainPath;
        Uuid m_uuid = Uuid::generate();

        void fillHeader(Fileheader* header) const;
        void writeLastParts() const;
        void checkError();
    };
  }

}

#endif // ZIM_WRITER_CREATOR_H
