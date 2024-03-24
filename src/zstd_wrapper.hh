#pragma once

#include <zstd.h>

namespace ZSTD {

struct deleter
{
  void operator()( ZSTD_DCtx * Ctx ) const
  {
    ZSTD_freeDCtx( Ctx );
  }

  void operator()( ZSTD_CCtx * Ctx ) const
  {
    ZSTD_freeCCtx( Ctx );
  }
};

} // namespace ZSTD
