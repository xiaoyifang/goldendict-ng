#include <xapian.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <zlib.h>
#include <QSet>
#include <QString>
#include <QList>
#include <QAtomicInt>
#include <QDebug>
#include "xapianidx.hh"

using namespace std;

struct WordArticleLink;

class exIndexWasNotOpened : public runtime_error {
public:
    exIndexWasNotOpened() : runtime_error("Index was not opened") {}
};

class exCorruptedChainData : public runtime_error {
public:
    exCorruptedChainData() : runtime_error("Corrupted chain data") {}
};

namespace XapianIndexing {

class XapianIndex {
public:
    XapianIndex();
    ~XapianIndex();

    void openIndex(const string& dbPath);
    vector<WordArticleLink> findArticles(const u32string& search_word, bool ignoreDiacritics, uint32_t maxMatchCount);
    vector<WordArticleLink> readChain(const char*& ptr, uint32_t maxMatchCount);
    void getAllHeadwords(QSet<QString>& headwords);
    void findAllArticleLinks(QList<WordArticleLink>& articleLinks);
    void findArticleLinks(QList<WordArticleLink>* articleLinks, QSet<uint32_t>* offsets, QSet<QString>* headwords, QAtomicInt* isCancelled);
    void getHeadwordsFromOffsets(QList<uint32_t>& offsets, QList<QString>& headwords, QAtomicInt* isCancelled);

private:
    Xapian::WritableDatabase* database;
    bool isOpened;
    mutex dbMutex;

    // Helper methods
    void buildIndex(const unordered_map<string, vector<WordArticleLink>>& indexedWords, const string& dbPath);
    void antialias(const u32string& str, vector<WordArticleLink>& chain, bool ignoreDiacritics);
};

XapianIndex::XapianIndex()
    : database(nullptr), isOpened(false)
{
}

XapianIndex::~XapianIndex()
{
    if (database && isOpened) {
        delete database;
    }
}

void XapianIndex::openIndex(const string& dbPath)
{
    lock_guard<mutex> lock(dbMutex);
    if (isOpened) {
        return;
    }

    try {
        database = new Xapian::WritableDatabase(dbPath, Xapian::DB_CREATE_OR_OPEN);
        isOpened = true;
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to open Xapian database: " << QString::fromStdString(e.get_msg());
        throw runtime_error("Failed to open Xapian database");
    }
}

void XapianIndex::buildIndex(const unordered_map<string, vector<WordArticleLink>>& indexedWords, const string& dbPath)
{
    try {
        Xapian::WritableDatabase db(dbPath, Xapian::DB_CREATE_OR_OVERWRITE);

        for (const auto& entry : indexedWords) {
            const string& term = entry.first;
            const vector<WordArticleLink>& links = entry.second;

            Xapian::Document doc;
            for (const auto& link : links) {
                // 将 articleOffset 作为文档数据的一部分
                doc.add_term("A" + to_string(link.articleOffset));
            }

            // 使用词条作为唯一标识符
            doc.set_data(term);
            db.replace_document(Xapian::QueryParser::escape_query_term(term), doc);
        }

        db.commit();
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to build Xapian index: " << QString::fromStdString(e.get_msg());
        throw runtime_error("Failed to build Xapian index");
    }
}

vector<WordArticleLink> XapianIndex::findArticles(const u32string& search_word, bool ignoreDiacritics, uint32_t maxMatchCount)
{
    vector<WordArticleLink> result;

    if (!isOpened) {
        throw exIndexWasNotOpened();
    }

    lock_guard<mutex> lock(dbMutex);

    try {
        Xapian::Enquire enquire(*database);
        Xapian::Query query;

        // 构建查询条件
        string searchTerm = Text::toUtf8(search_word);  // 假设 Text::toUtf8 可以将 u32string 转换为 UTF-8 字符串
        query = Xapian::Query(Xapian::QueryParser::escape_query_term(searchTerm));

        enquire.set_query(query);
        Xapian::MSet mset = enquire.get_mset(0, maxMatchCount);

        for (Xapian::MSetIterator it = mset.begin(); it != mset.end(); ++it) {
            Xapian::Document doc = it.get_document();
            string term = doc.get_data();

            // 解析文档中的 articleOffset
            for (auto&& termIter : doc.termlist()) {
                string termName = termIter.get_termname();
                if (termName.substr(0, 1) == "A") {
                    uint32_t offset = static_cast<uint32_t>(stoul(termName.substr(1)));
                    result.emplace_back(term, offset);
                }
            }
        }

        antialias(search_word, result, ignoreDiacritics);
    } catch (const exception& e) {
        qWarning() << "Search failed: " << e.what();
        result.clear();
    }

    return result;
}

void XapianIndex::antialias(const u32string& str, vector<WordArticleLink>& chain, bool ignoreDiacritics)
{
    // 这里可以实现具体的去重逻辑
    // 示例中直接保留结果
}

vector<WordArticleLink> XapianIndex::readChain(const char*& ptr, uint32_t maxMatchCount)
{
    // 由于 Xapian 已经处理了底层的数据结构，这里不需要手动解析链表
    // 返回空向量
    return vector<WordArticleLink>();
}

void XapianIndex::getAllHeadwords(QSet<QString>& headwords)
{
    if (!isOpened) {
        throw exIndexWasNotOpened();
    }

    lock_guard<mutex> lock(dbMutex);

    try {
        Xapian::TermIterator it = database->allterms_begin();
        Xapian::TermIterator end = database->allterms_end();

        for (; it != end; ++it) {
            if (it->substr(0, 1) != "A") {  // 排除 articleOffset 相关的术语
                headwords.insert(QString::fromStdString(*it));
            }
        }
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to retrieve headwords: " << QString::fromStdString(e.get_msg());
    }
}

void XapianIndex::findAllArticleLinks(QList<WordArticleLink>& articleLinks)
{
    if (!isOpened) {
        throw exIndexWasNotOpened();
    }

    lock_guard<mutex> lock(dbMutex);

    try {
        Xapian::DocIterator it = database->doc_iterator_begin();
        Xapian::DocIterator end = database->doc_iterator_end();

        for (; it != end; ++it) {
            Xapian::Document doc = *it;
            string term = doc.get_data();

            for (auto&& termIter : doc.termlist()) {
                string termName = termIter.get_termname();
                if (termName.substr(0, 1) == "A") {
                    uint32_t offset = static_cast<uint32_t>(stoul(termName.substr(1)));
                    articleLinks.push_back(WordArticleLink(term, offset));
                }
            }
        }
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to retrieve article links: " << QString::fromStdString(e.get_msg());
    }
}

void XapianIndex::findArticleLinks(QList<WordArticleLink>* articleLinks, QSet<uint32_t>* offsets, QSet<QString>* headwords, QAtomicInt* isCancelled)
{
    if (!isOpened) {
        throw exIndexWasNotOpened();
    }

    lock_guard<mutex> lock(dbMutex);

    try {
        Xapian::DocIterator it = database->doc_iterator_begin();
        Xapian::DocIterator end = database->doc_iterator_end();

        for (; it != end && (!isCancelled || !isCancelled->loadAcquire()); ++it) {
            Xapian::Document doc = *it;
            string term = doc.get_data();

            for (auto&& termIter : doc.termlist()) {
                string termName = termIter.get_termname();
                if (termName.substr(0, 1) == "A") {
                    uint32_t offset = static_cast<uint32_t>(stoul(termName.substr(1)));

                    if (offsets && offsets->contains(offset)) {
                        continue;
                    }

                    if (offsets) {
                        offsets->insert(offset);
                    }

                    if (articleLinks) {
                        articleLinks->push_back(WordArticleLink(term, offset));
                    }

                    if (headwords) {
                        headwords->insert(QString::fromStdString(term));
                    }
                }
            }
        }
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to retrieve article links: " << QString::fromStdString(e.get_msg());
    }
}

void XapianIndex::getHeadwordsFromOffsets(QList<uint32_t>& offsets, QList<QString>& headwords, QAtomicInt* isCancelled)
{
    if (!isOpened) {
        throw exIndexWasNotOpened();
    }

    lock_guard<mutex> lock(dbMutex);

    try {
        Xapian::DocIterator it = database->doc_iterator_begin();
        Xapian::DocIterator end = database->doc_iterator_end();

        for (; it != end && (!isCancelled || !isCancelled->loadAcquire()); ++it) {
            Xapian::Document doc = *it;
            string term = doc.get_data();

            for (auto&& termIter : doc.termlist()) {
                string termName = termIter.get_termname();
                if (termName.substr(0, 1) == "A") {
                    uint32_t offset = static_cast<uint32_t>(stoul(termName.substr(1)));

                    if (offsets.contains(offset)) {
                        QString word = QString::fromStdString(term);
                        if (!headwords.contains(word)) {
                            headwords.append(word);
                        }
                        offsets.removeAll(offset);
                    }

                    if (offsets.isEmpty()) {
                        break;
                    }
                }
            }

            if (offsets.isEmpty()) {
                break;
            }
        }
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to retrieve headwords from offsets: " << QString::fromStdString(e.get_msg());
    }
}

} // namespace XapianIndexing