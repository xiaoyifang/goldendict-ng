#pragma once

#include <QImage>
#include <vector>
namespace GdTiff {

void tiff2img( std::vector< char > & data, const char * format = "webp" );

}


