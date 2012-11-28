//#####################################################################
// Class TriangleSubdivision
//#####################################################################
#include <other/core/mesh/TriangleSubdivision.h>
#include <other/core/mesh/SegmentMesh.h>
#include <other/core/array/Array2d.h>
#include <other/core/array/NdArray.h>
#include <other/core/array/view.h>
#include <other/core/array/IndirectArray.h>
#include <other/core/math/constants.h>
#include <other/core/math/cube.h>
#include <other/core/python/Class.h>
#include <other/core/structure/Hashtable.h>
#include <other/core/utility/tr1.h>
#include <other/core/vector/SparseMatrix.h>
namespace other{

typedef real T;
OTHER_DEFINE_TYPE(TriangleSubdivision)

static Ref<TriangleMesh> make_fine_mesh(const TriangleMesh& coarse_mesh) {
  Ref<const SegmentMesh> segments=coarse_mesh.segment_mesh();
  NestedArray<const int> incident_elements=segments->incident_elements();
  int offset = coarse_mesh.nodes();
  Array<Vector<int,3> > triangles(4*coarse_mesh.elements.size(),false);
  for (int t=0;t<coarse_mesh.elements.size();t++) {
    Vector<int,3> nodes = coarse_mesh.elements[t];
    Vector<int,3> edges;
    for (int a=0;a<3;a++) {
      int start=nodes[a],end=nodes[(a+1)%3];
      RawArray<const int> incident = incident_elements[start];
      for (int i=0;i<incident.size();i++)
        if (segments->elements[incident[i]].contains(end)) {
          edges[a]=offset+incident[i];
          break;
        }
    }
    triangles[4*t+0].set(nodes[0],edges[0],edges[2]);
    triangles[4*t+1].set(edges[0],nodes[1],edges[1]);
    triangles[4*t+2].set(edges[2],edges[1],nodes[2]);
    triangles[4*t+3].set(edges[0],edges[1],edges[2]);
  }
  return new_<TriangleMesh>(triangles);
}

TriangleSubdivision::TriangleSubdivision(const TriangleMesh& coarse_mesh)
  : coarse_mesh(ref(coarse_mesh))
  , fine_mesh(make_fine_mesh(coarse_mesh)) {}

TriangleSubdivision::~TriangleSubdivision() {}

template<class TV> Array<TV> TriangleSubdivision::linear_subdivide(RawArray<const TV> X) const {
  typedef typename ScalarPolicy<TV>::type T;
  int offset = coarse_mesh->nodes();
  OTHER_ASSERT(X.size()==offset);
  Ref<const SegmentMesh> segments = coarse_mesh->segment_mesh();
  Array<TV> fine_X(offset+segments->elements.size(),false);
  fine_X.slice(0,offset) = X;
  for (int s=0;s<segments->elements.size();s++) {
    int i,j;segments->elements[s].get(i,j);
    fine_X[offset+s]=(T).5*(X[i]+X[j]);
  }
  return fine_X;
}

Array<T,2> TriangleSubdivision::linear_subdivide(RawArray<const T,2> X) const {
  int offset = coarse_mesh->nodes();
  OTHER_ASSERT(X.m==offset);
  Ref<const SegmentMesh> segments = coarse_mesh->segment_mesh();
  Array<T,2> fine_X(offset+segments->elements.size(),X.n,false);
  fine_X.slice(0,offset) = X;
  for (int s=0;s<segments->elements.size();s++) {
    int i,j;segments->elements[s].get(i,j);
    for (int a=0;a<X.n;a++)
      fine_X(offset+s,a) = (T).5*(X(i,a)+X(j,a));
  }
  return fine_X;
}

NdArray<T> TriangleSubdivision::linear_subdivide_python(NdArray<const T> X) const {
  if(X.rank()==1)
    return linear_subdivide(RawArray<const T>(X));
  else if(X.rank()==2) {
    switch(X.shape[1]) {
      case 1: return linear_subdivide(RawArray<const T>(X));
      case 2: return linear_subdivide(vector_view<2>(X.flat));
      case 3: return linear_subdivide(vector_view<3>(X.flat));
      default: return linear_subdivide(RawArray<const T,2>(X));
    }
  } else
    OTHER_FATAL_ERROR("expected rank 1 or 2");
}

template Array<T> TriangleSubdivision::linear_subdivide(RawArray<const T>) const;
template Array<Vector<T,2> > TriangleSubdivision::linear_subdivide(RawArray<const Vector<T,2> >) const;
template Array<Vector<T,3> > TriangleSubdivision::linear_subdivide(RawArray<const Vector<T,3> >) const;
template Array<Vector<T,4> > TriangleSubdivision::linear_subdivide(RawArray<const Vector<T,4> >) const;

static inline T new_loop_alpha(int degree) {
  // Generated by loop-helper script
  static const double alpha[10] = {0.59635416666666663,0.7957589285714286,0.4375,0.5,0.54546609462891005,0.625,0.62427255647332092,0.62242088005687379,0.62007316864426665,0.61765326579615698};
  if ((unsigned)(degree-1)<10)
    return alpha[degree-1];
  T lambda=(T).375+(T).25*cos(T(2*pi)/degree);
  return 1-lambda*(4+lambda*(5*lambda-8))/(2*(1-lambda))+sqr(lambda);
}

static inline T new_loop_beta(int degree) {
  // Generated by loop-helper script
  static const double beta[10] = {0.79427083333333337,0.21986607142857142,0.625,0.640625,0.65906781074217002,0.625,0.65755300218905677,0.68203664141560383,0.70086166999263333,0.7155692017233628};
  if ((unsigned)(degree-1)<10)
    return beta[degree-1];
  T lambda=(T).375+(T).25*cos((T)(2*pi)/degree);
  return lambda*(4+lambda*(5*lambda-8))/(2*(1-lambda));
}

static inline T new_loop_weight(int degree,int j) {
  // Generated by loop-helper script
  assert((unsigned)j<(unsigned)degree);
  static const double weights[8][11] = {{0.375,0.12500000000000003,0.12499999999999992},{0.3828125,0.125,0.0078125,0.12499999999999997},{0.39452882373436315,0.12152669943749474,0.010742794066409203,0.010742794066409215,0.12152669943749467},{0.375,0.125,2.0543252740130525e-33,0,8.217301096052199e-33,0.12499999999999978},{0.34891546271823215,0.15006402219953882,0.0018402318203779792,0.0024145157154956943,0.0024145157154956925,0.0018402318203779731,0.15006402219953874},{0.32273792109013977,0.16623201537498475,0.009140199808831068,0.0042771449789161626,0,0.0042771449789161644,0.0091401998088310541,0.16623201537498475},{0.29838474383698516,0.17504577596148707,0.021066016880964596,0.0025694273647703116,0.0025572428706021103,0.0025572428706021064,0.0025694273647703229,0.021066016880964544,0.17504577596148699},{0.2764028273524804,0.17852258978956867,0.034911074051062363,0.00036971064127200895,0.0057798127035380894,0,0.0057798127035380877,0.0003697106412720109,0.034911074051062328,0.17852258978956859}};
  if ((unsigned)(degree-3)<8)
    return weights[degree-3][j];
  T u = cos(T(2*pi)/degree*j);
  T lambda=(T).375+(T).25*cos(T(2*pi)/degree);
  return 2*cube(lambda)/(degree*(1-lambda))*(1+u)*sqr(1/lambda-(T)1.5+u);
}

Ref<SparseMatrix> TriangleSubdivision::loop_matrix() const {
  if (loop_matrix_)
    return ref(loop_matrix_);
  // Build matrix of Loop subdivision weights
  Hashtable<Vector<int,2>,T> A;
  int offset = coarse_mesh->nodes();
  RawArray<const Vector<int,2> > segments = coarse_mesh->segment_mesh()->elements;
  NestedArray<const int> neighbors = coarse_mesh->sorted_neighbors();
  NestedArray<const int> boundary_neighbors = coarse_mesh->boundary_mesh()->neighbors();
  unordered_set<int> corners_set(corners.begin(),corners.end());
  // Fill in node weights
  for (int i=0;i<offset;i++){
    if (!neighbors.valid(i) || !neighbors.size(i) || corners_set.count(i) || (boundary_neighbors.valid(i) && boundary_neighbors.size(i) && boundary_neighbors.size(i)!=2))
      A.set(vec(i,i),1);
    else if (boundary_neighbors.valid(i) && boundary_neighbors.size(i)==2) { // Regular boundary node
      A.set(vec(i,i),(T).75);
      for (int a=0;a<2;a++)
        A.set(vec(i,boundary_neighbors(i,a)),(T).125);
    } else { // Interior node
      RawArray<const int> ni = neighbors[i];
      T alpha = new_loop_alpha(ni.size());
      A.set(vec(i,i),alpha);
      T other = (1-alpha)/ni.size();
      for (int j : ni)
        A.set(vec(i,j),other);
    }
  }
  // Fill in edge values
  for (int s=0;s<segments.size();s++) {
    int i = offset+s;
    Vector<int,2> e = segments[s];
    RawArray<const int> n[2] = {neighbors.valid(e[0])?neighbors[e[0]]:RawArray<const int>(),
                                neighbors.valid(e[1])?neighbors[e[1]]:RawArray<const int>()};
    if (boundary_neighbors.valid(e[0]) && boundary_neighbors.valid(e[1]) && boundary_neighbors.size(e[0]) && boundary_neighbors.size(e[1]) && boundary_neighbors[e[0]].contains(e[1])) // Boundary edge
      for (int a=0;a<2;a++)
        A.set(vec(i,e[a]),(T).5);
    else if (n[0].size()==6 && n[1].size()==6) { // Edge between regular vertices
      int j = n[0].find(e[1]); 
      int c[2] = {n[0][(j-1+n[0].size())%n[0].size()],
                  n[0][(j+1)%n[0].size()]};
      for (int a=0;a<2;a++) {
        A.set(vec(i,e[a]),(T).375);
        A.set(vec(i,c[a]),(T).125);
      }
    } else { // Edge between one or two irregular vertices
      T factor = n[0].size()!=6 && n[1].size()!=6 ?.5:1;
      for (int k=0;k<2;k++)
        if (n[k].size()!=6) {
          A[vec(i,e[k])] += factor*(1-new_loop_beta(n[k].size()));
          int start = n[k].find(e[1-k]);
          for (int j=0;j<n[k].size();j++)
            A[vec(i,n[k][(start+j)%n[k].size()])] += factor*new_loop_weight(n[k].size(),j);
        }
    }
  }
  // Finalize construction
  loop_matrix_ = new_<SparseMatrix>(A,vec(offset+segments.size(),offset));
  return ref(loop_matrix_);
}

template<class TV> Array<TV> TriangleSubdivision::loop_subdivide(RawArray<const TV> X) const {
  OTHER_ASSERT(X.size()==coarse_mesh->nodes());
  Array<TV> fine_X(fine_mesh->nodes(),false);
  loop_matrix()->multiply(X,fine_X);
  return fine_X;
}

NdArray<T> TriangleSubdivision::loop_subdivide_python(NdArray<const T> X) const {
  if(X.rank()==1)
    return loop_subdivide(RawArray<const T>(X));
  else if(X.rank()==2) {
    switch(X.shape[1]) {
      case 1: return loop_subdivide(RawArray<const T>(X));
      case 2: return loop_subdivide(vector_view<2>(X.flat));
      case 3: return loop_subdivide(vector_view<3>(X.flat));
      default: OTHER_NOT_IMPLEMENTED("general size vectors");
    }
  } else
    OTHER_FATAL_ERROR("expected rank 1 or 2");
}

}
using namespace other;

void wrap_triangle_subdivision() {
  typedef TriangleSubdivision Self;
  Class<Self>("TriangleSubdivision")
    .OTHER_INIT(TriangleMesh&)
    .OTHER_FIELD(coarse_mesh)
    .OTHER_FIELD(fine_mesh)
    .OTHER_FIELD(corners)
    .OTHER_METHOD_2("linear_subdivide",linear_subdivide_python)
    .OTHER_METHOD_2("loop_subdivide",loop_subdivide_python)
    ;
}
