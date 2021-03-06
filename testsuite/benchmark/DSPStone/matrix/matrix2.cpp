/*
 * benchmark program:   matrix.c
 * 
 * benchmark suite:     DSP-kernel
 * 
 * description:         generic matrix - multiply benchmarking
 *
 * This program performs a matrix multiplication of the form C=AB,
 * where A and B are two dimensional matrices of arbitrary dimension.
 * The only restriction os that the inner dimension of the arrays must
 * be greater than 1. 
 * 
 *          A[X x Y] * B[Y x Z] = C[X x Z]
 *                    
 *                  |a11     a12     ..      a1y|
 *                  |a21     a22     ..      a2y|
 * matrix A[X x Y]= |..      ..      ..     ..  |
 *                  |a(x-1)1 a(x-1)2 ..  a(x-1)y|
 *                  |ax1     ax2     ..      axy|
 *
 *
 *                  |b11     b12     ..     b1z|
 *                  |b21     b22     ..     b2z|
 * matrix B[Y x Z]= |..      ..      ..     .. |
 *                  |b(y-1)1 b(y-1)2 .. b(y-1)z|
 *                  |by1     by2     ..     byz|
 *
 *                  |c11     c12     ..     c1z|
 *                  |c21     c22     ..     c2z|
 * matrix C[X x Z]= |..      ..      ..     .. |
 *                  |c(x-1)1 c(x-1)2 .. c(x-1)z|
 *                  |cx1     cx2     ..     cxz|
 * 
 * matrix elements are stored as
 *
 * A[X x Y] = { a11, a12, .. , a1y, 
 *              a21, a22, .. , a2y, ..., 
 *              ax1, ax2, .. , axy}
 * 
 * B[Y x Z] = { b11, b21, .., b(y-1)1, by1, b12, b22, .. , b(y-1)z, byz} 
 * 
 * C[X x Z] = { c11, c21, .. , c(x-1)1, cx1, c12, c22, .. ,c(x-1)z, cxz }
 * 
 * 
 * reference code: 
 * 
 * f. verification:     
 * 
 * organization:         Aachen University of Technology - IS2 
 *                       DSP Tools Group
 *                       phone:  +49(241)807887 
 *                       fax:    +49(241)8888195
 *                       e-mail: zivojnov@ert.rwth-aachen.de 
 *
 * author:              Juan Martinez Velarde
 * 
 * history:             3-4-94 creation (Martinez Velarde)
 *                      5-4-94 profiling (Martinez Velarde)
 *
 *                      $Author: schraut $
 *                      $Date: 1995/01/30 06:56:25 $
 *                      $Revision: 1.3 $
 */

#define STORAGE_CLASS  register
#define TYPE           int

#define X 10 /* first dimension of array A */
#define Y 10 /* second dimension of array A, first dimension of array B */
#define Z 10 /* second dimension of array B */

#ifdef __cplusplus
extern "C" {
#endif
#include<stdio.h>
		void matrix2(TYPE *p_a,TYPE *p_b,TYPE *p_c){
			STORAGE_CLASS TYPE k,i,f;
			TYPE *A,*B;
			A = p_a;
			B = p_b;
			for (k = 0 ; k < Z ; k++)
				{
					p_a = A ;                  /* point to the beginning of array A */
      
					for (i = 0 ; i < X; i++)
			{
				p_b = B + (k*Y) ;            /* take next column */
	  
				*p_c =  *p_a++ * *p_b++ ;  /* first multiply */
	  
				for (f = 0 ; f < Y-2; f++) /* do multiply */
					*p_c += *p_a++ * *p_b++ ;
	  
				*p_c++ += *p_a++ * *p_b++ ;/* last multiply */
	  	  
			}
				}

		}
#ifdef __cplusplus
}
#endif

TYPE
pin_down(TYPE A[], TYPE B[], TYPE C[])
{
  int i ; 
  
  for (i = 0 ; i < X*Y; i++)
      A[i] = 1 ; 
  
  for (i = 0 ; i < Y*Z ; i++)
      B[i] = 1 ; 
  
  for (i = 0 ; i < X*Z ; i++)
      C[i] = 0 ; 
  
  return((TYPE)0) ; 

}

TYPE
main()
{ 
  static  TYPE A[X*Y] ; 
  static  TYPE B[Y*Z] ;
  static  TYPE C[X*Z] ;

  STORAGE_CLASS TYPE *p_a = &A[0] ;
  STORAGE_CLASS TYPE *p_b = &B[0] ;
  STORAGE_CLASS TYPE *p_c = &C[0] ;
  int j;
 // TYPE f,i,k ; 

  pin_down(&A[0], &B[0], &C[0]) ; 

	matrix2(*&p_a,*&p_b,*&p_c);
 
	for(j=0;j<100;j++)
		printf("C[%d]: %d\n",j,C[j]);
      
  pin_down(&A[0], &B[0], &C[0]) ; 
    
  return(0)  ; 
}
