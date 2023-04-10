
/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "termination.hh"
#include <exception>
#include <QtCore>


static void termHandler()
{

  // Trick: In c++17, there is no standardized way to know the exception name/type inside terminate_handler
  // So, we just get the exception and throw it again, so that we can call the std::exception::what :)

  // This trick can be removed/improved if something similar to
  // boost::current_exception_diagnostic_information got standardized,

  std::exception_ptr curException = std::current_exception();

  try {
    if ( curException != nullptr ) {
      std::rethrow_exception( curException );
    }
    else {
      qDebug() << "GoldenDict has crashed unexpectedly.\n\n";
    }
  }
  catch ( const std::exception & e ) {
    qDebug() << "GoldenDict has crashed with exception: " << e.what() ;
  }

  if( logFilePtr && logFilePtr->isOpen() )
    logFilePtr->close();

  abort();
}

void installTerminationHandler()
{
  std::set_terminate( termHandler );
}
