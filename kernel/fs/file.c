#include "riscv.h"
#include "fs/file.h"
#include "lib/lock.h"
#include "proc/proc.h"
#include 

dev_t devs[N_DEV];

file_t files[N_FILE]
spinlock_t flock;

void file_init()
{
    spinlock_init(&flock, "flock");
}

file_t *file_alloc()
{
    spinlock_acquire(&flock);
    for (int i = 0; i < N_FILE; i++) {
        if (files[i].ref == 0) {
            files[i].ref=1;
            spinlock_release(&flock);
            return &files[i];
        }
    }
    spinlock_release(&flock);
    return NULL;
}

file_t* file_create_dev(char* path, uint16 major, uint16 minor)
{

}
