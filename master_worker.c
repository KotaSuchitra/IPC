// master_workers.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define FTOK_PATH "./shm_demo_keyfile"
#define FTOK_PROJID 0x42

/* Change these defaults as needed */
#define DEFAULT_NUM_WORKERS 4
#define DEFAULT_INCREMENTS 100000

typedef struct {
    int num_workers;
    _Atomic int counters[]; /* flexible array; actual size allocated at runtime */
} ShmRegion;

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void ensure_keyfile() {
    if (access(FTOK_PATH, F_OK) != 0) {
        FILE *f = fopen(FTOK_PATH, "ab");
        if (!f) die("fopen keyfile");
        fclose(f);
    }
}

int main(int argc, char **argv) {
    int num_workers = DEFAULT_NUM_WORKERS;
    int increments_per_worker = DEFAULT_INCREMENTS;

    if (argc >= 2) num_workers = atoi(argv[1]);
    if (argc >= 3) increments_per_worker = atoi(argv[2]);

    if (num_workers <= 0) num_workers = DEFAULT_NUM_WORKERS;
    if (increments_per_worker < 0) increments_per_worker = DEFAULT_INCREMENTS;

    printf("master: num_workers=%d increments_per_worker=%d\n", num_workers, increments_per_worker);

    ensure_keyfile();

    key_t key = ftok(FTOK_PATH, FTOK_PROJID);
    if (key == (key_t)-1) die("ftok");

    /* allocate shared memory sized for header + counters */
    size_t shm_size = sizeof(ShmRegion) + sizeof(_Atomic int) * num_workers;
    int shmid = shmget(key, shm_size, IPC_CREAT | 0666);
    if (shmid == -1) die("shmget");

    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *) -1) die("shmat");

    ShmRegion *region = (ShmRegion *)addr;
    region->num_workers = num_workers;

    /* Initialize counters to zero (use atomic_store) */
    for (int i = 0; i < num_workers; ++i) {
        atomic_store(&region->counters[i], 0);
    }

    pid_t *children = calloc(num_workers, sizeof(pid_t));
    if (!children) die("calloc");

    /* Fork workers */
    for (int i = 0; i < num_workers; ++i) {
        pid_t pid = fork();
        if (pid < 0) {
            die("fork");
        } else if (pid == 0) {
            /* Child (worker) */
            /* Seed PRNG differently per worker (optional) */
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            srand((unsigned int)(ts.tv_nsec ^ (getpid()<<8)));

            /* Do increments on own counter */
            for (int k = 0; k < increments_per_worker; ++k) {
                /* Using atomic_fetch_add for safe increments across processes */
                atomic_fetch_add_explicit(&region->counters[i], 1, memory_order_relaxed);

                /* Optional tiny random sleep to interleave work (uncomment to see interleaving) */
                /* if ((k & 0x3FF) == 0) usleep(rand() & 63); */
            }

            /* Worker prints a short message and exits */
            printf("[worker %d pid=%d] finished (expected=%d)\n", i, getpid(), increments_per_worker);
            /* detach before exit */
            if (shmdt(region) == -1) {
                perror("worker shmdt");
            }
            _exit(EXIT_SUCCESS);
        } else {
            /* Parent */
            children[i] = pid;
        }
    }

    /* Master: wait for all workers */
    for (int i = 0; i < num_workers; ++i) {
        int status;
        pid_t w = waitpid(children[i], &status, 0);
        if (w == -1) {
            perror("waitpid");
        } else {
            if (WIFEXITED(status)) {
                /* ok */
            } else {
                fprintf(stderr, "child %d exited abnormally\n", children[i]);
            }
        }
    }

    /* Aggregate results */
    long total = 0;
    printf("\n[master] per-worker counters:\n");
    for (int i = 0; i < num_workers; ++i) {
        int val = atomic_load_explicit(&region->counters[i], memory_order_relaxed);
        printf("  worker %d -> %d\n", i, val);
        total += val;
    }

    printf("[master] aggregated total = %ld (expected = %ld)\n",
           total, (long)num_workers * (long)increments_per_worker);

    /* Detach and remove shared memory */
    if (shmdt(region) == -1) {
        perror("master shmdt");
    }

    /* Mark the segment for removal */
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl(IPC_RMID)");
    } else {
        printf("[master] removed shared memory segment (shmid=%d)\n", shmid);
    }

    free(children);
    return EXIT_SUCCESS;
}
