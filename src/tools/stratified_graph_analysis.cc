/*
  This is a tool shipped by 'Aleph - A Library for Exploring Persistent
  Homology'.

  The purpose of this tool is to analyse stratified graphs or adjacency
  matrices of bipartite graphs in terms of persistent homology. To this
  end, the tool permits a selection of different filtrations and weight
  assignment strategies for vertices.

      Usage: stratified_graph_analysis [OPTIONS] FILES

      If no other options are given, the tool attempts to read a set of
      edge lists from each file and uses the standard weight filtration
      to calculate total persistence. These values will then be printed
      to `stdout`, following the convention `INDEX\tTOTAL_PERSISTENCE`,
      where `INDEX` refers to the index of the file parameter. Numerous
      options permit changing the way things are being calculated:

        --bipartite (-b): If set, attempts to read an adjacency matrix,
                          instead of reading edge lists. Normally, this
                          parameter is *not* required.

        --filtration (-f): Changes the filtration. Supported values are
                           "standard" for the standard weight-based one
                           and "absolute" for using absolute weights of
                           the edges for sorting

        --normalize (-n): Change the normalization strategy. Choose one
                          of "minmax", or "standardize", or leave empty
                          to default to "diagram" normalization.

                          - "diagram": scales all points in the diagram
                            between $0$ and $1$

                          - "minmax": scales all points in the graph to
                            satisfy $w \in [-1,+1]$

                          - "standardize": scales all weights to have a
                            mean of $0$ and a standard deviation of $1$

        --persistence-diagrams (-p): If, calculates persistence diagrams
                                     instead of only reporting the total
                                     persistence values.

        --reverse (-r): If set, reverses the filtration.

        --verbose (v): If set, adds an layer of verbosity to the output
                       so that debugging is simplified. This should not
                       be required normally.

        --weights (-w): Changes the strategy for setting vertex weights
                        and influencing the total persistence. Only two
                        valid settings exist, with "global" setting all
                        weights to the same value and "local" using the
                        first neighbour of a vertex to set the weight.
*/

#include <aleph/math/Statistics.hh>
#include <aleph/math/SymmetricMatrix.hh>

#include <aleph/persistenceDiagrams/Norms.hh>
#include <aleph/persistenceDiagrams/PersistenceDiagram.hh>

#include <aleph/persistentHomology/Calculation.hh>

#include <aleph/topology/Simplex.hh>
#include <aleph/topology/SimplicialComplex.hh>

#include <aleph/topology/filtrations/Data.hh>

#include <aleph/topology/io/BipartiteAdjacencyMatrix.hh>
#include <aleph/topology/io/EdgeLists.hh>

#include <cassert>
#include <cmath>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <getopt.h>

// These declarations should remain global because we have to refer to
// them in utility functions that are living outside of `main()`.
using DataType           = double;
using VertexType         = unsigned short;
using Simplex            = aleph::topology::Simplex<DataType, VertexType>;
using SimplicialComplex  = aleph::topology::SimplicialComplex<Simplex>;
using PersistenceDiagram = aleph::PersistenceDiagram<DataType>;
using Point              = typename PersistenceDiagram::Point;

PersistenceDiagram merge( const PersistenceDiagram& D, const PersistenceDiagram& E )
{
  PersistenceDiagram F;

  if( D.dimension() != F.dimension() )
    throw std::runtime_error( "Persistence diagram dimensions have to agree" );

  for( auto&& diagram : { D, E } )
    for( auto&& p : diagram )
      F.add( p.x(), p.y() );

  return F;
}

template
<
  class Engine,       // random engine to use for weight generation (e.g. std::default_random_engine)
  class Distribution  // distribution to use for weight generation (e.g. std::uniform_real_distribution)
>
SimplicialComplex makeRandomStratifiedGraph(
  const std::vector<unsigned>& strata,
  Engine& engine,
  Distribution& distribution
)
{
  auto n = strata.size();

  if( n <= 1 )
    throw std::runtime_error( "Invalid number of strata" );

  std::vector<Simplex> simplices;

  // Create vertices ---------------------------------------------------
  //
  // The `strata` vector contains the size of each stratum, so we just
  // have to add the correct number of vertices here.

  VertexType index = VertexType(0);
  for( auto&& stratum : strata )
  {
    for( unsigned i = 0; i < stratum; i++ )
      simplices.push_back( Simplex( index++ ) );
  }

  // Create edges ------------------------------------------------------
  //
  // Every stratum is connected to every other stratum, but there are no
  // connections *within* a given stratum.

  VertexType offset = VertexType(0);
  for( decltype(n) i = 0; i < n - 1; i++ )
  {
    // All vertices in the next stratum start with this offset to their
    // indices. It depends on the sum of all vertices in *all* previous
    // strata.
    offset += strata[i];

    for( unsigned j = 0; j < strata[i]; j++ )
    {
      for( unsigned k = 0; k < strata[i+1]; k++ )
      {
        simplices.push_back(
          Simplex(
            {
              VertexType( offset - strata[i] + j ),
              VertexType( offset             + k )
            },
            distribution( engine )
          )
        );
      }
    }
  }

  return SimplicialComplex( simplices.begin(), simplices.end() );
}

SimplicialComplex applyFiltration( const SimplicialComplex& K,
                                   const std::string& strategy,
                                   bool reverse = false )
{
  auto L = K;

  if( strategy == "standard" )
  {
    if( reverse )
    {
      L.sort(
        aleph::topology::filtrations::Data<Simplex, std::greater<DataType> >()
      );
    }
    else
    {
      L.sort(
        aleph::topology::filtrations::Data<Simplex, std::less<DataType> >()
      );
    }
  }
  else if( strategy == "absolute" )
  {
    if( reverse )
    {
      auto functor = [] ( const Simplex& s, const Simplex& t )
      {
        auto w1 = s.data();
        auto w2 = t.data();

        if( std::abs( w1 ) > std::abs( w2 ) )
          return true;
        else if( std::abs( w1 ) == std::abs( w2 ) )
        {
          // This amounts to saying that w1 is positive and w2 is
          // negative.
          if( w1 > w2 )
            return true;
          else
          {
            if( s.dimension() < t.dimension() )
              return true;

            // Absolute value is equal, signed value is equal, and the
            // dimension is equal. We thus have to fall back to merely
            // using the lexicographical order.
            else
              return s < t;
          }
        }

        return false;
      };

      L.sort( functor );
    }
    else
    {
      auto functor = [] ( const Simplex& s, const Simplex& t )
      {
        auto w1 = s.data();
        auto w2 = t.data();

        if( std::abs( w1 ) < std::abs( w2 ) )
          return true;
        else if( std::abs( w1 ) == std::abs( w2 ) )
        {
          // This amounts to saying that w1 is negative and w2 is
          // positive.
          if( w1 < w2 )
            return true;
          else
          {
            if( s.dimension() < t.dimension() )
              return true;

            // Absolute value is equal, signed value is equal, and the
            // dimension is equal. We thus have to fall back to merely
            // using the lexicographical order.
            else
              return s < t;
          }
        }

        return false;
      };

      L.sort( functor );
    }
  }

  return L;
}

DataType getMinimumAbsoluteWeight( const SimplicialComplex& K )
{
  DataType result = std::numeric_limits<DataType>::max();

  // Again, only need to consider $1$-dimensional simplices, i.e. edges
  // here, because their weights are guarantee to be salient.
  for( auto&& s : K )
  {
    if( s.dimension() == 1 )
      result = std::min( result, std::abs( s.data() ) );
  }

  return result;
}

template <class T> T min_abs( T a, T b )
{
  if( std::abs( a ) < std::abs( b ) )
    return a;
  else
    return b;
}

template <class T> T max_abs( T a, T b )
{
  if( std::abs( a ) > std::abs( b ) )
    return a;
  else
    return b;
}

SimplicialComplex assignVertexWeights( const SimplicialComplex& K,
                                       const std::string& filtration,
                                       const std::string& strategy,
                                       bool reverse = false )
{
  DataType minData = std::numeric_limits<DataType>::max();
  DataType maxData = std::numeric_limits<DataType>::lowest();

  for( auto&& s : K )
  {
    if( s.dimension() != 1 )
      continue;

    if( filtration == "standard" )
    {
      minData = std::min( minData, s.data() );
      maxData = std::max( maxData, s.data() );
    }
    else if( filtration == "absolute" )
    {
      if( minData == std::numeric_limits<DataType>::max() )
        minData = s.data();

      if( maxData == std::numeric_limits<DataType>::lowest() )
        maxData = s.data();

      minData = min_abs( minData, s.data() );
      maxData = max_abs( maxData, s.data() );
    }
  }

  // Setting up the weights --------------------------------------------
  //
  // This function assumes that the simplicial complex is already in
  // filtration ordering with respect to its weights. Hence, we only
  // have to take the *first* weight that we encounter (when using a
  // global vertex weight assignment) or the *extremal* value, which
  // is either a minimum or a maximum depending on the direction.

  std::unordered_map<VertexType, DataType> weight;

  for( auto&& s : K )
  {
    if( s.dimension() != 1 )
      continue;

    auto u     = s[0];
    auto v     = s[1];
    DataType w = DataType(); // weight to assign; depends on filtration

    // Assign the global minimum or maximum. This is rather wasteful
    // because the values do not change, but at least the code makes
    // it clear that all updates are done in the same place.
    if( strategy == "global" )
      w = reverse ? maxData : minData;
    else if( strategy == "local" )
      w = s.data();
    else
      throw std::runtime_error( "Unknown update strategy '" + strategy + "'" );

    // This only performs the update *once*.
    weight.insert( {u,w} );
    weight.insert( {v,w} );
  }

  // Assign the weights ------------------------------------------------
  //
  // Having set up the map of weights, we now only need to traverse it
  // in order to assign weights afterwards.

  auto L = K;

  for( auto it = L.begin(); it != L.end(); ++it )
  {
    if( it->dimension() == 0 )
    {
      auto s = *it;  // simplex
      auto v = s[0]; // vertex

      s.setData( weight.at(v) );

      auto result = L.replace( it, s );
      if( !result )
        throw std::runtime_error( "Unable to replace simplex in simplicial complex" );
    }
  }

  return L;
}

void normalizeSimplicialComplex( SimplicialComplex& K, const std::string& normalization )
{
  if( normalization.empty() )
    return;

  // Only collect weights of 1-dimensional simplices, i.e. edges,
  // because those are *guaranteed* not to have been changed, and
  // we want to work with the original weights.
  std::vector<DataType> weights;
  for( auto&& s : K )
  {
    if( s.dimension() == 1 )
      weights.push_back( s.data() );
  }

  if( weights.empty() )
    return;

  auto minmax = std::minmax_element( weights.begin(), weights.end() );
  auto mu     = aleph::math::sampleMean( weights.begin(), weights.end() );
  auto sigma  = aleph::math::sampleStandardDeviation( weights.begin(), weights.end() );

  assert( *minmax.first != *minmax.second );

  for( auto it = K.begin(); it != K.end(); ++it )
  {
    auto s = *it;
    auto w = s.data();

    if( normalization == "abs" )
      w = w / std::max( std::abs( *minmax.first ), std::abs( *minmax.second ) );
    else if( normalization == "minmax" )
    {
      w = ( w - *minmax.first ) / ( *minmax.second - *minmax.first ); // [ 0, 1]
      w = 2*w;                                                        // [ 0, 2]
      w = w - 1;                                                      // [-1,+1]
    }
    else if( normalization == "standardize" )
      w = (w - mu) / sigma;
    else
      throw std::runtime_error( "Unknown normalization strategy '" + normalization + "'" );

    s.setData( w );
    bool success = K.replace( it, s );

    assert( success );
  }
}

template <class Reader> std::vector<SimplicialComplex> loadSimplicialComplexes(
  int argc, char** argv,
  const std::string& normalization )
{
  Reader reader;

  std::vector<SimplicialComplex> simplicialComplexes;
  simplicialComplexes.reserve( static_cast<std::size_t>( argc - optind ) );

  for( int i = optind; i < argc; i++ )
  {
    auto filename = std::string( argv[i] );

    std::cerr << "* Processing " << filename << "...";

    SimplicialComplex K;
    reader( filename, K );

    std::cerr << "finished\n";

    normalizeSimplicialComplex( K, normalization );
    simplicialComplexes.emplace_back( K );
  }

  return simplicialComplexes;
}

int main( int argc, char** argv )
{
  bool bipartite             = false;
  bool normalize             = false;
  bool reverse               = false;
  bool verbose               = false;
  bool calculateDiagrams     = false;

  // The default filtration sorts simplices by their weights. Negative
  // weights are treated as being less relevant than positive ones.
  std::string filtration = "standard";

  // Defines how the minimum value for the vertices is to be set. Valid
  // options include:
  //
  //  - global (uses the global extremal value)
  //  - local  (uses the local  extremal value over all neighbours)
  std::string weights = "global";

  // By default, only the output persistence diagram will be normalized,
  // but the user can change this:
  //
  //   - abs         (scales all weights in the graph by dividing by the absolute value of its weights)
  //   - minmax      (scales all weights in the graph to $[-1,+1]$
  //   - standardize (scales all weights to have a mean of $0$ and standard deviation of $1)
  std::string normalization;

  {
    static option commandLineOptions[] =
    {
      { "bipartite"           , no_argument,       nullptr, 'b' },
      { "default"             , no_argument,       nullptr, 'd' },
      { "persistence-diagrams", no_argument,       nullptr, 'p' },
      { "reverse"             , no_argument,       nullptr, 'r' },
      { "verbose"             , no_argument,       nullptr, 'v' },
      { "normalize"           , optional_argument, nullptr, 'n' },
      { "filtration"          , required_argument, nullptr, 'f' },
      { "weights"             , required_argument, nullptr, 'w' },
      { nullptr               , 0                , nullptr,  0  }
    };

    int option = 0;
    while( ( option = getopt_long( argc, argv, "bdprtvf:w:n::", commandLineOptions, nullptr ) ) != -1 )
    {
      switch( option )
      {
      case 'b':
        bipartite = true;
        break;
      case 'd':
        std::cerr << "* Applying default arguments\n";

        filtration    = "absolute";
        normalize     = true;
        normalization = "abs";
        reverse       = true;
        weights       = "global";

        break;
      case 'f':
        filtration = optarg;
        break;
      case 'n':
        normalize = true;

        if( optarg != nullptr )
          normalization = optarg;

        break;
      case 'p':
        calculateDiagrams = true;
        break;
      case 'r':
        reverse = true;
        break;
      case 'v':
        verbose = true;
        break;
      case 'w':
        weights = optarg;
        break;
      default:
        throw std::runtime_error( "Unknown or incorrect command-line argument encountered" );
        break;
      }
    }

    // Check filtration validity ---------------------------------------

    if(    filtration != "absolute"
        && filtration != "standard" )
    {
      std::cerr << "* Invalid filtration value '" << filtration << "', so falling back to standard one\n";
      filtration = "standard";
    }

    // Check validity of weight strategy -------------------------------

    if(    weights != "global"
        && weights != "local" )
    {
      std::cerr << "* Invalid weight strategy value '" << weights << "', so falling back to global one\n";
      weights = "global";
    }
  }

  // Be verbose about parameters ---------------------------------------

  if( bipartite )
    std::cerr << "* Mode: reading bipartite adjacency matrices\n";
  else
    std::cerr << "* Mode: reading edge lists\n";

  std::cerr << "* Filtration: " << filtration
            << " (" << ( reverse ? "" : "not " ) << "reversed" << ")\n"
            << "* Vertex weight assignment strategy: " << weights << "\n"
            << "* Normalization strategy: " << normalization << "\n";

  if( verbose )
    std::cerr << "* Verbose output\n";

  // 1. Read simplicial complexes --------------------------------------

  std::vector<SimplicialComplex> simplicialComplexes;
  simplicialComplexes.reserve( static_cast<unsigned>( argc - optind - 1 ) );

  if( argc - optind >= 1 )
  {
    if( bipartite )
    {
      using Reader = aleph::topology::io::BipartiteAdjacencyMatrixReader;

      simplicialComplexes
        = loadSimplicialComplexes<Reader>( argc, argv, normalization );
    }
    else
    {
      using Reader = aleph::topology::io::EdgeListReader;

      simplicialComplexes
        = loadSimplicialComplexes<Reader>( argc, argv, normalization );
    }
  }
  else
  {
    std::default_random_engine engine;
    engine.seed(
      static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()
      )
    );

    DataType minWeight = DataType(-1);
    DataType maxWeight = DataType( 1);

    std::uniform_real_distribution<DataType> distribution(
      minWeight,
      std::nextafter( maxWeight, std::numeric_limits<DataType>::max() )
    );

    for( unsigned i = 0; i < 1e3; i++ )
    {
      auto K
        = makeRandomStratifiedGraph( {2,3}, // FIXME: {2,3,1} for the complete network
                                     engine,
                                     distribution
      );

      simplicialComplexes.emplace_back( K );
    }
  }

  // Establish filtration order ----------------------------------------

  for( auto&& K : simplicialComplexes )
  {
    K = applyFiltration( K, filtration, reverse );
    K = assignVertexWeights( K, filtration, weights, reverse );
    K = applyFiltration( K, filtration, reverse );

    if( verbose )
      std::cerr << K << "\n";
  }

  // 2. Calculate persistent homology ----------------------------------

  for( std::size_t i = 0; i < simplicialComplexes.size(); i++ )
  {
    // The persistence diagram that will be used in the subsequent
    // analysis. This does not necessarily have to stem from data,
    // but can be calculated from a suitable transformation.
    PersistenceDiagram D;

    auto&& K = simplicialComplexes[i];
    auto diagrams = aleph::calculatePersistenceDiagrams( K );
    D             = diagrams.back(); // Use the *last* diagram of the filtration so that
                                     // we get features in the highest dimension.

    D.removeDiagonal();

    auto minAbsoluteWeight = getMinimumAbsoluteWeight( K );

    std::transform( D.begin(), D.end(), D.begin(),
      [&minAbsoluteWeight] ( const Point& p )
      {
        if( p.isUnpaired() )
          return Point( p.x(), minAbsoluteWeight );
        else
          return Point( p );
      }
    );

    if( filtration == "absolute" )
    {
      std::transform( D.begin(), D.end(), D.begin(),
        [] ( const Point& p )
        {
          auto x = p.x();
          auto y = p.y();

          return Point( std::abs(x), std::abs(y) );
        }
      );
    }

    if( normalize && normalization.empty() )
    {
      DataType minData = std::numeric_limits<DataType>::max();
      DataType maxData = std::numeric_limits<DataType>::lowest();

      for( auto&& s : K )
      {
        minData = std::min( minData, s.data() );
        maxData = std::max( maxData, s.data() );
      }

      // Ensures that all weights are in [0:1] for the corresponding
      // diagram. This enables the comparison of time-varying graphs
      // or different instances.
      std::transform( D.begin(), D.end(), D.begin(),
        [&minData, &maxData] ( const Point& p )
        {
          auto x = p.x();
          auto y = p.y();

          if( minData != maxData )
          {
            x = (x - minData) / (maxData - minData);
            y = (y - minData) / (maxData - minData);
          }

          return Point( x,y );
        }
      );
    }

    // Determine mode of operation -------------------------------------
    //
    // Several modes of operation exist for this program. They can be
    // set using the flags specified above. At present, the following
    // operations are possible:
    //
    // - Calculate persistence diagrams
    // - Calculate 2-norm of the persistence diagrams

    if( calculateDiagrams )
      std::cout << D << "\n\n";
    else
      std::cout << i << "\t" << aleph::pNorm( D ) << "\n";
  }
}
