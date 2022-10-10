/** @file
 * @brief Geospatial search support routines.
 */
/* Copyright 2008,2009 Lemur Consulting Ltd
 * Copyright 2010,2011 Richard Boulton
 * Copyright 2012,2013,2014,2015,2016 Olly Betts
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

#ifndef XAPIAN_INCLUDED_GEOSPATIAL_H
#define XAPIAN_INCLUDED_GEOSPATIAL_H

#if !defined XAPIAN_IN_XAPIAN_H && !defined XAPIAN_LIB_BUILD
# error Never use <xapian/geospatial.h> directly; include <xapian.h> instead.
#endif

#include <iterator>
#include <vector>
#include <string>

#include <xapian/attributes.h>
#include <xapian/derefwrapper.h>
#include <xapian/keymaker.h>
#include <xapian/postingsource.h>
#include <xapian/queryparser.h> // For sortable_serialise
#include <xapian/visibility.h>

namespace Xapian {

class Registry;

double
XAPIAN_NOTHROW(miles_to_metres(double miles)) XAPIAN_CONST_FUNCTION;

/** Convert from miles to metres.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
inline double
miles_to_metres(double miles) XAPIAN_NOEXCEPT
{
    return 1609.344 * miles;
}

double
XAPIAN_NOTHROW(metres_to_miles(double metres)) XAPIAN_CONST_FUNCTION;

/** Convert from metres to miles.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
inline double
metres_to_miles(double metres) XAPIAN_NOEXCEPT
{
    return metres * (1.0 / 1609.344);
}

/** A latitude-longitude coordinate.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 *
 *  Note that latitude-longitude coordinates are only precisely meaningful if
 *  the datum used to define them is specified.  This class ignores this
 *  issue - it is up to the caller to ensure that the datum used for each
 *  coordinate in a system is consistent.
 */
struct XAPIAN_VISIBILITY_DEFAULT LatLongCoord {
    /** A latitude, as decimal degrees.
     *
     *  Should be in the range -90 <= latitude <= 90
     *
     *  Positive latitudes represent the northern hemisphere.
     */
    double latitude;

    /** A longitude, as decimal degrees.
     *
     *  Will be wrapped around, so for example, -150 is equal to 210.  When
     *  obtained from a serialised form, will be in the range 0 <= longitude <
     *  360.
     *
     *  Longitudes increase as coordinates move eastwards.
     */
    double longitude;

    /** Construct an uninitialised coordinate.
     */
    XAPIAN_NOTHROW(LatLongCoord()) {}

    /** Construct a coordinate.
     *
     *  If the supplied longitude is out of the standard range, it will be
     *  normalised to the range 0 <= longitude < 360.
     *
     *  If you want to avoid the checks (for example, you know that your values
     *  are already in range), you can use the alternate constructor to
     *  construct an uninitialised coordinate, and then set the latitude and
     *  longitude directly.
     *
     *  @exception InvalidArgumentError the supplied latitude is out of range.
     */
    LatLongCoord(double latitude_, double longitude_);

    /** Unserialise a string and set this object to its coordinate.
     *
     *  @param serialised the string to unserialise the coordinate from.
     *
     *  @exception Xapian::SerialisationError if the string does not contain
     *  a valid serialised latitude-longitude pair, or contains extra data at
     *  the end of it.
     */
    void unserialise(const std::string & serialised);

    /** Unserialise a buffer and set this object to its coordinate.
     *
     *  The buffer may contain further data after that for the coordinate.
     *
     *  @param ptr A pointer to the start of the string.  This will be updated
     *  to point to the end of the data representing the coordinate.
     *  @param end A pointer to the end of the string.
     *
     *  @exception Xapian::SerialisationError if the string does not start with
     *  a valid serialised latitude-longitude pair.
     */
    void unserialise(const char ** ptr, const char * end);

    /** Return a serialised representation of the coordinate.
     */
    std::string serialise() const;

    /** Compare with another LatLongCoord.
     *
     *  This is mostly provided so that things like std::map<LatLongCoord> work
     *  - the ordering isn't particularly meaningful.
     */
    bool XAPIAN_NOTHROW(operator<(const LatLongCoord & other) const)
    {
	if (latitude < other.latitude) return true;
	if (latitude > other.latitude) return false;
	return (longitude < other.longitude);
    }

    /// Return a string describing this object.
    std::string get_description() const;
};

/** An iterator across the values in a LatLongCoords object.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
class XAPIAN_VISIBILITY_DEFAULT LatLongCoordsIterator {
    /// Friend class which needs to be able to construct us.
    friend class LatLongCoords;

    /// The current position of the iterator.
    std::vector<LatLongCoord>::const_iterator iter;

    /// Constructor used by LatLongCoords.
    LatLongCoordsIterator(std::vector<LatLongCoord>::const_iterator iter_)
	    : iter(iter_) {}

  public:
    /// Default constructor.  Produces an uninitialised iterator.
    LatLongCoordsIterator() {}

    /** Get the LatLongCoord for the current position. */
    const LatLongCoord & operator*() const {
	return *iter;
    }

    /// Advance the iterator to the next position.
    LatLongCoordsIterator & operator++() {
	++iter;
	return *this;
    }

    /// Advance the iterator to the next position (postfix version).
    DerefWrapper_<LatLongCoord> operator++(int) {
	const LatLongCoord & tmp = **this;
	++iter;
	return DerefWrapper_<LatLongCoord>(tmp);
    }

    /// Equality test for LatLongCoordsIterator objects.
    bool operator==(const LatLongCoordsIterator &other) const
    {
	return iter == other.iter;
    }

    /** @private @internal LatLongCoordsIterator is what the C++ STL calls an
     *  input_iterator.
     *
     *  The following typedefs allow std::iterator_traits<> to work so that
     *  this iterator can be used with the STL.
     *
     *  These are deliberately hidden from the Doxygen-generated docs, as the
     *  machinery here isn't interesting to API users.  They just need to know
     *  that Xapian iterator classes are compatible with the STL.
     */
    // @{
    /// @private
    typedef std::input_iterator_tag iterator_category;
    /// @private
    typedef LatLongCoord value_type;
    /// @private
    typedef size_t difference_type;
    /// @private
    typedef LatLongCoord * pointer;
    /// @private
    typedef LatLongCoord & reference;
    // @}
};

/** A sequence of latitude-longitude coordinates.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
class XAPIAN_VISIBILITY_DEFAULT LatLongCoords {
    /// The coordinates.
    std::vector<LatLongCoord> coords;

  public:
    /// Get a begin iterator for the coordinates.
    LatLongCoordsIterator begin() const {
	return LatLongCoordsIterator(coords.begin());
    }

    /// Get an end iterator for the coordinates.
    LatLongCoordsIterator end() const {
	return LatLongCoordsIterator(coords.end());
    }

    /// Get the number of coordinates in the container.
    size_t size() const
    {
	return coords.size();
    }

    /// Return true if and only if there are no coordinates in the container.
    bool empty() const
    {
	return coords.empty();
    }

    /// Append a coordinate to the end of the sequence.
    void append(const LatLongCoord & coord)
    {
	coords.push_back(coord);
    }

    /// Construct an empty container.
    LatLongCoords() : coords() {}

    /// Construct a container holding one coordinate.
    LatLongCoords(const LatLongCoord & coord) : coords()
    {
	coords.push_back(coord);
    }

    /** Unserialise a string and set this object to the coordinates in it.
     *
     *  @param serialised the string to unserialise the coordinates from.
     *
     *  @exception Xapian::SerialisationError if the string does not contain
     *  a valid serialised latitude-longitude pair, or contains junk at the end
     *  of it.
     */
    void unserialise(const std::string & serialised);

    /** Return a serialised form of the coordinate list.
     */
    std::string serialise() const;

    /// Return a string describing this object.
    std::string get_description() const;
};

/// Inequality test for LatLongCoordsIterator objects.
inline bool
operator!=(const LatLongCoordsIterator &a, const LatLongCoordsIterator &b)
{
    return !(a == b);
}

/** Base class for calculating distances between two lat/long coordinates.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 */
class XAPIAN_VISIBILITY_DEFAULT LatLongMetric {
  public:
    /// Destructor.
    virtual ~LatLongMetric();

    /** Return the distance between two coordinates, in metres.
     */
    virtual double pointwise_distance(const LatLongCoord & a,
				      const LatLongCoord & b) const = 0;

    /** Return the distance between two coordinate lists, in metres.
     *
     *  The distance between the coordinate lists is defined to be the minimum
     *  pairwise distance between coordinates in the lists.
     *
     *  @exception InvalidArgumentError either of the lists is empty.
     *
     *  @param a The first coordinate list.
     *  @param b The second coordinate list.
     */
    double operator()(const LatLongCoords & a, const LatLongCoords & b) const;

    /** Return the distance between two coordinate lists, in metres.
     *
     *  One of the coordinate lists is supplied in serialised form.
     *
     *  The distance between the coordinate lists is defined to be the minimum
     *  pairwise distance between coordinates in the lists.
     *
     *  @exception InvalidArgumentError either of the lists is empty.
     *
     *  @param a The first coordinate list.
     *  @param b The second coordinate list, in serialised form.
     */
    double operator()(const LatLongCoords & a, const std::string & b) const
    {
	return (*this)(a, b.data(), b.size());
    }

    /** Return the distance between two coordinate lists, in metres.
     *
     *  One of the coordinate lists is supplied in serialised form.
     *
     *  The distance between the coordinate lists is defined to be the minimum
     *  pairwise distance between coordinates in the lists.
     *
     *  @exception InvalidArgumentError either of the lists is empty.
     *
     *  @param a The first coordinate list.
     *  @param b_ptr The start of the serialised form of the second coordinate
     *               list.
     *  @param b_len The length of the serialised form of the second coordinate
     *               list.
     */
    double operator()(const LatLongCoords & a,
		      const char * b_ptr, size_t b_len) const;

    /** Clone the metric. */
    virtual LatLongMetric * clone() const = 0;

    /** Return the full name of the metric.
     *
     *  This is used when serialising and unserialising metrics; for example,
     *  for performing remote searches.
     *
     *  If the subclass is in a C++ namespace, the namespace should be included
     *  in the name, using "::" as a separator.  For example, for a
     *  LatLongMetric subclass called "FooLatLongMetric" in the "Xapian"
     *  namespace the result of this call should be "Xapian::FooLatLongMetric".
     */
    virtual std::string name() const = 0;

    /** Serialise object parameters into a string.
     *
     *  The serialised parameters should represent the configuration of the
     *  metric.
     */
    virtual std::string serialise() const = 0;

    /** Create object given string serialisation returned by serialise().
     *
     *  @param serialised A serialised instance of this LatLongMetric subclass.
     */
    virtual LatLongMetric * unserialise(const std::string & serialised) const = 0;
};

/** Calculate the great-circle distance between two coordinates on a sphere.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 *
 *  This uses the haversine formula to calculate the distance.  Note that this
 *  formula is subject to inaccuracy due to numerical errors for coordinates on
 *  the opposite side of the sphere.
 *
 *  See https://en.wikipedia.org/wiki/Haversine_formula
 */
class XAPIAN_VISIBILITY_DEFAULT GreatCircleMetric : public LatLongMetric {
    /** The radius of the sphere in metres.
     */
    double radius;

  public:
    /** Construct a GreatCircleMetric.
     *
     *  The (quadratic mean) radius of the Earth will be used by this
     *  calculator.
     */
    GreatCircleMetric();

    /** Construct a GreatCircleMetric using a specified radius.
     *
     *  This is useful for data sets in which the points are not on Earth (eg,
     *  a database of features on Mars).
     *
     *  @param radius_ The radius of the sphere to use, in metres.
     */
    explicit GreatCircleMetric(double radius_);

    /** Return the great-circle distance between points on the sphere.
     */
    double pointwise_distance(const LatLongCoord & a,
			      const LatLongCoord &b) const;

    LatLongMetric * clone() const;
    std::string name() const;
    std::string serialise() const;
    LatLongMetric * unserialise(const std::string & serialised) const;
};

/** Posting source which returns a weight based on geospatial distance.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 *
 *  Results are weighted by the distance from a fixed point, or list of points,
 *  calculated according to the metric supplied.  If multiple points are
 *  supplied (either in the constructor, or in the coordinates stored in a
 *  document), the closest pointwise distance is used.
 *
 *  Documents further away than a specified maximum range (or with no location
 *  stored in the specified slot) will not be returned.
 *
 *  The weight returned is computed from the distance using the formula:
 *
 *  k1 * pow(distance + k1, -k2)
 *
 *  (Where k1 and k2 are (strictly) positive, floating point constants, which
 *  default to 1000 and 1, respectively.  Distance is measured in metres, so
 *  this means that something at the centre gets a weight of 1.0, something 1km
 *  away gets a weight of 0.5, and something 3km away gets a weight of 0.25,
 *  etc)
 */
class XAPIAN_VISIBILITY_DEFAULT LatLongDistancePostingSource : public ValuePostingSource
{
    /// Current distance from centre.
    double dist;

    /// Centre, to compute distance from.
    LatLongCoords centre;

    /// Metric to compute the distance with.
    const LatLongMetric * metric;

    /// Maximum range to allow.  If set to 0, there is no maximum range.
    double max_range;

    /// Constant used in weighting function.
    double k1;

    /// Constant used in weighting function.
    double k2;

    /// Calculate the distance for the current document.
    void calc_distance();

    /// Internal constructor; used by clone() and serialise().
    LatLongDistancePostingSource(Xapian::valueno slot_,
				 const LatLongCoords & centre_,
				 const LatLongMetric * metric_,
				 double max_range_,
				 double k1_,
				 double k2_);

  public:
    /** Construct a new posting source which returns only documents within
     *  range of one of the central coordinates.
     *
     *  @param slot_ The value slot to read values from.
     *  @param centre_ The centre point to use for distance calculations.
     *  @param metric_ The metric to use for distance calculations.
     *  @param max_range_ The maximum distance for documents which are returned.
     *  @param k1_ The k1 constant to use in the weighting function.
     *  @param k2_ The k2 constant to use in the weighting function.
     */
    LatLongDistancePostingSource(Xapian::valueno slot_,
				 const LatLongCoords & centre_,
				 const LatLongMetric & metric_,
				 double max_range_ = 0.0,
				 double k1_ = 1000.0,
				 double k2_ = 1.0);

    /** Construct a new posting source which returns only documents within
     *  range of one of the central coordinates.
     *
     *  @param slot_ The value slot to read values from.
     *  @param centre_ The centre point to use for distance calculations.
     *  @param max_range_ The maximum distance for documents which are returned.
     *  @param k1_ The k1 constant to use in the weighting function.
     *  @param k2_ The k2 constant to use in the weighting function.
     *
     *  Xapian::GreatCircleMetric is used as the metric.
     */
    LatLongDistancePostingSource(Xapian::valueno slot_,
				 const LatLongCoords & centre_,
				 double max_range_ = 0.0,
				 double k1_ = 1000.0,
				 double k2_ = 1.0);
    ~LatLongDistancePostingSource();

    void next(double min_wt);
    void skip_to(Xapian::docid min_docid, double min_wt);
    bool check(Xapian::docid min_docid, double min_wt);

    double get_weight() const;
    LatLongDistancePostingSource * clone() const;
    std::string name() const;
    std::string serialise() const;
    LatLongDistancePostingSource *
	    unserialise_with_registry(const std::string &serialised,
				      const Registry & registry) const;
    void init(const Database & db_);

    std::string get_description() const;
};

/** KeyMaker subclass which sorts by distance from a latitude/longitude.
 *
 *  Experimental - see https://xapian.org/docs/deprecation#experimental-features
 *
 *  Results are ordered by the distance from a fixed point, or list of points,
 *  calculated according to the metric supplied.  If multiple points are
 *  supplied (either in the constructor, or in the coordinates stored in a
 *  document), the closest pointwise distance is used.
 *
 *  If a document contains no coordinate stored in the specified slot, a
 *  special value for the distance will be used.  This defaults to a large
 *  number, so that such results get a low rank, but may be specified by a
 *  constructor parameter.
 */
class XAPIAN_VISIBILITY_DEFAULT LatLongDistanceKeyMaker : public KeyMaker {

    /// The value slot to read.
    Xapian::valueno slot;

    /// The centre point (or points) for distance calculation.
    LatLongCoords centre;

    /// The metric to use when calculating distances.
    const LatLongMetric * metric;

    /// The default key to return, for documents with no value stored.
    std::string defkey;

  public:
    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		List of points to calculate distance from
     *				(closest distance is used).
     *  @param metric_		LatLongMetric to use.
     *  @param defdistance	Distance to use for docs with no value set.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoords & centre_,
			    const LatLongMetric & metric_,
			    double defdistance)
	    : slot(slot_),
	      centre(centre_),
	      metric(metric_.clone()),
	      defkey(sortable_serialise(defdistance))
    {}

    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		List of points to calculate distance from
     *				(closest distance is used).
     *  @param metric_		LatLongMetric to use.
     *
     *  Documents where no value is set are assumed to be a large distance
     *  away.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoords & centre_,
			    const LatLongMetric & metric_)
	    : slot(slot_),
	      centre(centre_),
	      metric(metric_.clone()),
	      defkey(9, '\xff')
    {}

    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		List of points to calculate distance from
     *				(closest distance is used).
     *
     *  Xapian::GreatCircleMetric is used as the metric.
     *
     *  Documents where no value is set are assumed to be a large distance
     *  away.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoords & centre_)
	    : slot(slot_),
	      centre(centre_),
	      metric(new Xapian::GreatCircleMetric()),
	      defkey(9, '\xff')
    {}

    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		Point to calculate distance from.
     *  @param metric_		LatLongMetric to use.
     *  @param defdistance	Distance to use for docs with no value set.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoord & centre_,
			    const LatLongMetric & metric_,
			    double defdistance)
	    : slot(slot_),
	      centre(),
	      metric(metric_.clone()),
	      defkey(sortable_serialise(defdistance))
    {
	centre.append(centre_);
    }

    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		Point to calculate distance from.
     *  @param metric_		LatLongMetric to use.
     *
     *  Documents where no value is set are assumed to be a large distance
     *  away.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoord & centre_,
			    const LatLongMetric & metric_)
	    : slot(slot_),
	      centre(),
	      metric(metric_.clone()),
	      defkey(9, '\xff')
    {
	centre.append(centre_);
    }

    /** Construct a LatLongDistanceKeyMaker.
     *
     *  @param slot_		Value slot to use.
     *  @param centre_		Point to calculate distance from.
     *
     *  Xapian::GreatCircleMetric is used as the metric.
     *
     *  Documents where no value is set are assumed to be a large distance
     *  away.
     */
    LatLongDistanceKeyMaker(Xapian::valueno slot_,
			    const LatLongCoord & centre_)
	    : slot(slot_),
	      centre(),
	      metric(new Xapian::GreatCircleMetric()),
	      defkey(9, '\xff')
    {
	centre.append(centre_);
    }

    ~LatLongDistanceKeyMaker();

    std::string operator()(const Xapian::Document & doc) const;
};

}

#endif /* XAPIAN_INCLUDED_GEOSPATIAL_H */
