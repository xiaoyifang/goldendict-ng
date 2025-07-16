#pragma once

#include <string>
#include <vector>
#include <QSet>
#include <QString>
#include <QList>
#include <QAtomicInt>

struct WordArticleLink
{
  std::string word;
  uint32_t articleOffset;
  std::string prefix;

  WordArticleLink( const std::string & w, uint32_t offset, const std::string & p = "" ):
    word( w ),
    articleOffset( offset ),
    prefix( p )
  {
  }
};

class BaseIndex {
public:
  virtual ~BaseIndex() = default;
  
  virtual void openIndex( const IndexInfo &, File::Index &, QMutex & ) = 0;
  virtual void openIndex(const std::string &) = 0;
  virtual std::vector<WordArticleLink> findArticles(const char32_t *, bool, uint32_t) = 0;
  virtual void getAllHeadwords(QSet<QString> &) = 0;
  virtual void findAllArticleLinks(QList<WordArticleLink> &) = 0;
  virtual void findArticleLinks(QList<WordArticleLink> *, QSet<uint32_t> *, QSet<QString> *, QAtomicInt *) = 0;
  virtual void getHeadwordsFromOffsets(QList<uint32_t> &, QList<QString> &, QAtomicInt *) = 0;

protected:
  virtual std::vector<WordArticleLink> readChain(const char *&, uint32_t) = 0;
  virtual void antialias(const char32_t *, std::vector<WordArticleLink> &, bool) = 0;
};