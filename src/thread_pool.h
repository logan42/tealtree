#ifndef __tealtree__thread_pool__
#define __tealtree__thread_pool__

// TODO:
// Rewrite the threadpool. The main disadvantage of the boost threadpool is that
// it doesn't support blocking when the threadpool is all busy. This might lead to
// creating a huge queue of waiting tasks. We'd like to block the current thread instead.
// We can use BlockingBoundedQueue instaed as a container for waiting tasks.
// Also boost threadpool seems to be dead as a project.
#include "boost/threadpool.hpp"

#endif /* defined(__tealtree__thread_pool__) */
