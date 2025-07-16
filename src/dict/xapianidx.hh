#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <stdexcept>
#include <stdint.h>
#include <QSet>
#include <QString>
#include <QList>
#include <QAtomicInt>

struct WordArticleLink
{
  std::string word;
  uint32_t articleOffset;
  std::string prefix;

  WordArticleLink( const string & w, uint32_t offset, const string & p = "" ):
    word( w ),
    articleOffset( offset ),
    prefix( p )
  {
  }
};

class exIndexWasNotOpened : public std::runtime_error {
public:
    exIndexWasNotOpened();
};

class exCorruptedChainData : public std::runtime_error {
public:
    exCorruptedChainData();
};

namespace XapianIndexing {

class XapianIndex : public BaseIndex {
public:
    XapianIndex();
    ~XapianIndex();

    void openIndex(const std::string &dbPath) override;
    std::vector<WordArticleLink> findArticles(const char32_t *search_word, bool ignoreDiacritics, uint32_t maxMatchCount) override;
    void getAllHeadwords(QSet<QString> &headwords) override;
    void findAllArticleLinks(QList<WordArticleLink> &articleLinks) override;
    void findArticleLinks(QList<WordArticleLink> *articleLinks, QSet<uint32_t> *offsets, QSet<QString> *headwords, QAtomicInt *isCancelled) override;
    void getHeadwordsFromOffsets(QList<uint32_t> &offsets, QList<QString> &headwords, QAtomicInt *isCancelled) override;

protected:
    std::vector<WordArticleLink> readChain(const char *&ptr, uint32_t maxMatchCount) override;
    void antialias(const char32_t *str, std::vector<WordArticleLink> &chain, bool ignoreDiacritics) override;

private:
    void buildIndex(const std::unordered_map<std::string, std::vector<WordArticleLink>> &indexedWords, const std::string &dbPath);
    void antialias(const char32_t *str, std::vector<WordArticleLink> &chain, bool ignoreDiacritics);
    class XapianWritableDatabase;
    XapianWritableDatabase* database;
    bool isOpened;
    std::mutex dbMutex;
};

} // namespace XapianIndexing
