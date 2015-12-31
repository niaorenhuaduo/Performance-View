#ifndef SUPERMATRIX
#define SUPERMATRIX

#include "matrix_util.h"



#ifdef FUTURE

#define MAX_SUPERMAT_DIM 4

typedef struct { /* matrix of matrices */
  int rows;
  int cols;
  MATRIX sub[MAX_SUPERMAT_DIM][MAX_SUPERMAT_DIM];
} SUPERMAT;  

#endif


SUPERMAT two_way_partition(MATRIX m, int *dim, int n);
SUPERMAT row_partition(MATRIX m, int *dim, int n);
MATRIX expand(SUPERMAT s);
void Sprint(SUPERMAT s);
SUPERMAT Salloc(int r, int c);
SUPERMAT Sinit(int r, int c, MATRIX **m);
SUPERMAT partition(MATRIX m, SUPERMAT s);


#endif



