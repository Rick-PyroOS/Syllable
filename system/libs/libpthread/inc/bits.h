/* POSIX threads for Syllable - A simple PThreads implementation             */
/*                                                                           */
/* Copyright (C) 2002 Kristian Van Der Vliet (vanders@users.sourceforge.net) */
/*                                                                           */
/* This program is free software; you can redistribute it and/or             */
/* modify it under the terms of the GNU Library General Public License       */
/* as published by the Free Software Foundation; either version 2            */
/* of the License, or (at your option) any later version.                    */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU Library General Public License for more details.                      */

#ifndef __F_SYLLABLE_PTHREAD_BITS_H_
#define __F_SYLLABLE_PTHREAD_BITS_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <time.h>

#include <atheos/semaphore.h>

inline status_t __pt_lock_mutex( sem_id mutex );
inline status_t __pt_unlock_mutex( sem_id mutex );

typedef struct __pt_timer_args_s
{
	struct timespec *abstime;
	pthread_t thread;
	pthread_cond_t* cond;
	int error;
} __pt_timer_args;

typedef struct __pt_cleanup_s
{
	void (*routine)(void*);
	void *arg;
	struct __pt_cleanup_s *prev;
} __pt_cleanup;


#define PT_TID(x)	(x)->id
#define PT_ATTR(x)	(x)->attr

/* What a pthread_t points at: the public struct, then what pthread_join()
   needs after the thread is gone.  The record lives until both the thread
   (its selfKey TLD, released when it exits) and its joiner (pthread_join() or
   pthread_detach()) have let go of it. */
typedef struct __pt_thread_s
{
	struct __pthread_s pub;		/* first: a pthread_t points at it */
	sem_id done;			/* unlocked when the thread exits; -1: not joinable (a pthread_self() record) */
	void *result;			/* its pthread_exit() value */
	atomic_t refs;
	bool detached;			/* joined, detached, or created detached */
} __pt_thread;

#define PT_PRIV(x)	((__pt_thread *)(x))

#ifdef __cplusplus
}
#endif

#endif	/* __F_SYLLABLE_PTHREAD_BITS_H_ */









