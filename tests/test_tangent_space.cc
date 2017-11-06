#include <tests/Base.hh>

#include <aleph/containers/PointCloud.hh>

#include <aleph/geometry/TangentSpace.hh>

#include <iostream>

using namespace aleph;
using namespace containers;
using namespace geometry;

template <class T> void test()
{
#ifdef ALEPH_WITH_EIGEN
  using PointCloud = PointCloud<T>;

  PointCloud pc = load<T>( CMAKE_SOURCE_DIR + std::string( "/tests/input/Iris_colon_separated.txt" ) );

  TangentSpace ts;
  ts( pc );
#endif
}

int main( int, char** )
{
  test<float> ();
  test<double>();
}