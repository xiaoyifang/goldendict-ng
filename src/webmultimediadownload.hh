#ifndef WEBMULTIMEDIADOWNLOAD_HH
#define WEBMULTIMEDIADOWNLOAD_HH

#include "dict/dictionary.hh"
#include <QtNetwork>

namespace Dictionary {

/// Downloads data from the web, wrapped as a dictionary's DataRequest. This
/// is useful for multimedia files, like sounds and pronunciations.
class WebMultimediaDownload: public DataRequest
{
  Q_OBJECT

  QNetworkReply * reply;
  QNetworkAccessManager & mgr;
  int redirectCount;

public:

  WebMultimediaDownload( QUrl const &, QNetworkAccessManager & );

  virtual void cancel();

private slots:

  void replyFinished( QNetworkReply * );
};

} // namespace Dictionary

#endif // WEBMULTIMEDIADOWNLOAD_HH
