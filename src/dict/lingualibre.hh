#ifndef GOLDENDICT_LINGUALIBRE_H
#define GOLDENDICT_LINGUALIBRE_H

#include "dictionary.hh"
#include "config.hh"
#include "wstring.hh"
#include <QNetworkAccessManager>
#include <QNetworkReply>


namespace Lingua{

using std::vector;
using std::string;
using gd::wstring;

vector< sptr< Dictionary::Class > > makeDictionaries(
  Dictionary::Initializing &,
  Config::Lingua const &,
  QNetworkAccessManager & );


/// Exposed here for moc
class LinguaArticleRequest: public Dictionary::DataRequest
{
  Q_OBJECT

  struct NetReply
  {
    sptr< QNetworkReply > reply;
    string word;
    bool finished;

    NetReply( sptr< QNetworkReply > const & reply_, string const & word_ ):
        reply( reply_ ), word( word_ ), finished( false )
    {}
  };

  typedef std::list< NetReply > NetReplies;
  NetReplies netReplies;
  QString languageCode,langWikipediaID;
  string dictionaryId;

 public:

  LinguaArticleRequest( wstring const & word,
                        vector< wstring > const & alts,
                        QString const & languageCode_,
                        QString const & langWikipediaID_,
                        string const & dictionaryId_,
                        QNetworkAccessManager & mgr );

  virtual void cancel();

 private:

  void addQuery( QNetworkAccessManager & mgr, wstring const & word );

 private slots:
  virtual void requestFinished( QNetworkReply * );
};

}

#endif //GOLDENDICT_LINGUALIBRE_H
