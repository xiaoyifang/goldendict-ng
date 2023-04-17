#ifndef __TIFF_HH_INCLUDED__
#define __TIFF_HH_INCLUDED__

#include <QImage>
#include <vector>
namespace GdTiff {

void tiff2img( std::vector< char > & data, const char * format = "webp" );

}


#endif // TIFF_HH
