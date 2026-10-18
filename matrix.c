#include "matrix.h"
#include "logger.h"

#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>


Complex complex_create(double real, double imag)
{
    Complex c;
    c.real = real;
    c.imag = imag;
    return c;
}

Complex complex_add(Complex a, Complex b)
{
    return complex_create(a.real + b.real, a.imag + b.imag);
}

Complex complex_multiply(Complex a, Complex b)
{
    double real = a.real * b.real - a.imag * b.imag;
    double imag = a.real * b.imag + a.imag * b.real;
    return complex_create(real, imag);
}


Matrix* matrix_create(int rows, int cols)
{
    if (rows <= 0 || cols <= 0)
        return NULL;

    Matrix *m = (Matrix*)malloc(sizeof(Matrix));
    if (!m) return NULL;

    m->rows = rows;
    m->cols = cols;
    m->data = (Complex*)calloc(rows * cols, sizeof(Complex));

    if (!m->data) {
        free(m);
        return NULL;
    }

    return m;
}

void matrix_destroy(Matrix **m)
{
    if (m && *m) {
        free((*m)->data);
        free(*m);
        *m = NULL;
    }
}

void matrix_clear(Matrix *m)
{
    if (m && m->data) {
        for (int i = 0; i < m->rows * m->cols; i++) {
            m->data[i] = complex_create(0.0, 0.0);
        }
    }
}


bool matrix_valid_index(const Matrix *m, int row, int col)
{
    return (m != NULL && row >= 0 && row < m->rows &&
            col >= 0 && col < m->cols);
}

void matrix_fill_random(Matrix *m, int min_val, int max_val)
{
    if (!m || !m->data) return;

    for (int i = 0; i < m->rows * m->cols; i++) {
        double real = min_val + rand() % (max_val - min_val + 1);
        double imag = min_val + rand() % (max_val - min_val + 1);
        m->data[i] = complex_create(real, imag);
    }
}

void matrix_fill_sequential(Matrix *m)
{
    if (!m || !m->data) return;

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            double real = (i + 1) * 10 + (j + 1);
            double imag = (i + 1) + (j + 1);
            m->data[i * m->cols + j] = complex_create(real, imag);
        }
    }
}

bool matrix_copy(const Matrix *src, Matrix *dst)
{
    if (!src || !dst || src->rows != dst->rows || src->cols != dst->cols)
        return false;

    for (int i = 0; i < src->rows * src->cols; i++)
        dst->data[i] = src->data[i];

    return true;
}



Matrix* matrix_multiply(const Matrix *a, const Matrix *b)
{
    if (!a || !b || a->cols != b->rows) {
        log_error("incompatible matrix dimensions for multiplication");
    }

    Matrix *c = matrix_create(a->rows, b->cols);
    if (!c) return NULL;

    int a_rows = a->rows;
    int a_cols = a->cols;
    int b_cols = b->cols;

    for (int i = 0; i < a_rows; i++) {
        for (int k = 0; k < a_cols; k++) {
            Complex a_ik = a->data[i * a_cols + k];

            if (a_ik.real == 0.0 && a_ik.imag == 0.0)
                continue;

            for (int j = 0; j < b_cols; j++) {
                Complex b_kj = b->data[k * b_cols + j];
                Complex product = complex_multiply(a_ik, b_kj);
                int idx = i * b_cols + j;
                c->data[idx] = complex_add(c->data[idx], product);
            }
        }
    }

    return c;
}

static bool read_char(int fd, char *c)
{
    ssize_t n = read(fd, c, 1);
    return (n == 1);
}

static void skip_whitespace(int fd)
{
    char c;
    while (read_char(fd, &c)) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            lseek(fd, -1, SEEK_CUR);
            break;
        }
    }
}

static bool read_int_fd(int fd, int *value)
{
    skip_whitespace(fd);

    char c;
    if (!read_char(fd, &c))
        return false;

    int sign = 1;
    if (c == '-') {
        sign = -1;
        if (!read_char(fd, &c))
            return false;
    } else if (c == '+') {
        if (!read_char(fd, &c))
            return false;
    }

    if (c < '0' || c > '9')
        return false;

    int result = c - '0';
    while (read_char(fd, &c)) {
        if (c < '0' || c > '9') {
            lseek(fd, -1, SEEK_CUR);
            break;
        }
        result = result * 10 + (c - '0');
    }

    *value = sign * result;
    return true;
}

static bool read_double_fd(int fd, double *value)
{
    skip_whitespace(fd);

    char c;
    if (!read_char(fd, &c))
        return false;

    int sign = 1;
    if (c == '-') {
        sign = -1;
        if (!read_char(fd, &c))
            return false;
    } else if (c == '+') {
        if (!read_char(fd, &c))
            return false;
    }

    if (c < '0' || c > '9')
        return false;

    double result = c - '0';
    while (read_char(fd, &c)) {
        if (c < '0' || c > '9') {
            if (c == '.') break;
            lseek(fd, -1, SEEK_CUR);
            break;
        }
        result = result * 10 + (c - '0');
    }

    if (c == '.') {
        double frac = 0.1;
        while (read_char(fd, &c)) {
            if (c < '0' || c > '9') {
                lseek(fd, -1, SEEK_CUR);
                break;
            }
            result += (c - '0') * frac;
            frac *= 0.1;
        }
    }

    *value = sign * result;
    return true;
}

Matrix* matrix_read_from_fd(int fd)
{
    int rows, cols;
    if (!read_int_fd(fd, &rows) || !read_int_fd(fd, &cols))
        return NULL;

    Matrix *m = matrix_create(rows, cols);
    if (!m) return NULL;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            double real, imag;
            if (!read_double_fd(fd, &real) || !read_double_fd(fd, &imag)) {
                matrix_destroy(&m);
                return NULL;
            }
            m->data[i * cols + j] = complex_create(real, imag);
        }
    }

    return m;
}

void matrix_print(const Matrix *m, int fd)
{
    if (!m || !m->data) {
        log_write_str(fd, "(empty matrix)\n");
        return;
    }

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            Complex c = m->data[i * m->cols + j];
            log_write_double(fd, c.real, 1);
            if (c.imag >= 0)
                log_write_str(fd, "+");
            log_write_double(fd, c.imag, 1);
            log_write_str(fd, "i");
            if (j < m->cols - 1)
                log_write_str(fd, "  ");
        }
        log_write_str(fd, "\n");
    }
}

int matrix_size(const Matrix *m)
{
    return m ? m->rows * m->cols : 0;
}

Matrix* matrix_transpose(const Matrix *m)
{
    if (!m) return NULL;

    Matrix *t = matrix_create(m->cols, m->rows);
    if (!t) return NULL;

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            t->data[j * m->rows + i] = m->data[i * m->cols + j];
        }
    }

    return t;
}
