#ifndef ALEPH_VECTOR_HH__
#define ALEPH_VECTOR_HH__

#include <algorithm>
#include <utility>
#include <vector>

namespace aleph
{

namespace representations
{

template <class IndexType = unsigned> class Vector
{
public:
  using Index = IndexType;

  void setNumColumns( Index numColumns )
  {
    _data.resize( numColumns );
    _dimensions.resize( numColumns );
  }

  Index getNumColumns() const
  {
    return static_cast<Index>( _data.size() );
  }

  std::pair<Index, bool> getMaximumIndex( Index column ) const
  {
    if( _data.at( column ).empty() )
      return std::make_pair( Index(0), false );
    else
      return std::make_pair( _data.at( column ).back(), true );
  }

  void addColumns( Index source, Index target )
  {
    auto&& sourceColumn = _data.at( source );
    auto&& targetColumn = _data.at( target );

    std::vector<Index> result;
    result.reserve( sourceColumn.size() + targetColumn.size() );

    std::set_symmetric_difference( sourceColumn.begin(), sourceColumn.end(),
                                   targetColumn.begin(), targetColumn.end(),
                                   std::back_inserter( result ) );

    targetColumn.swap( result );
  }

  template <class InputIterator> void setColumn( Index column,
                                                 InputIterator begin, InputIterator end )
  {
    _data.at( column ).assign( begin, end );

    // Ensures proper sorting order. Else, the reduction algorithm will
    // not be able to reduce the matrix.
    std::sort( _data.at( column ).begin(), _data.at( column ).end() );

    // Upon initialization, the column must by necessity have the dimension
    // that is indicated by the amount of indices in its boundary. The case
    // of 0-simplices needs special handling.
    _dimensions.at( column )
        = begin == end ? 0
                       : static_cast<Index>( std::distance( begin, end ) - 1 );
  }

  std::vector<Index> getColumn( Index column ) const
  {
    return _data.at( column );
  }

  void clearColumn( Index column )
  {
    _data.at( column ).clear();
  }

  Index getDimension( Index column ) const
  {
    return _dimensions.at( column );
  }

  Index getDimension() const
  {
    if( _dimensions.empty() )
      return Index(0);
    else
      return *std::max_element( _dimensions.begin(), _dimensions.end() );
  }

  void setDualized( bool value )
  {
    _isDualized = value;
  }

  bool isDualized() const
  {
    return _isDualized;
  }

private:
  std::vector< std::vector<Index> > _data;
  std::vector<Index> _dimensions;

  /**
    Flag indicating whether the matrix is dualized or not. By default
    no matrix is dualized. This flag is used by some of the reduction
    to determine how to calculate indices.
  */

  bool _isDualized = false;
};

}

}

#endif
