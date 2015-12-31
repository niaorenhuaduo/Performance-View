#include "matrix_util.h"
#include "supermat.h"



MATRIX 
expand(SUPERMAT s) {
  int i,j,r=0,c=0,ii,jj,bi,bj,t1,t2;
  MATRIX temp;


  for (i=0; i < s.rows; i++)  for (j=0; j < s.cols; j++) {
    t2 = (s.sub[i][j].rows != s.sub[i][0].rows) ;
    t1 = (s.sub[i][j].cols != s.sub[0][j].cols);
    if (t1 || t2) {
      printf("problem in dimension of matrix to be expanded\n");
      exit(0);
    }
  }
  if (s.cols) for (i=0; i < s.rows; i++) r += s.sub[i][0].rows;
  if (s.rows) for (j=0; j < s.cols; j++) c += s.sub[0][j].cols;
  temp = Malloc(r,c);
  bi = 0;
  for (i=0; i < s.rows; i++) {
    bj = 0;
    for (j=0; j < s.cols; j++) {
      for (ii=0; ii < s.sub[i][j].rows; ii++)
	for (jj=0; jj < s.sub[i][j].cols; jj++) 
	  temp.el[bi+ii][bj+jj] = s.sub[i][j].el[ii][jj];
      bj += s.sub[0][j].cols;
    }
    bi += s.sub[i][0].rows;
  }
  return(temp);
}
    
  

    
#define SPC 2

void
Sprint(SUPERMAT s) {
  MATRIX m;
  int ii,jj,i,j,hline,vline,r;
  
  m = expand(s);
  ii = jj = 0;
  for (i=0; i < m.rows; i++) {
    hline = 0;
    r = 0;
    for (ii=0; ii < s.rows; ii++) if (i == (r += s.sub[ii][0].rows)) hline = 1;
    if (hline)  {
      for (r=0; r < 3*s.cols + (5+SPC)*m.cols; r++) printf("-");
      printf("\n");
    }
    for (j=0; j < m.cols; j++) {
      vline = 0;
      r = 0;
      for (jj=0; jj < s.cols; jj++) if (j == (r+=s.sub[0][jj].cols)) vline = 1;
      if (vline) {
	printf(" | ");
      }
      printf("%5.3f",m.el[i][j]);
      for (r=0; r < SPC; r++) printf(" ");
    }
    printf("\n");
  }
  printf("\n");
}


/*SUPERMAT
Salloc(int r, int c) {
  SUPERMAT temp;
  int i,j;
  
  temp.rows = r;
  temp.cols = c;
  temp.sub = (MATRIX **) malloc(r*sizeof(MATRIX*));
  for (i=0; i < r; i++) temp.sub[i] = (MATRIX *) malloc(c*sizeof(MATRIX));
  return(temp);
}
*/  


SUPERMAT
Sinit(int r, int c, MATRIX **m) {
  SUPERMAT temp;
  int i,j;
  
  temp.rows = r;
  temp.cols = c;
  temp.sub = (MATRIX **) malloc(r*sizeof(MATRIX*));
  for (i=0; i < r; i++) temp.sub[i] = (MATRIX *) malloc(c*sizeof(MATRIX));
  for (i=0; i < r; i++) for (j=0; j < c; j++) {
    if (m[i][j].rows != m[i][0].rows || m[i][j].cols != m[0][j].cols) {
      printf("mismatch in making a super-duper matrix\n");
      exit(0);
    }
    temp.sub[i][j] = m[i][j];
  }
  return(temp);
}
    

SUPERMAT 
partition(MATRIX m, SUPERMAT s) {
  int i,j,r=0,c=0,ii,jj,bi,bj;
  SUPERMAT temp;

  for (i=0; i < s.rows; i++) r += s.sub[i][0].rows;
  for (j=0; j < s.cols; j++) c += s.sub[0][j].cols;
  if (r != m.rows || c != m.cols) {
    printf("mismatch in partition\n");
    exit(0);
  }
  temp = Salloc(s.rows,s.cols);
  for (i=0; i < s.rows; i++)  for (j=0; j < s.cols; j++) 
    temp.sub[i][j] = Malloc(s.sub[i][j].rows,s.sub[i][j].cols);
    
  bi = 0;
  for (i=0; i < s.rows; i++) {
    bj = 0;
    for (j=0; j < s.cols; j++) {
      for (ii=0; ii < s.sub[i][j].rows; ii++)
	for (jj=0; jj < s.sub[i][j].cols; jj++) 
	  temp.sub[i][j].el[ii][jj] = m.el[bi+ii][bj+jj];
      bj += s.sub[0][j].cols;
    }
    bi += s.sub[i][0].rows;
  }
  return(temp);
}


SUPERMAT       
row_partition(MATRIX m, int *dim, int n) { 
/* matrix m should have same number rows as the total # of the n dim[i]'s */

  SUPERMAT s;
  int i,j,jj,k,total=0;
  MATRIX temp;

  for (i=0; i < n; i++) total += dim[i];
  if (m.rows != total) {
    printf("dimension mismatch in row_partition\n");
    exit(0);
  }
  s = Salloc(n,1);
  jj = 0;
  for (i=0; i < n; i++) {
    temp = Malloc(dim[i],m.cols);
    for (j = 0; j < dim[i]; j++) {
      for (k=0; k < m.cols; k++) temp.el[j][k] = m.el[jj][k];
      jj++;
    }
    s.sub[i][0] = temp;
  }
  return(s);
}


SUPERMAT       
two_way_partition(MATRIX m, int *dim, int n) {
  SUPERMAT temp;
  int i,ii,j,jj,total=0,bi,bj;

  for (i=0; i < n; i++) total += dim[i];
  if (m.rows != total || m.cols != total) {
    printf("dimension mismatch in two_way_partition\n");
    exit(0);
  }
  temp = Salloc(n,n);
  bi = 0;
  for (i=0; i < n; i++) {
    bj = 0;
    for (j=0; j < n; j++) {
      temp.sub[i][j] = Malloc(dim[i],dim[j]);
      for (ii=0; ii < dim[i]; ii++)
	for (jj=0; jj < dim[j]; jj++) 
	  temp.sub[i][j].el[ii][jj] = m.el[bi+ii][bj+jj];
      bj += dim[j];
    }
    bi += dim[i];
  }
  return(temp);
}


