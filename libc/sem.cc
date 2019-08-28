/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include <semaphore.h>
#include <osv/semaphore.hh>
#include <osv/sched.hh>
#include <memory>
#include "libc.hh"

#include <unordered_map>

// FIXME: smp safety

//TODO: Add open count for named semaphores
struct indirect_semaphore : std::unique_ptr<semaphore> {
    explicit indirect_semaphore(unsigned units)
        : std::unique_ptr<semaphore>(new semaphore(units)) {}
};

indirect_semaphore& from_libc(sem_t* p)
{
    return *reinterpret_cast<indirect_semaphore*>(p);
}

int sem_init(sem_t* s, int pshared, unsigned val)
{
    static_assert(sizeof(indirect_semaphore) <= sizeof(*s), "sem_t overflow");
    new (s) indirect_semaphore(val);
    return 0;
}

int sem_destroy(sem_t *s)
{
    from_libc(s).~indirect_semaphore();
    return 0;
}

int sem_post(sem_t* s)
{
    from_libc(s)->post();
    return 0;
}

int sem_wait(sem_t* s)
{
    from_libc(s)->wait();
    return 0;
}

int sem_timedwait(sem_t* s, const struct timespec *abs_timeout)
{
    if ((abs_timeout->tv_sec < 0) || (abs_timeout->tv_nsec < 0) || (abs_timeout->tv_nsec > 1000000000LL)) {
        return libc_error(EINVAL);
    }

    sched::timer tmr(*sched::thread::current());
    osv::clock::wall::time_point time(std::chrono::seconds(abs_timeout->tv_sec) +
                                      std::chrono::nanoseconds(abs_timeout->tv_nsec));
    tmr.set(time);
    if (!from_libc(s)->wait(1, &tmr)) {
        return libc_error(ETIMEDOUT);
    }
    return 0;
}

int sem_trywait(sem_t* s)
{
    if (!from_libc(s)->trywait())
        return libc_error(EAGAIN);
    return 0;
}

//TODO: Need to maintain open count for each semaphore
static std::unordered_map<std::string, indirect_semaphore> named_semaphores;
static mutex named_semaphores_mutex;

//TODO: Create common fun
sem_t *sem_open(const char *name, int oflag)
{
    SCOPE_LOCK(named_semaphores_mutex);
    auto semaphore = indirect_semaphore(0);
    named_semaphores.emplace(std::string(name), semaphore(0));
    return semaphore;
}

//TODO: Handle oflag (no need to worry about value)
//TODO: Need to increment open count
sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned int value)
{
    SCOPE_LOCK(named_semaphores_mutex);
    auto semaphore = indirect_semaphore(value);
    named_semaphores.emplace(std::string(name), semaphore(0));
    return semaphore;
}

//TODO: Need to derement open count (decrement when 0?)
int sem_close(sem_t *sem)
{
    //SCOPE_LOCK(named_semaphores_mutex);
    return 0;
}

//TODO: Should remove from map but probably not destroy
int sem_unlink(const char *name)
{
    SCOPE_LOCK(named_semaphores_mutex);
    auto semaphore = named_semaphores.erase(std::string(name));
    semaphore..~indirect_semaphore();
    return 0;
}
