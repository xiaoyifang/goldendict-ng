/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "tiff.hh"
#include <QBuffer>
#include <QApplication>
#include <QScreen>

namespace GdTiff {

void tiff2img( std::vector< char > & data )
{
  QImage img = QImage::fromData( (unsigned char *)&data.front(), data.size() );

  if ( !img.isNull() ) {
    QByteArray ba;

    QBuffer buffer( &ba ); // buffer doesn't own ba
    buffer.open( QIODevice::WriteOnly );

    QSize screenSize = QApplication::primaryScreen()->availableSize();
    QSize imgSize    = img.size();
    int scaleSize    = qMin( imgSize.width(), screenSize.width() );
    img.scaledToWidth( scaleSize ).save( &buffer, "webp" );

    memcpy( data.data(), ba.data(), ba.size() );
    data.resize( ba.size() );
  }
}

} // namespace GdTiff
