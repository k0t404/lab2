#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/types.h>

#include "matrix.h"
#include "logger.h"

typedef struct {
    int matrix_size;
    int n_threads;
    int random_seed;
    int range_min;
    int range_max;
    bool benchmark_mode;
    int pause_seconds;
} ProgramArgs;

typedef struct {
    const Matrix *a;
    const Matrix *b;
    Matrix *c;
    int row_start;
    int row_end;
} ThreadArgs;

static void *matrix_multiply_thread(void *_args)
{
    ThreadArgs *args = (ThreadArgs *)_args;
    const Matrix *a = args->a;
    const Matrix *b = args->b;
    Matrix *c = args->c;

    int a_cols = a->cols;
    int b_cols = b->cols;

    for (int i = args->row_start; i < args->row_end; i++) {
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
    return NULL;
}

typedef struct {
    long sec;
    long nsec;
} TimePoint;

static TimePoint get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TimePoint){ .sec = ts.tv_sec, .nsec = ts.tv_nsec };
}

static double time_diff_ms(TimePoint start, TimePoint end)
{
    double sec = (double)(end.sec - start.sec);
    double nsec = (double)(end.nsec - start.nsec);
    return (sec * 1000.0) + (nsec / 1000000.0);
}

static double benchmark_sequential(const Matrix *a, const Matrix *b, Matrix *c)
{
    matrix_clear(c);
    TimePoint t_start = get_time();
    Matrix *result = matrix_multiply(a, b);
    TimePoint t_end = get_time();
    if (result) {
        matrix_copy(result, c);
        matrix_destroy(&result);
    }
    return time_diff_ms(t_start, t_end);
}

static double benchmark_parallel(const Matrix *a, const Matrix *b, Matrix *c, int n_threads)
{
    matrix_clear(c);

    pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
    ThreadArgs *thread_args = malloc(n_threads * sizeof(ThreadArgs));

    if (!threads || !thread_args) {
        free(threads);
        free(thread_args);
        log_error("failed to allocate memory for threads");
    }

    int total_rows = a->rows;
    int rows_per_thread = total_rows / n_threads;
    int remainder = total_rows % n_threads;

    int current_row = 0;
    for (int t = 0; t < n_threads; t++) {
        int rows_for_this = rows_per_thread + (t < remainder ? 1 : 0);
        thread_args[t] = (ThreadArgs){
            .a = a, .b = b, .c = c,
            .row_start = current_row,
            .row_end = current_row + rows_for_this,
        };
        current_row += rows_for_this;

        if (pthread_create(&threads[t], NULL, matrix_multiply_thread,
                           &thread_args[t]) != 0) {
            free(thread_args);
            free(threads);
            log_error("failed to create thread");
        }
    }

    TimePoint t_start = get_time();
    for (int t = 0; t < n_threads; t++)
        pthread_join(threads[t], NULL);
    TimePoint t_end = get_time();

    free(thread_args);
    free(threads);

    return time_diff_ms(t_start, t_end);
}

static void print_usage(const char *prog_name)
{
    log_write_str(STDOUT_FILENO, "Usage: ");
    log_write_str(STDOUT_FILENO, prog_name);
    log_write_str(STDOUT_FILENO,
        " [options]\n"
        "Options:\n"
        "  -n <size>    Matrix size N x N (default: 256)\n"
        "  -p <threads> Number of threads (required in normal mode)\n"
        "  -s <seed>    Random seed (default: 42)\n"
        "  -r <min:max> Value range (default: 1:10)\n"
        "  -b           Benchmark mode\n"
        "  -w <sec>     Pause for ps demonstration (default: 0)\n"
        "  -h           Show this help\n");
}

static ProgramArgs parse_args(int argc, char **argv)
{
    ProgramArgs args = {
        .matrix_size = 256,
        .n_threads = 0,
        .random_seed = 42,
        .range_min = 1,
        .range_max = 10,
        .benchmark_mode = false,
        .pause_seconds = 0,
    };

    int opt;
    while ((opt = getopt(argc, argv, "n:p:s:r:bw:h")) != -1) {
        switch (opt) {
        case 'n': args.matrix_size = atoi(optarg); break;
        case 'p': args.n_threads = atoi(optarg); break;
        case 's': args.random_seed = atoi(optarg); break;
        case 'r': {
            char *colon = strchr(optarg, ':');
            if (colon) {
                *colon = '\0';
                args.range_min = atoi(optarg);
                args.range_max = atoi(colon + 1);
            }
            break;
        }
        case 'b': args.benchmark_mode = true; break;
        case 'w': args.pause_seconds = atoi(optarg); break;
        case 'h': print_usage(argv[0]); exit(EXIT_SUCCESS);
        default: print_usage(argv[0]); exit(EXIT_FAILURE);
        }
    }

    if (args.matrix_size <= 0)
        log_error("matrix size must be positive");
    if (!args.benchmark_mode && args.n_threads <= 0)
        log_error("thread count (-p) is required in normal mode");

    return args;
}

int main(int argc, char **argv)
{
    ProgramArgs args = parse_args(argc, argv);

    pid_t pid = getpid();
    log_write_str(STDOUT_FILENO, "Process PID: ");
    log_write_int(STDOUT_FILENO, pid);
    log_write_str(STDOUT_FILENO, "\n");
    log_write_str(STDOUT_FILENO, "To view threads: ps -T -p ");
    log_write_int(STDOUT_FILENO, pid);
    log_write_str(STDOUT_FILENO, "\n\n");

    if (args.pause_seconds > 0) {
        log_write_str(STDOUT_FILENO, "Pausing for ");
        log_write_int(STDOUT_FILENO, args.pause_seconds);
        log_write_str(STDOUT_FILENO, " seconds for ps demonstration...\n");
        sleep(args.pause_seconds);
    }

    srand(args.random_seed);

    Matrix *a = matrix_create(args.matrix_size, args.matrix_size);
    Matrix *b = matrix_create(args.matrix_size, args.matrix_size);
    Matrix *c_seq = matrix_create(args.matrix_size, args.matrix_size);
    Matrix *c_par = matrix_create(args.matrix_size, args.matrix_size);

    if (!a || !b || !c_seq || !c_par)
        log_error("failed to create matrices");

    matrix_fill_random(a, args.range_min, args.range_max);
    matrix_fill_random(b, args.range_min, args.range_max);

    double t_seq = benchmark_sequential(a, b, c_seq);

    double t_par = benchmark_parallel(a, b, c_par, args.n_threads);

    double speedup = (t_par > 0.0) ? t_seq / t_par : 0.0;
    double efficiency = (args.n_threads > 0) ? speedup / args.n_threads : 0.0;

    log_write_str(STDOUT_FILENO,
        "--------------------------------------------------------------------------------\n"
        "| Threads | Seq (ms)    | Par (ms)    | Speedup     | Efficiency  |\n"
        "--------------------------------------------------------------------------------\n"
        "| ");
    log_write_int(STDOUT_FILENO, args.n_threads);
    log_write_str(STDOUT_FILENO, "        | ");
    log_write_double(STDOUT_FILENO, t_seq, 3);
    log_write_str(STDOUT_FILENO, "    | ");
    log_write_double(STDOUT_FILENO, t_par, 3);
    log_write_str(STDOUT_FILENO, "    | ");
    log_write_double(STDOUT_FILENO, speedup, 3);
    log_write_str(STDOUT_FILENO, "    | ");
    log_write_double(STDOUT_FILENO, efficiency, 3);
    log_write_str(STDOUT_FILENO, "    |\n"
        "--------------------------------------------------------------------------------\n\n");


    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&c_seq);
    matrix_destroy(&c_par);

    return 0;
}
