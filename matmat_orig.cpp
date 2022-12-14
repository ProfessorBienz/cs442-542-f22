#include <stdlib.h>
#include <stdio.h>
#include <cmath>
#include "timer.h"

// To compile with and without vectorization (in gcc):
// g++ -o matmat_orig matmat_orig.cpp -O1     <--- no vectorization
// Flag to vectoize : -ftree-vectorize  
// Flag needed for vectorization of X86 processors : -msse -msse2
// Flag needed for vectorization of PowerPC platforms : -maltivec
// Other optional flags (floating point reductions) : -ffast-math -fassociative-math
//
// To see what the compiler vectorizes : -fopt-info-vec (or -fopt-info-vec-optimized)
// To see what the compiler is not able to vectorize : -fopt-info-vec-missed


// Matrix-Matrix Multiplication of Doubles (Double Pointer)
// Test without the restrict variables
void matmat(int n, double* A, double*  B, double* C, int n_iter)
{
    double val;
    for (int iter = 0; iter < n_iter; iter++)
    {
        // Do Matrix-Multiplication HERE
        // Make sure to initalize C each time to 0 if you want consistent results
    }
}

int main(int argc, char* argv[])
{

    double start, end;
    int n_access = 1000000000;
    
    if (argc < 3)
    {
        printf("Need Matrix Dimemsion n and step size k passed as Command Line Arguments (e.g. ./matmat 8 2)\n");
        return 0;
    }

    // Read Matrix Dimension from Command Line
    int n = atoi(argv[1]);

    // Read Step Size (k) for part (c) from Command Line
    int k = atoi(argv[2]);
    int n_iter = (n_access / (n*n*n)) + 1;

    double* A = new double[n*n];
    double* B = new double[n*n];
    double* C = new double[n*n];

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            A[i*n+j] = 1.0/(i+1);
            B[i*n+j] = 1.0;
        }
    }

    // Warm-Up 
    matmat(n, A, B, C, n_iter);

    start = get_time();
    matmat(n, A, B, C, n_iter);
    end = get_time();
    printf("N %d, Time Per MatMat %e\n", n, (end - start)/n_iter);

    delete[] A;
    delete[] B; 
    delete[] C;

    return 0;
}
