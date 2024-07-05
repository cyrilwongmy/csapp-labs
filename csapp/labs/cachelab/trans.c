/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include "cachelab.h"
#include <stdio.h>

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {

  int i, j;

  if (M == 32 && N == 32) {
    for (i = 0; i < N; i += 8) {
      for (j = 0; j < M; j += 8) {
        for (int k = 0; k < 8; k++) {
          int a00 = A[i + k][j];
          int a01 = A[i + k][j + 1];
          int a02 = A[i + k][j + 2];
          int a03 = A[i + k][j + 3];
          int a04 = A[i + k][j + 4];
          int a05 = A[i + k][j + 5];
          int a06 = A[i + k][j + 6];
          int a07 = A[i + k][j + 7];
          B[j][i + k] = a00;
          B[j + 1][i + k] = a01;
          B[j + 2][i + k] = a02;
          B[j + 3][i + k] = a03;
          B[j + 4][i + k] = a04;
          B[j + 5][i + k] = a05;
          B[j + 6][i + k] = a06;
          B[j + 7][i + k] = a07;
        }
      }
    }
  } else if (M == 64 && N == 64) {
    for (i = 0; i < N; i += 8) {
      for (j = 0; j < M; j += 8) {
        // upper 4x8 sub-matrix
        for (int k = 0; k < 4; k++) {
          int a00 = A[i + k][j];
          int a01 = A[i + k][j + 1];
          int a02 = A[i + k][j + 2];
          int a03 = A[i + k][j + 3];
          int a04 = A[i + k][j + 4];
          int a05 = A[i + k][j + 5];
          int a06 = A[i + k][j + 6];
          int a07 = A[i + k][j + 7];
          B[j + 0][i + k] = a00;
          B[j + 1][i + k] = a01;
          B[j + 2][i + k] = a02;
          B[j + 3][i + k] = a03;
          B[j + 0][i + k + 4] = a04;
          B[j + 1][i + k + 4] = a05;
          B[j + 2][i + k + 4] = a06;
          B[j + 3][i + k + 4] = a07;
        }

        // move A's lower left to B's upper right, column by column
        for (int col = 0; col < 4; ++col) {
          int b04 = B[j + col][i + 4];
          int b05 = B[j + col][i + 5];
          int b06 = B[j + col][i + 6];
          int b07 = B[j + col][i + 7];

          int a00 = A[i + 4][j + col];
          int a10 = A[i + 5][j + col];
          int a20 = A[i + 6][j + col];
          int a30 = A[i + 7][j + col];

          B[j + col][i + 4] = a00;
          B[j + col][i + 5] = a10;
          B[j + col][i + 6] = a20;
          B[j + col][i + 7] = a30;

          B[j + col + 4][i + 0] = b04;
          B[j + col + 4][i + 1] = b05;
          B[j + col + 4][i + 2] = b06;
          B[j + col + 4][i + 3] = b07;
        }

        // move A's lower right
        for (int row = 4; row < 8; ++row) {
          int a44 = A[i + row][j + 4];
          int a45 = A[i + row][j + 5];
          int a46 = A[i + row][j + 6];
          int a47 = A[i + row][j + 7];
          B[j + 4][i + row] = a44;
          B[j + 5][i + row] = a45;
          B[j + 6][i + row] = a46;
          B[j + 7][i + row] = a47;
        }
      }
    }
  } else if (M == 61 && N == 67) {
    int i, j;

    int sz = 16;
    for (i = 0; i < N; i += sz) {
      for (j = 0; j < M; j += sz) {
        for (int k = i; k < N && k < i + sz; ++k) {
          for (int l = j; l < M && l < j + sz; ++l) {
            B[l][k] = A[k][l];
          }
        }
      }
    }
  }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, tmp;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j++) {
      tmp = A[i][j];
      B[j][i] = tmp;
    }
  }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions() {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);

  /* Register any additional transpose functions */
  registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i, j;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; ++j) {
      if (A[i][j] != B[j][i]) {
        return 0;
      }
    }
  }
  return 1;
}
