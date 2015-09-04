/*
//@HEADER
// ************************************************************************
// 
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
// 
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
// 
// ************************************************************************
//@HEADER
*/

#include<cmath>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<sys/time.h>
// Include Kokkos Headers
#include<Kokkos_Core.hpp>

// Struct to do arbitrary reduction by overloading += Operator
struct MinFinder {
  // The value for which the minimum is to be found
  double value;
  // The index associated with the value
  int index;
  KOKKOS_INLINE_FUNCTION
  MinFinder():
    value(1.0e200),index(-1) {}

  KOKKOS_INLINE_FUNCTION
  MinFinder(const MinFinder& f):
    value(f.value),index(f.index) {}

  KOKKOS_INLINE_FUNCTION
  MinFinder(const double& value_, const int& index_):
    value(value_),index(index_) {}

  // We need volatile assignment operator for Kokkos reductions
  KOKKOS_INLINE_FUNCTION
  void operator = (const volatile MinFinder& f) volatile {
    value = f.value;
    index = f.index;
  }
  // We overload the += operator to do the custom reduction
  // The operator and its argument has to be marked volatile for Kokkos reductions
  KOKKOS_INLINE_FUNCTION
  void operator += (const volatile MinFinder& f) volatile {
    if( f.value < value ) {
      value = f.value;
      index = f.index;
    }
  }
};

int main(int argc, char* argv[]) {

  // Setting default parameters
  int num_points = 100000;  // number of vectors
  int nrepeat = 10; // number of times test is repeated

  // Read command line arguments
  for(int i=0; i<argc; i++) {
           if( (strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "-num_points") == 0)) {
      num_points = atoi(argv[++i]);
    } else if( strcmp(argv[i], "-nrepeat") == 0) {
      nrepeat = atoi(argv[++i]);
    } else if( (strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "-help") == 0)) {
      printf("Nearest Point Options:\n");
      printf("  -num_points (-p)  <int>: number of points (default: 100000)\n");
      printf("  -nrepeat <int>:          number of test invocations (default: 10)\n");
      printf("  -help (-h):              print this message\n");
    }
  }

  //Initialize Kokkos
  Kokkos::initialize(argc,argv);

  // Allocate data arrays
  Kokkos::View<double*[3]> points("Points",num_points); // the point coordinates
  typename Kokkos::View<double*[3]>::HostMirror
    h_points = Kokkos::create_mirror_view(points);
  srand(90391);  

  // Initialize points with random coordinates in a 1Mx1Mx1M grid
  for(int i = 0; i < num_points; i++) {
    h_points(i, 0) = rand()%(1024*1024);
    h_points(i, 1) = rand()%(1024*1024);
    h_points(i, 2) = rand()%(1024*1024);
  }

  // Copy initial data to device
  Kokkos::deep_copy(points,h_points);

  // Initialize search point
  double search_x = rand()%(1024*1024);
  double search_y = rand()%(1024*1024);
  double search_z = rand()%(1024*1024);

  // Time finding of centroids
  struct timeval begin,end;

  gettimeofday(&begin,NULL);

  // iterate
  for(int repeat = 0; repeat < nrepeat; repeat++) {

    // Initialize reduction values
    MinFinder minf;

    // Find point with minimal distance
    Kokkos::parallel_reduce( num_points, KOKKOS_LAMBDA (const int& i, MinFinder& f) {
      // Calculate distance
      const double dx = search_x - points(i, 0);
      const double dy = search_y - points(i, 1);
      const double dz = search_z - points(i, 2);

      const double dist2 = dx*dx + dy*dy + dz*dz;
      
      // If distance is smaller than previous smallest distance
      // Set min_indx to current index, and lower min_dist2
      const MinFinder tmp(dist2,i);
      f += tmp;
    },minf);
    printf("Min indx: %i with dist2 %lf\n",minf.index,minf.value);
  }


  gettimeofday(&end,NULL);

  // Calculate time
  double time = 1.0*(end.tv_sec-begin.tv_sec) + 1.0e-6*(end.tv_usec-begin.tv_usec);

  // Error check
  int error = 0;

  // Print time

  if(error==0) { 
    printf("#NumPoints Time(s) TimePerIter(s) ProblemSize(MB) Bandwidth(GB/s)\n");
    printf("%i %lf %e %lf %lf\n",num_points,time,time/nrepeat,1.0e-6*num_points*3*8,1.0e-9*num_points*3*8*nrepeat/time);
  }
  else printf("Error\n");

  // Shutdown Kokkos
  Kokkos::finalize();
}
