#ifndef ALEPH_GEOMETRY_POINT_HH__
#define ALEPH_GEOMETRY_POINT_HH__

#include <algorithm>
#include <initializer_list>
#include <ostream>
#include <vector>

namespace aleph
{

namespace geometry
{

/**
  @class Point
  @brief Basic point class of arbitrary dimensionality

  This is a simple container class for representing points of arbitrary
  dimensionality. It can be used within some classes, such as the cover
  tree class, to represent data points.

  @see CoverTree
*/

template <class T> class Point
{
public:

  // Typedefs ----------------------------------------------------------

  using Container = std::vector<T>;

  // STL-like definitions because they make it easier to use the class
  // with standard algorithms.
  using const_iterator = typename Container::const_iterator;
  using iterator       = typename Container::iterator;
  using value_type     = T;

  // Constructors ------------------------------------------------------

  template <class InputIterator> Point( InputIterator begin, InputIterator end )
    : _data( begin, end )
  {
  }

  Point( std::initializer_list<T> data )
    : _data( data.begin(), data.end() )
  {
  }

  // Iterators ---------------------------------------------------------

  const_iterator begin() const noexcept { return _data.begin(); }
  iterator       begin()       noexcept { return _data.begin(); }

  const_iterator end()   const noexcept { return _data.end();   }
  iterator       end()         noexcept { return _data.end();   }

  // Attributes --------------------------------------------------------

  /** @returns Dimension of the point */
  std::size_t dimension() const noexcept
  {
    return _data.size();
  }

  /**
    @returns Dimension of the point; provided to be compatible with STL
    methods that are unaware of the `dimension` attribute.
  */

  std::size_t size() const noexcept
  {
    return _data.size();
  }

  // Operators ---------------------------------------------------------

  /** Checks all coordinates of two points for equality */
  bool operator==( const Point& other ) const noexcept
  {
    return std::equal( _data.begin(), _data.end(), other._data.begin() );
  }

  /** Checks whether at least one coordinate of the two points differs */
  bool operator!=( const Point& other ) const noexcept
  {
    return !this->operator==( other );
  }

  /**
    Performs a lexicographical comparison of two points. This amounts to
    a strict weak ordering as long as the dimensionality coincides.
  */

  bool operator<( const Point& other ) const noexcept
  {
    return std::lexicographical_compare(
      _data.begin(), _data.end(),
      other._data.begin(), other._data.end()
    );
  }

private:

  /** Stores the individual coordinates of the point */
  Container _data;
};

template <class T> std::ostream& operator<<( std::ostream& o, const Point<T>& p )
{
  for( auto it = p.begin(); it != p.end(); ++it )
  {
    if( it != p.begin() )
      o << " ";

    o << *it;
  }

  return o;
}

} // namespace geometry

} // namespace aleph

#endif
