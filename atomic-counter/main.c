#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include "../measurement.h"

typedef struct {
    int counter;
    pthread_mutex_t lock;
} AtomicCounter;

AtomicCounter *counter_create() {
    AtomicCounter *counter = malloc(sizeof(AtomicCounter));
    assert (counter != NULL);

    counter->counter = 0;
    int rc = pthread_mutex_init(&counter->lock, NULL);
    assert (rc == 0);

    return counter;
}

void counter_free(AtomicCounter *counter) {
    if (counter == NULL) {
        return;
    }

    assert(pthread_mutex_destroy(&counter->lock) == 0);
    free(counter);
}

void counter_increment(AtomicCounter *counter) {
    assert (pthread_mutex_lock(&counter->lock) == 0);
    counter->counter++;
    assert (pthread_mutex_unlock(&counter->lock) == 0);
}

int counter_get(AtomicCounter *counter) {
    assert (pthread_mutex_lock(&counter->lock) == 0);
    int val = counter->counter;
    assert (pthread_mutex_unlock(&counter->lock) == 0);
    return val;
}

void *incrementer(void *arg) {
    const int INCREMENT_COUNT = 1000000;

    ThreadBench *bench = malloc(sizeof(ThreadBench));
    assert (bench != NULL);

    bench_begin(bench);

    AtomicCounter *counter = (AtomicCounter*)arg;

    for (int i = 0; i<INCREMENT_COUNT; i++) {
        counter_increment(counter);
    }

    bench_end(bench);

    return bench;
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        return 1;
    }

    const int no_threads = atoi(argv[1]);

    ThreadBench bench;
    bench_begin(&bench);

    AtomicCounter *counter = counter_create();

    pthread_t threads[no_threads];

    for (int i = 0; i<no_threads; i++) {
        assert (pthread_create(threads+i, NULL, incrementer, counter) == 0);
    }

    ThreadBench *results[no_threads];
    for (int i = 0; i<no_threads; i++) {
        assert (pthread_join(threads[i], (void**)results+i) == 0);
    }

    printf("Counter: %d\n", counter_get(counter));

    counter_free(counter);

    bench_end(&bench);
    bench_print(&bench);

    for (int i = 0; i<no_threads; i++) {
        free(results[i]);
    }
}
