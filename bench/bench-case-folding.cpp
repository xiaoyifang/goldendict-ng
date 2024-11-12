#include "folding.hh"
#include <benchmark/benchmark.h>

static std::u32string a = U"HOMEBREW_NO_AUTO_UPDATE. Hide these hints with HOMEBREW_NO_ENV_HINTS (see `man brew`).";

static void applyFolding( benchmark::State & state )
{
  for ( auto _ : state ) {
    Folding::apply( a );
  }
}
BENCHMARK( applyFolding );


static void applyFolding2( benchmark::State & state )
{
  for ( auto _ : state ) {
    Folding::apply2( a );
  }
}
BENCHMARK( applyFolding2 );