#ifndef MATRIX_H
#define MATRIX_H

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    double real;
    double imag;
} Complex;

Complex complex_create(double real, double imag);
Complex complex_add(Complex a, Complex b);
Complex complex_multiply(Complex a, Complex b);

typedef struct {
    int rows;
    int cols;
    Complex *data;
} Matrix;

Matrix* matrix_create(int rows, int cols);
void matrix_destroy(Matrix **m);
void matrix_clear(Matrix *m);


bool matrix_valid_index(const Matrix *m, int row, int col);

void matrix_fill_random(Matrix *m, int min_val, int max_val);
void matrix_fill_sequential(Matrix *m);
bool matrix_copy(const Matrix *src, Matrix *dst);
bool matrix_equal(const Matrix *a, const Matrix *b);

Matrix* matrix_multiply(const Matrix *a, const Matrix *b);

void matrix_print(const Matrix *m, int fd);
Matrix* matrix_read_from_fd(int fd);

int matrix_size(const Matrix *m);
Matrix* matrix_transpose(const Matrix *m);

#endif
