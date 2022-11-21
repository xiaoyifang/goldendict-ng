/** @file
 * @brief Factory functions for constructing Database and WritableDatabase objects
 */
/* Copyright (C) 2005,2006,2007,2008,2009,2011,2013,2014,2016 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef XAPIAN_INCLUDED_DBFACTORY_H
#define XAPIAN_INCLUDED_DBFACTORY_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/dbfactory.h> directly; include <xapian.h> instead.
#endif

#ifndef _MSC_VER
# include <sys/types.h>
#endif

#include <string>

#include <xapian/constants.h>
#include <xapian/database.h>
#include <xapian/deprecated.h>
#include <xapian/types.h>
#include <xapian/version.h>
#include <xapian/visibility.h>

namespace Xapian {

#ifdef _MSC_VER
typedef unsigned useconds_t;
#endif

/// Database factory functions which determine the database type automatically.
namespace Auto {

/** Construct a Database object for a stub database file.
 *
 * The stub database file contains serialised parameters for one
 * or more databases.
 *
 * @param file  pathname of the stub database file.
 */
XAPIAN_DEPRECATED(Database open_stub(const std::string &file));

inline Database
open_stub(const std::string &file)
{
    return Database(file, DB_BACKEND_STUB);
}

/** Construct a WritableDatabase object for a stub database file.
 *
 *  The stub database file must contain serialised parameters for exactly one
 *  database.
 *
 *  @param file		pathname of the stub database file.
 *  @param action	determines handling of existing/non-existing database:
 *  - Xapian::DB_CREATE			fail if database already exist,
 *					otherwise create new database.
 *  - Xapian::DB_CREATE_OR_OPEN		open existing database, or create new
 *					database if none exists.
 *  - Xapian::DB_CREATE_OR_OVERWRITE	overwrite existing database, or create
 *					new database if none exists.
 *  - Xapian::DB_OPEN			open existing database, failing if none
 *					exists.
 */
XAPIAN_DEPRECATED(WritableDatabase open_stub(const std::string &file, int action));

inline WritableDatabase
open_stub(const std::string &file, int action)
{
    return WritableDatabase(file, action|DB_BACKEND_STUB);
}

}

#ifdef XAPIAN_HAS_INMEMORY_BACKEND
/// Database factory functions for the inmemory backend.
namespace InMemory {

/** Construct a WritableDatabase object for a new, empty InMemory database.
 *
 *  Only a writable InMemory database can be created, since a read-only one
 *  would always remain empty.
 */
XAPIAN_DEPRECATED(WritableDatabase open());

inline WritableDatabase
open()
{
    return WritableDatabase(std::string(), DB_BACKEND_INMEMORY);
}

}
#endif

#ifdef XAPIAN_HAS_CHERT_BACKEND
/// Database factory functions for the chert backend.
namespace Chert {

/** Construct a Database object for read-only access to a Chert database.
 *
 * @param dir  pathname of the directory containing the database.
 */
XAPIAN_DEPRECATED(Database open(const std::string &dir));

inline Database
open(const std::string &dir)
{
    return Database(dir, DB_BACKEND_CHERT);
}

/** Construct a Database object for update access to a Chert database.
 *
 * @param dir		pathname of the directory containing the database.
 * @param action	determines handling of existing/non-existing database:
 *  - Xapian::DB_CREATE			fail if database already exist,
 *					otherwise create new database.
 *  - Xapian::DB_CREATE_OR_OPEN		open existing database, or create new
 *					database if none exists.
 *  - Xapian::DB_CREATE_OR_OVERWRITE	overwrite existing database, or create
 *					new database if none exists.
 *  - Xapian::DB_OPEN			open existing database, failing if none
 *					exists.
 * @param block_size	the Btree blocksize to use (in bytes), which must be a
 *			power of two between 2048 and 65536 (inclusive).  The
 *			default (also used if an invalid value if passed) is
 *			8192 bytes.  This parameter is ignored when opening an
 *			existing database.
 */
XAPIAN_DEPRECATED(WritableDatabase open(const std::string &dir, int action, int block_size = 0));

inline WritableDatabase
open(const std::string &dir, int action, int block_size)
{
    return WritableDatabase(dir, action|DB_BACKEND_CHERT, block_size);
}

}
#endif

#ifdef XAPIAN_HAS_REMOTE_BACKEND
/// Database factory functions for the remote backend.
namespace Remote {

#if defined __GNUC__ && defined __MINGW32__
// Avoid deprecation warnings about useconds_t on mingw.
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/** Construct a Database object for read-only access to a remote database
 *  accessed via a TCP connection.
 *
 * Access to the remote database is via a TCP connection to the specified
 * host and port.
 *
 * @param host		hostname to connect to.
 * @param port		port number to connect to.
 * @param timeout	timeout in milliseconds.  If this timeout is exceeded
 *			for any individual operation on the remote database
 *			then Xapian::NetworkTimeoutError is thrown.  A timeout
 *			of 0 means don't timeout.  (Default is 10000ms, which
 *			is 10 seconds).
 * @param connect_timeout	timeout to use when connecting to the server.
 *				If this timeout is exceeded then
 *				Xapian::NetworkTimeoutError is thrown.  A
 *				timeout of 0 means don't timeout.  (Default is
 *				10000ms, which is 10 seconds).
 */
XAPIAN_VISIBILITY_DEFAULT
Database open(const std::string &host, unsigned int port, useconds_t timeout = 10000, useconds_t connect_timeout = 10000);

/** Construct a WritableDatabase object for update access to a remote database
 *  accessed via a TCP connection.
 *
 * Access to the remote database is via a TCP connection to the specified
 * host and port.
 *
 * @param host		hostname to connect to.
 * @param port		port number to connect to.
 * @param timeout	timeout in milliseconds.  If this timeout is exceeded
 *			for any individual operation on the remote database
 *			then Xapian::NetworkTimeoutError is thrown.  (Default
 *			is 0, which means don't timeout).
 * @param connect_timeout	timeout to use when connecting to the server.
 *				If this timeout is exceeded then
 *				Xapian::NetworkTimeoutError is thrown.  A
 *				timeout of 0 means don't timeout.  (Default is
 *				10000ms, which is 10 seconds).
 * @param flags		Xapian::DB_RETRY_LOCK or 0.
 */
XAPIAN_VISIBILITY_DEFAULT
WritableDatabase open_writable(const std::string &host, unsigned int port, useconds_t timeout = 0, useconds_t connect_timeout = 10000, int flags = 0);

/** Construct a Database object for read-only access to a remote database
 *  accessed via a program.
 *
 * Access to the remote database is done by running an external program and
 * communicating with it on stdin/stdout.
 *
 * @param program	the external program to run.
 * @param args		space-separated list of arguments to pass to program.
 * @param timeout	timeout in milliseconds.  If this timeout is exceeded
 *			for any individual operation on the remote database
 *			then Xapian::NetworkTimeoutError is thrown.  A timeout
 *			of 0 means don't timeout.  (Default is 10000ms, which
 *			is 10 seconds).
 */
XAPIAN_VISIBILITY_DEFAULT
Database open(const std::string &program, const std::string &args, useconds_t timeout = 10000);

/** Construct a WritableDatabase object for update access to a remote database
 *  accessed via a program.
 *
 * Access to the remote database is done by running an external program and
 * communicating with it on stdin/stdout.
 *
 * @param program	the external program to run.
 * @param args		space-separated list of arguments to pass to program.
 * @param timeout	timeout in milliseconds.  If this timeout is exceeded
 *			for any individual operation on the remote database
 *			then Xapian::NetworkTimeoutError is thrown.  (Default
 *			is 0, which means don't timeout).
 * @param flags		Xapian::DB_RETRY_LOCK or 0.
 */
XAPIAN_VISIBILITY_DEFAULT
WritableDatabase open_writable(const std::string &program, const std::string &args, useconds_t timeout = 0, int flags = 0);

#if defined __GNUC__ && defined __MINGW32__
# pragma GCC diagnostic pop
#endif

}
#endif

}

#endif /* XAPIAN_INCLUDED_DBFACTORY_H */
