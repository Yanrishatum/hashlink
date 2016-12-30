/*
 * Copyright (C)2005-2016 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <hl.h>

#ifndef HL_WIN
#	include <pthread.h>
#	include <sys/syscall.h>
#endif

HL_PRIM hl_thread *hl_thread_current() {
#	ifdef HL_WIN
	return (hl_thread*)(int_val)GetCurrentThreadId();
#	else
	return (hl_thread*)pthread_self();
#	endif
}

HL_PRIM int hl_thread_id() {
#	ifdef HL_WIN
	return (int)GetCurrentThreadId();
#	else
#	ifdef SYS_gettid
	return syscall(SYS_gettid);
#	else
	hl_error("hl_thread_id() not available for this platform");
	return -1;
#	endif
#	endif
}

HL_PRIM hl_thread *hl_thread_start( void *callback, void *param, bool withGC ) {
	if( withGC ) hl_error("Threads with garbage collector are currently not supported");
#	ifdef HL_WIN
	DWORD tid;
	HANDLE h = CreateThread(NULL,0,callback,param,0,&tid);
	if( h == NULL )
		return NULL;
	CloseHandle(h);
	return (hl_thread*)(int_val)tid;
#	else
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if( pthread_create(&t,&attr,callback,param) != 0 ) {
		pthread_attr_destroy(&attr);
		return NULL;
	}
	pthread_attr_destroy(&attr);
	return (hl_thread*)t;
#	endif
}

HL_PRIM bool hl_thread_pause( hl_thread *t, bool pause ) {
#	ifdef HL_WIN
	bool ret;
	HANDLE h = OpenThread(THREAD_ALL_ACCESS,FALSE,(DWORD)(int_val)t);
	if( pause ) 
		ret = ((int)SuspendThread(h)) >= 0;
	else {
		int r;
		while( (r = (int)ResumeThread(h)) > 0 ) {
		}
		ret = r == 0;
	}
	CloseHandle(h);
	return ret;
#	else
	// TODO : use libthread_db
	return false;
#	endif
}

HL_PRIM int hl_thread_context_size() {
#	ifdef HL_WIN
	return (sizeof(CONTEXT) + sizeof(int_val) - 1) / sizeof(int_val);
#	else
	return 0;
#	endif
}

HL_API int hl_thread_context_index( const char *name ) {
#	ifdef HL_WIN
	CONTEXT *c = NULL;
#	define _ADDR(__r) (int)(((int_val)&c->__r) / sizeof(int_val))
#	ifdef HL_64
#		define ADDR(_,r) _ADDR(r)
#	else
#		define ADDR(r,_) _ADDR(r)
#	endif
	if( strcmp(name,"eip") == 0 )
		return ADDR(Eip,Rip);
	if( strcmp(name,"esp") == 0 )
		return ADDR(Esp,Rsp);
	return -1;
#	else
	return -1;
#	endif
#	undef ADDR
#	undef _ADDR
}

HL_API bool hl_thread_get_context( hl_thread *t, hl_thread_registers *regs ) {
#	ifdef HL_WIN
	HANDLE h = OpenThread(THREAD_ALL_ACCESS,FALSE,(DWORD)(int_val)t);
	bool ret;
	CONTEXT *c = (CONTEXT*)regs;
	c->ContextFlags = CONTEXT_FULL;
	ret = (bool)GetThreadContext(h,c);
	CloseHandle(h);
	return ret;
#	else
	return false;
#	endif
}

HL_API bool hl_thread_set_context( hl_thread *t, hl_thread_registers *regs ) {
#	ifdef HL_WIN
	HANDLE h = OpenThread(THREAD_ALL_ACCESS,FALSE,(DWORD)(int_val)t);
	bool ret;
	CONTEXT *c = (CONTEXT*)regs;
	c->ContextFlags = CONTEXT_FULL;
	ret = (bool)SetThreadContext(h,c);
	CloseHandle(h);
	return ret;
#	else
	return false;
#	endif
}

// Lock

static void lock_finalize(hl_lock* l)
{
#ifdef HL_WIN
  CloseHandle(l->semaphore);
#else

#endif
}

HL_PRIM hl_lock* hl_lock_alloc()
{
  hl_lock* l = (hl_lock*)hl_gc_alloc_finalizer(sizeof(hl_lock));
  l->locked = false;
  l->finalize = lock_finalize;
#ifdef HL_WIN
  l->semaphore = CreateSemaphore(NULL, 0, (1 << 30), NULL);
  // TODO: Error handling
#else
  hl_error("Lock not available on this platform");
#endif
  return l;
}

HL_PRIM bool hl_lock_wait(hl_lock* l, double timeout)
{
#ifdef HL_WIN
  if (l->locked) return false;
  l->locked = true;
  switch (WaitForSingleObjectEx(l->semaphore, (timeout == -1 ? INFINITE : timeout*1000.0), false) != WAIT_TIMEOUT)
  {
  case WAIT_ABANDONED:
  case WAIT_OBJECT_0:
    l->locked = false;
    return true;
  case WAIT_TIMEOUT:
    l->locked = false;
    return false;
  default:
    // TODO: Error
    l->locked = false;
    return false;
  }
#else
  return false;
#endif
}

HL_PRIM void hl_lock_release(hl_lock* l)
{
#ifdef HL_WIN
  if (l->locked)
  {
    ReleaseSemaphore(l->semaphore, 1, NULL);
  }
#endif
}

// Mutex

static void mutex_finalize(hl_mutex* m)
{
#ifdef HL_WIN
  DeleteCriticalSection(&m->cs);
#else

#endif
}

HL_PRIM hl_mutex* hl_mutex_alloc()
{
  hl_mutex* m = (hl_mutex*)hl_gc_alloc_finalizer(sizeof(hl_mutex));
  m->finalize = mutex_finalize;
#ifdef HL_WIN
  InitializeCriticalSection(&m->cs);
#else
  hl_error("Mutex not supported on this platform");
#endif
  return m;
}

HL_PRIM void hl_mutex_acquire(hl_mutex* m)
{
#ifdef HL_WIN
  EnterCriticalSection(&m->cs);
#else
  hl_error("Mutex not supported on this platform");
#endif
}

HL_PRIM bool hl_mutex_try_acquire(hl_mutex* m)
{
#ifdef HL_WIN
  return TryEnterCriticalSection(&m->cs);
#else
  hl_error("Mutex not supported on this platform");
#endif
}

HL_PRIM void hl_mutex_release(hl_mutex* m)
{
#ifdef HL_WIN
  LeaveCriticalSection(&m->cs);
#else
  hl_error("Mutex not supported on this platform");
#endif
}

// Deque

#ifdef HL_WIN
#define DEQUE_LOCK(l)   EnterCriticalSection(&(l))
#define DEQUE_UNLOCK(l) LeaveCriticalSection(&(l))
#define DEQUE_SIGNAL(l) ReleaseSemaphore(l,1,NULL)
#else
#define DEQUE_LOCK(l)   pthread_mutex_lock(&(l))
#define DEQUE_UNLOCK(l) pthread_mutex_unlock(&(l))
#define DEQUE_SIGNAL(l) pthread_cond_signal(&(l))
#endif

static void deque_finalize(hl_deque* d)
{
#ifdef HL_WIN
  DeleteCriticalSection(&d->lock);
  CloseHandle(d->wait);
#else

#endif
}

HL_PRIM hl_deque* hl_deque_alloc()
{
  hl_deque* d = (hl_deque*)hl_gc_alloc_finalizer(sizeof(hl_deque));
  d->finalize = deque_finalize;
#ifdef HL_WIN
  d->wait = CreateSemaphore(NULL, 0, (1 << 30), NULL);
  InitializeCriticalSection(&d->lock);
#else

#endif
  return d;
}

HL_PRIM void hl_deque_add(hl_deque* d, vdynamic* value)
{
  hl_queue* q = (hl_queue*)hl_gc_alloc_raw(sizeof(hl_queue)); // Not sure I'm doing it right
  q->msg = value;
  q->next = NULL;
  DEQUE_LOCK(d->lock);
  if (d->last == NULL)
    d->first = q;
  else
    d->last->next = q;
  d->last = q;
  DEQUE_SIGNAL(d->wait);
  DEQUE_UNLOCK(d->lock);
}

HL_PRIM void hl_deque_push(hl_deque* d, vdynamic* value)
{
  hl_queue* q = (hl_queue*)hl_gc_alloc_raw(sizeof(hl_queue)); // Not sure I'm doing it right
  q->msg = value;
  q->next = NULL;
  DEQUE_LOCK(d->lock);
  q->next = d->first;
  d->first = q;
  if (d->last == NULL) d->last = q;
  DEQUE_SIGNAL(d->wait);
  DEQUE_UNLOCK(d->lock);
}

HL_PRIM vdynamic* hl_deque_pop(hl_deque* d, bool block)
{
  vdynamic* msg;
  DEQUE_LOCK(d->lock);
  while (d->first == NULL)
  {
    if (block)
    {
#ifdef HL_WIN
      DEQUE_UNLOCK(d->lock);
      WaitForSingleObject(d->wait, INFINITE);
#else

#endif
    }
    else
    {
      DEQUE_UNLOCK(d->lock);
      return NULL;
    }
  }
  msg = d->first->msg;
  d->first = d->first->next;
  if (d->first == NULL) d->last = NULL;
  else DEQUE_SIGNAL(d->wait);
  
  DEQUE_UNLOCK(d->lock);
  return msg;
}

#define _LOCK _ABSTRACT(hl_lock)
DEFINE_PRIM(_LOCK, lock_alloc, _NO_ARG);
DEFINE_PRIM(_BOOL, lock_wait, _LOCK _F64);
DEFINE_PRIM(_VOID, lock_release, _LOCK);

#define _MUTEX _ABSTRACT(hl_mutex)
DEFINE_PRIM(_MUTEX, mutex_alloc, _NO_ARG);
DEFINE_PRIM(_VOID, mutex_acquire, _MUTEX);
DEFINE_PRIM(_BOOL, mutex_try_acquire, _MUTEX);
DEFINE_PRIM(_VOID, mutex_release, _MUTEX);

#define _DEQUE _ABSTRACT(hl_deque)
DEFINE_PRIM(_DEQUE, deque_alloc, _NO_ARG);
DEFINE_PRIM(_VOID, deque_add, _DEQUE _DYN);
DEFINE_PRIM(_VOID, deque_push, _DEQUE _DYN);
DEFINE_PRIM(_DYN, deque_pop, _DEQUE _BOOL);

#define _THREAD _ABSTRACT(hl_thread)
DEFINE_PRIM(_THREAD, thread_current, _NO_ARG);
//DEFINE_PRIM(_I32, thread_id, _NO_ARG);
DEFINE_PRIM(_THREAD, thread_start, _FUN(_VOID, _TYPE) _TYPE _BOOL);
DEFINE_PRIM(_BOOL, thread_pause, _THREAD _BOOL);
//DEFINE_PRIM(_I32, thread_context_size, _THREAD);