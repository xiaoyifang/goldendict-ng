#pragma once

#include "config.hh"

#include <QNetworkReply>
#include <QObject>

class AnkiConnector: public QObject
{
  Q_OBJECT

public:
  explicit AnkiConnector( QObject * parent, const Config::Class & cfg );

  void sendToAnki( const QString & word, QString text, const QString & sentence );
  void ankiSearch( const QString & word );

private:
  QNetworkAccessManager * mgr;
  const Config::Class & cfg;
  void postToAnki( const QString & postData );
  static constexpr auto transfer_timeout = 3000;

public:
signals:
  void errorText( const QString & );
private slots:
  void finishedSlot( QNetworkReply * reply );
};
