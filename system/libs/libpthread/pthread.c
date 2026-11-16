/* POSIX threads for Syllable - A simple PThreads implementation             */
/*                                                                           */
/* Copyright (C) 2002 Kristian Van Der Vliet (vanders@users.sourceforge.net) */
/* Copyright (C) 2000 Kurt Skauen                                            */
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

#include <pthread.h>
#include <signal.h>				/* pthread_signal() etc.           */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errno.h>
#include <posix/errno.h>
#include <posix/signal.h>
#include <atheos/atomic.h>
#include <atheos/tld.h>			/* Thread specific local data       */
#include <atheos/threads.h>		/* Kernel threads interface         */
#include <atheos/kernel.h>		/* Additional kernel & thread API's */

#include <bits.h>
#include <debug.h>

static pthread_key_t cleanupKey = 0;
static pthread_key_t selfKey = 0;

static int __pt_thread_exists(pthread_t thread)
{
	thread_info info;
	return ( get_thread_info( PT_TID(thread), &info ) == EOK ) ? 1 : 0;
}

static pthread_t __pt_new_thread(void)
{
	__pt_thread *t = malloc( sizeof( __pt_thread ) );

	if( t == NULL )
		return NULL;
	memset( t, 0, sizeof( __pt_thread ) );
	t->done = -1;
	atomic_set( &t->refs, 1 );
	return &t->pub;
}

/* drop one reference to a thread record, freeing it with the last */
static void __pt_release(pthread_t thread)
{
	__pt_thread *t = PT_PRIV(thread);

	if( !atomic_dec_and_test( &t->refs ) )
		return;

	debug( "cleaning up thread data\n" );

	/* delete the attribute data if we created it */
	if( thread->attr && thread->attr->internal_malloc )
		free( thread->attr );
	if( t->done >= 0 )
		delete_semaphore( t->done );
	free( t );
}

/* the selfKey TLD's destructor: the thread has exited */
static void __pt_cleanup_data(void *data)
{
	if( data )
		__pt_release( (pthread_t)data );
}

static void __pt_handle_cancel( int signo )
{
	pthread_t thread = pthread_getspecific( selfKey );
	pthread_attr_t *attr = ( thread != NULL ) ? PT_ATTR(thread) : NULL;

	debug( "cancellation requested\n" );

	if( attr != NULL )
	{
		if( attr->cancellation->state == PTHREAD_CANCEL_ENABLE )
			attr->cancellation->cancelled = true;

		if( attr->cancellation->type == PTHREAD_CANCEL_ASYNCHRONOUS )
			pthread_exit( NULL );
	}
}

void __attribute__((constructor)) __pt_init(void)
{
	cleanupKey = alloc_tld( NULL );
	selfKey = alloc_tld( __pt_cleanup_data );
}

void __attribute__((destructor)) __pt_uninit(void)
{
	free_tld( cleanupKey );
	free_tld( selfKey );
}

struct __pt_entry_data
{
	void *(*start_routine)(void *);
	void *arg;
	pthread_t pthread;
};

static void *__pt_entry(void *arg)
{
	struct __pt_entry_data *entry_data = (struct __pt_entry_data *)arg;
	struct sigaction sa;
	void *result;

	pthread_setspecific( selfKey, (void *)entry_data->pthread );

	/* catch SIGCANCEL from pthread_cancel() */
	sa.sa_handler = __pt_handle_cancel;
	sigaction( SIGCANCEL, &sa, NULL );

	result = entry_data->start_routine(entry_data->arg);
	free( entry_data );
	pthread_exit( result );
}

int pthread_cancel(pthread_t thread)
{
	debug( "attempting to cancel thread %d\n", PT_TID(thread) );
	return pthread_kill( thread, SIGCANCEL );
}

void pthread_cleanup_push(void (*routine)(void*), void *arg)
{
	__pt_cleanup *cleanup;

	debug( "adding cleanup handler at %p(%p) for thread %d\n", routine, arg, PT_TID(pthread_self()) );

	cleanup = malloc( sizeof( __pt_cleanup ) );
	cleanup->routine = routine;
	cleanup->arg = arg;
	cleanup->prev = (__pt_cleanup *) pthread_getspecific( cleanupKey );

	pthread_setspecific( cleanupKey, (void *) cleanup );
}

void pthread_cleanup_pop(int execute)
{
	__pt_cleanup *cleanup;

	cleanup = (__pt_cleanup *) pthread_getspecific( cleanupKey );
	if ( cleanup )
	{
		debug( "running cleanup for thread %d\n", PT_TID(pthread_self()) );

		if ( execute && cleanup->routine )
			(*cleanup->routine) (cleanup->arg);
		pthread_setspecific( cleanupKey, (void *) cleanup->prev );
		free( cleanup );
	}
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	pthread_t pthread;
	thread_id kthread;
	pthread_attr_t *thread_attr = NULL;
	struct __pt_entry_data *entry_data = NULL;

	if( thread == NULL || start_routine == NULL )
		return EINVAL;

	if( attr && attr->destroyed == true )
		return EINVAL;

	debug( "creating new thread\n" );

	pthread = __pt_new_thread();
	if( pthread == NULL )
		return ENOMEM;

	if( attr == NULL )
	{
		debug( "creating default attributes\n" );

		thread_attr = malloc( sizeof( pthread_attr_t ) );
		if( thread_attr == NULL )
			return ENOMEM;

		pthread_attr_init( thread_attr );		/* Use default values */

		thread_attr->internal_malloc = true;
	}
	else
	{
		debug( "using supplied attributes\n" );

		thread_attr = (pthread_attr_t*)attr;
	}

	/* joinable: one reference for the thread, one for its joiner */
	if( thread_attr->__detachstate == PTHREAD_CREATE_DETACHED )
		PT_PRIV(pthread)->detached = true;
	else
	{
		PT_PRIV(pthread)->done = create_semaphore( "pthread_join", 0, 0 );
		if( PT_PRIV(pthread)->done < 0 )
		{
			pthread->attr = thread_attr;
			__pt_release( pthread );
			return EAGAIN;
		}
		atomic_set( &PT_PRIV(pthread)->refs, 2 );
	}
	pthread->attr = thread_attr;

	entry_data = malloc( sizeof( *entry_data ) );
	if( entry_data == NULL )
	{
		atomic_set( &PT_PRIV(pthread)->refs, 1 );
		__pt_release( pthread );
		return ENOMEM;
	}
	entry_data->start_routine = start_routine;
	entry_data->arg = arg;
	entry_data->pthread = pthread;

	kthread = spawn_thread( thread_attr->__name, __pt_entry, thread_attr->__schedpriority, thread_attr->__stacksize, entry_data);
	if( kthread < 0 )
	{
		free( entry_data );
		atomic_set( &PT_PRIV(pthread)->refs, 1 );
		__pt_release( pthread );
		return( kthread );
	}

	debug( "spawn_thread gives thread id %d\n", kthread );

	pthread->id = kthread;

	*thread = pthread;

	debug( "resuming new thread\n" );

	resume_thread( kthread );

	return 0;
}

int pthread_detach(pthread_t thread)
{
	if( thread == NULL )
		return ESRCH;
	if( PT_PRIV(thread)->detached || PT_PRIV(thread)->done < 0 )
		return EINVAL;

	/* nobody will join it: give up the joiner's reference */
	PT_PRIV(thread)->detached = true;
	__pt_release( thread );
	return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
	if( t1 == NULL || t2 == NULL )
		return 0;

	return ( PT_TID(t1) == PT_TID(t2) );
}

void pthread_exit(void *value_ptr)
{
	__pt_cleanup *cleanup, *prev;
	pthread_t self;
	int result = 0;

	debug( "exit thread %d\n", PT_TID(pthread_self()) );

	/* call all cleanup routines */
	cleanup = (__pt_cleanup *) pthread_getspecific( cleanupKey );
	while ( cleanup )
	{
		if ( cleanup->routine )
			(*cleanup->routine) (cleanup->arg);
		prev = cleanup->prev;
		free( cleanup );
		cleanup = prev;
	}
	pthread_setspecific( cleanupKey, NULL );

	/* hand the value to pthread_join(), which may come before or after this */
	self = pthread_getspecific( selfKey );
	if( self != NULL && PT_PRIV(self)->done >= 0 )
	{
		PT_PRIV(self)->result = value_ptr;
		unlock_semaphore( PT_PRIV(self)->done );
	}

	if( NULL != value_ptr )
		result = (int)value_ptr;
	exit_thread( result );
}

int pthread_getconcurrency(void)
{
	return ENOSYS;
}

int pthread_getschedparam(pthread_t thread, int *foo, struct sched_param *param)
{
	return ENOSYS;
}

void *pthread_getspecific(pthread_key_t key)
{
	/* get_tld() will perform additional validation */
	if( key < TLD_USER )
		return NULL;

	return get_tld( key );
}

int pthread_join(pthread_t thread, void **value_ptr)
{
	__pt_thread *t;
	status_t error;

	if( thread == NULL )
		return ESRCH;
	t = PT_PRIV(thread);

	debug( "thread %d joins on thread %d\n", PT_TID(pthread_self()), PT_TID(thread) );

	if( pthread_equal( pthread_self(), thread ) )
		return EDEADLK;

	if( t->detached )
		return EINVAL;

	/* a record pthread_self() made (the main thread): there is no exit value to
	   wait for, only the kernel thread */
	if( t->done < 0 )
	{
		int result;

		if( !__pt_thread_exists( thread ) )
			return ESRCH;
		result = wait_for_thread( PT_TID(thread) );
		if( NULL != value_ptr )
			*value_ptr = (void*)result;
		return 0;
	}

	/* the thread unlocks done in pthread_exit(), whether that was before now
	   or not: the record outlives it until we let go */
	while( ( error = lock_semaphore( t->done ) ) < 0 && ( error == -EINTR || errno == EINTR ) )
		;
	if( error < 0 )
		return ESRCH;

	debug( "result was %p\n", t->result );
	if( NULL != value_ptr )
		*value_ptr = t->result;

	t->detached = true;
	__pt_release( thread );
	return 0;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	int tld = alloc_tld( destructor );

	debug( "allocated new TLD %d with destructor at %p for thread %d\n", tld, destructor, PT_TID(pthread_self()) );

	if ( tld >= 0 )
	{
		*key = tld;
		return 0;
	}
	else
		return -1;
}

int pthread_key_delete(pthread_key_t key)
{
	debug( "freeing TLD %d for thread %d\n", key, PT_TID(pthread_self()) );
	return free_tld( key );
}

int pthread_kill(pthread_t thread, int sig)
{
	int ret;

	if( sig < 0 || sig > _NSIG )
		return EINVAL;

	if( __pt_thread_exists( thread ) )
		ret = kill( (pid_t)PT_TID(thread), sig );
	else
		ret = ESRCH;

	return ret;
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	if ( atomic_swap( once_control, 1 ) == 0 )
		init_routine();

	return 0;
}

pthread_t pthread_self(void)
{
	pthread_t thread;

	thread = (pthread_t)get_tld( selfKey );
	/* if the calling thread is the "main" thread then no selfKey will have
	   been set, so we must create one */
	if( thread == NULL )
	{
		thread = __pt_new_thread();
		if( thread == NULL )
			return NULL;

		thread->id = get_thread_id( NULL );

		thread->attr = malloc( sizeof( pthread_attr_t ) );
		if( thread->attr == NULL )
			return NULL;

		pthread_attr_init( thread->attr );		/* Use default values */
		thread->attr->internal_malloc = true;

		pthread_setspecific( selfKey, (void *)thread );
	}
	__pt_assert( thread != NULL );

	return thread;
}

int pthread_setcancelstate(int state, int *oldstate)
{
	/* pthread_self(): the main thread has no self record until it is made */
	pthread_t thread = pthread_self();
	pthread_attr_t *attr;

	if( ( state != PTHREAD_CANCEL_ENABLE ) && ( state != PTHREAD_CANCEL_DISABLE ) )
		return EINVAL;
	if( thread == NULL )
		return ENOMEM;
	attr = PT_ATTR(thread);

	if( oldstate != NULL )
		*oldstate = attr->cancellation->state;
	attr->cancellation->state = state;

	return 0;
}

int pthread_setcanceltype(int type, int *oldtype)
{
	pthread_t thread = pthread_self();
	pthread_attr_t *attr;

	if( ( type != PTHREAD_CANCEL_DEFERRED ) && ( type != PTHREAD_CANCEL_ASYNCHRONOUS ) )
		return EINVAL;
	if( thread == NULL )
		return ENOMEM;
	attr = PT_ATTR(thread);

	if( oldtype != NULL )
		*oldtype = attr->cancellation->type;
	attr->cancellation->type = type;

	return 0;
}

int pthread_setconcurrency(int foo)
{
	return ENOSYS;
}

int pthread_setschedparam(pthread_t thread, int foo, const struct sched_param *param)
{
	return ENOSYS;
}

int pthread_setspecific(pthread_key_t key, const void *data)
{
	/* set_tld() will perform additional validation */
	if( key < TLD_USER )
		return EINVAL;

	set_tld( key, (void*)data );
	return 0;
}

void pthread_testcancel(void)
{
	pthread_t thread = pthread_getspecific( selfKey );
	pthread_attr_t *attr;

	/* no self record: the main thread, which nothing can have cancelled */
	if( thread == NULL )
		return;
	attr = PT_ATTR(thread);

	if( attr->cancellation->state == PTHREAD_CANCEL_ENABLE && attr->cancellation->cancelled )
		pthread_exit( NULL );
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	return sigprocmask( how, set, oset );
}

