/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <string>
#include <fmt/format.h>

// clang-format off

/// A way to declare an exception class fast
/// Do like this:
/// DEF_EX( exErrorInFoo, "An error in foo encountered", std::exception )
/// DEF_EX( exFooNotFound, "Foo was not found", exErrorInFoo )
#define DEF_EX( exName, exDescription, exParent )         \
  constexpr static char ExStr_## exName[] = exDescription; \
  using exName = defineEx< exParent, ExStr_## exName >;

/// Same as DEF_EX, but takes a runtime string argument, which gets concatenated
/// with the description.
///
///   DEF_EX_STR( exCantOpen, "can't open file", std::exception )
///   ...
///   throw exCantOpen( "example.txt" );
///
///   what() would return "can't open file example.txt"
///
#define DEF_EX_STR( exName, exDescription, exParent )     \
  constexpr static char ExStr_## exName[] = exDescription; \
  using exName = defineExStr< exParent, ExStr_## exName >;

// clang-format on

template< typename ParentEx, const char * description >
class defineEx: public ParentEx
{
public:
  virtual const char * what() const noexcept
  {
    return description;
  }
};

template< typename ParentEx, const char * description >
class defineExStr: public ParentEx
{
public:
  explicit defineExStr( std::string const & message_ ):
    message( fmt::format( "{} {}", description, message_ ) )
  {
  }
  virtual const char * what() const noexcept
  {
    return message.c_str();
  }

private:
  std::string message;
};
