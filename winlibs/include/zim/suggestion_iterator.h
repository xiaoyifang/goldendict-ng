/*
 * Copyright (C) 2021 Maneesh P M <manu.pm55@gmail.com>
 * Copyright (C) 2020 Matthieu Gautier <mgautier@kymeria.fr>
 * Copyright (C) 2006 Tommi Maekitalo
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

#ifndef ZIM_SUGGESTION_ITERATOR_H
#define ZIM_SUGGESTION_ITERATOR_H

#include "archive.h"
#include <iterator>

namespace zim
{
class SuggestionResultSet;
class SuggestionItem;
class SearchIterator;

/**
 * A interator on suggestion.
 *
 * Be aware that the referenced/pointed SuggestionItem is generated and stored
 * in the iterator itself.
 * Once the iterator is destructed or incremented/decremented, you must NOT
 * use the SuggestionItem.
 */
class LIBZIM_API SuggestionIterator
{
    typedef Archive::iterator<EntryOrder::titleOrder> RangeIterator;
    friend class SuggestionResultSet;
    public:
        /* SuggestionIterator is conceptually a bidirectional iterator.
         * But std *LegayBidirectionalIterator* is also a *LegacyForwardIterator* and
         * it would impose us that :
         * > Given a and b, dereferenceable iterators of type It:
         * >  If a and b compare equal (a == b is contextually convertible to true)
         * >  then either they are both non-dereferenceable or *a and *b are references bound to the same object.
         * and
         * > the LegacyForwardIterator requirements requires dereference to return a reference.
         * Which cannot be as we create the entry on demand.
         *
         * So we are stick with declaring ourselves at `input_iterator`.
         */
        using iterator_category = std::input_iterator_tag;
        using value_type = SuggestionItem;
        using pointer = SuggestionItem*;
        using reference = SuggestionItem&;

        SuggestionIterator() = delete;
        SuggestionIterator(const SuggestionIterator& it);
        SuggestionIterator& operator=(const SuggestionIterator& it);
        SuggestionIterator(SuggestionIterator&& it);
        SuggestionIterator& operator=(SuggestionIterator&& it);
        ~SuggestionIterator();

        bool operator== (const SuggestionIterator& it) const;
        bool operator!= (const SuggestionIterator& it) const;

        SuggestionIterator& operator++();
        SuggestionIterator operator++(int);
        SuggestionIterator& operator--();
        SuggestionIterator operator--(int);

        Entry getEntry() const;

        const SuggestionItem& operator*();
        const SuggestionItem* operator->();

    private: // data
        struct SuggestionInternalData;
        std::unique_ptr<RangeIterator> mp_rangeIterator;
        std::unique_ptr<SuggestionItem> m_suggestionItem;

    private: // methods
        SuggestionIterator(RangeIterator rangeIterator);

// Xapian based methods and data
#if defined(LIBZIM_WITH_XAPIAN)
#ifdef ZIM_PRIVATE
    public:
        std::string getDbData() const;
#endif
    private: // xapian based data
        std::unique_ptr<SuggestionInternalData> mp_internal;

    private: // xapian based methods
        std::string getIndexPath() const;
        std::string getIndexTitle() const;
        std::string getIndexSnippet() const;
        SuggestionIterator(SuggestionInternalData* internal_data);
#endif  // LIBZIM_WITH_XAPIAN
};

class LIBZIM_API SuggestionItem
{
    public: // methods
        SuggestionItem(std::string title, std::string path, std::string snippet = "")
        :   title(title),
            path(path),
            snippet(snippet) {}

        std::string getTitle() const { return title; }
        std::string getPath() const { return path; }
        std::string getSnippet() const { return snippet; }

        bool hasSnippet() const { return !snippet.empty(); }

    private: // data
        std::string title;
        std::string path;
        std::string snippet;
};

} // namespace zim

#endif // ZIM_SUGGESTION_ITERATOR_H
