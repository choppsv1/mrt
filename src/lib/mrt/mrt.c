/*
 * $Id: mrt.c,v 1.1.1.1 2000/08/14 18:46:11 labovit Exp $
 */

#include <mrt.h>
#ifdef NT
#include <ntconfig.h>
#include <process.h>
#include <io.h>

int _open(const char *filename, int oflag, ...);
#ifndef SIGALRM
#define SIGALRM	SIGTERM
#endif /* SIGALRM */
#endif /* NT */

mrt_t *MRT;


void
mrt_exit (status)
    int status;
{
    if (status < 0) {
        trace (INFO, MRT->trace, "abort\n");
	abort ();
    }
    trace (INFO, MRT->trace, "exit (%d)\n", status);
    exit (status);
}
  

/* mrt_thread_create
 * a wrapper around pthread create, but also adds
 * thread to bookkeeping structures
 * Note: not locking down structure because (for moment) only main
 *       thread calls this routine. Maybe this will change.
 */

/* I'm changing a way to handle pthread_attr being passed to pthread_create ().
   man page doesn't say if I can destroy it right after pthread_create (),
   so I'm saving it into mrt_thread_t structure to delete it when exiting. */

mrt_thread_t *mrt_thread_create (char *name, schedule_t *schedule,
				 thread_fn_t call_fn, void *arg) 
{
  mrt_thread_t *mrt_thread;
  pthread_t thread;
#ifndef HAVE_LIBPTHREAD
  pthread_t save;
#endif /* HAVE_LIBPTHREAD */
  pthread_attr_t tattr;

  /* Locking is required to avoid a confusion 
     in case a created thread executes mrt_thread_exit()
     before adding a mrt_thread structure. */
  pthread_mutex_lock (&MRT->mutex_lock);

  pthread_attr_init (&tattr);
#ifdef HAVE_LIBPTHREAD
  /* as long as I remember, the following attributes are required
     to reuse a thread id. */
  pthread_attr_setscope (&tattr, PTHREAD_SCOPE_SYSTEM);
  /* make the thread detached since mrt doesn't wait it by pthread_join */
  pthread_attr_setdetachstate (&tattr, PTHREAD_CREATE_DETACHED);

  if (pthread_create (&thread, &tattr, (thread_fn_t) call_fn, arg) < 0) {
    trace (TR_ERROR, MRT->trace, "pthread_create %s (%m)\n", name);
    pthread_mutex_unlock (&MRT->mutex_lock);
    pthread_attr_destroy (&tattr);
    return (NULL);
  }

#else
  thread = ++MRT->threadn;
  /* a call should be delayed at the end */
#endif /* HAVE_LIBPTHREAD */

  mrt_thread = New (mrt_thread_t);
  /* this expects New () clears the memory */
  mrt_thread->name = strdup ((name)? name: "");
  mrt_thread->thread = thread;
  mrt_thread->schedule = schedule;
  mrt_thread->attr = tattr;
  LL_Add (MRT->ll_threads, mrt_thread);
  pthread_mutex_unlock (&MRT->mutex_lock);

  if (schedule)
     schedule->self = thread;
#ifndef HAVE_LIBPTHREAD
  save = set_thread_id (thread);
  (*call_fn) (arg);
  set_thread_id (save);
#endif /* HAVE_LIBPTHREAD */
  trace (TR_THREAD, MRT->trace, "thread %s id %ld created\n", mrt_thread->name,
	 thread);
  return (mrt_thread);
}


static void
mrt_thread_process_schedule (event_t *event)
{
    schedule_t *schedule = event->args[1];

    init_mrt_thread_signals ();
    if (event->call_fn)
	event->call_fn (event->args[0]);

    Deref_Event (event);
    if (schedule == NULL)
	mrt_thread_exit ();
#ifdef HAVE_LIBPTHREAD
    while (1)
        schedule_wait_for_event (schedule);
    /* NOT REACHED */
#else
    /* We should put the schedule into the global linked list here
       so that they will be processed by the main program sequentially.
       But, they are already on the list when it was created. */
#endif /* HAVE_LIBPTHREAD */
}


mrt_thread_t *
mrt_thread_create2 (char *name, schedule_t *schedule,
		    thread_fn_t init_fn, void *arg) 
{
    event_t *event;

    /* here I borrowed event structure 
       since pthread_create can pass only one argument */
    event = New_Event (2);
    event->call_fn = (event_fn_t) init_fn;
    event->args[0] = arg;
    event->args[1] = schedule;
    return (mrt_thread_create (name, schedule, 
			       (thread_fn_t) mrt_thread_process_schedule, 
			       event));
}


/*
 * mrt_thread_exit
 */
void mrt_thread_exit (void) {
  mrt_thread_t *mrt_thread;
  pthread_t thread = pthread_self ();

  assert (thread > 0);
  pthread_mutex_lock (&MRT->mutex_lock);
  LL_Iterate (MRT->ll_threads, mrt_thread) {
     if (mrt_thread->thread == thread)
     	break;
  }
  assert (mrt_thread);
  LL_Remove (MRT->ll_threads, mrt_thread);
  pthread_mutex_unlock (&MRT->mutex_lock);
  trace (TR_THREAD, MRT->trace, "thread %s exit\n", mrt_thread->name);
#ifdef HAVE_LIBPTHREAD
  pthread_attr_destroy (&mrt_thread->attr);
#endif /* HAVE_LIBPTHREAD */
  Delete (mrt_thread->name);
  Delete (mrt_thread);
#ifdef HAVE_LIBPTHREAD
  pthread_exit (NULL);
#endif /* HAVE_LIBPTHREAD */
}


#ifdef notdef
/* be careful that FreeBSD 3.1 doesn't have pthread_cancel() */
void 
mrt_thread_kill_all (void)
{
  mrt_thread_t *mrt_thread;
  pthread_t thread = pthread_self ();

  pthread_mutex_lock (&MRT->mutex_lock);
  LL_Iterate (MRT->ll_threads, mrt_thread) {
     if (mrt_thread->thread != thread) {
        mrt_thread_t *prev;
#ifdef HAVE_LIBPTHREAD
        pthread_cancel (mrt_thread->thread);
  	pthread_attr_destroy (&mrt_thread->attr);
#endif /* HAVE_LIBPTHREAD */
        Delete (mrt_thread->name);
        Delete (mrt_thread);
	prev = LL_GetPrev (MRT->ll_threads, mrt_thread);
	LL_Remove (MRT->ll_threads, mrt_thread);
	mrt_thread = prev;
    }
  }
  pthread_mutex_unlock (&MRT->mutex_lock);
}
#endif


#ifndef HAVE_LIBPTHREAD
#ifndef NT
/* use real assert */
#undef assert
#include </usr/include/assert.h>
#endif /* NT */
int
mrt_pthread_mutex_init (pthread_mutex_t *mutex, 
	const pthread_mutexattr_t *attr, char *file, int line)
{
    assert (mutex);
    mutex->value = 0;
    mutex->file = file;
    mutex->line = line;
    return (0);
}

int
mrt_pthread_mutex_lock (pthread_mutex_t *mutex, char *file, int line)
{
    assert (mutex);
    assert (mutex->value >= 0);
    while (mutex->value) {
        mrt_busy_loop (&mutex->value, 1);
    }
    mutex->value = 1;
    mutex->file = file;
    mutex->line = line;
    return (0);
}

int
mrt_pthread_mutex_trylock (pthread_mutex_t *mutex, char *file, int line)
{
    assert (mutex);
    assert (mutex->value >= 0);
    if (mutex->value == 0) {
	mutex->value = 1;
	mutex->file = file;
	mutex->line = line;
	return (0);
    }
    return (1);
}

int
mrt_pthread_mutex_unlock (pthread_mutex_t *mutex, char *file, int line)
{
    assert (mutex);
    assert (mutex->value >= 0);
    mutex->value = 0;
    mutex->file = file;
    mutex->line = line;
    return (0);
}

int
mrt_pthread_mutex_destroy (pthread_mutex_t *mutex, char *file, int line)
{
    assert (mutex);
    mutex->value = -1; /* destroyed */
    mutex->file = file;
    mutex->line = line;
    return (0);
}

int 
mrt_pthread_cond_init (pthread_cond_t *cond, const pthread_condattr_t *attr, 
		       char *file, int line)
{
    assert (cond);
    cond->value = 0;
    cond->file = file;
    cond->line = line;
    return (0);
}

int 
mrt_pthread_cond_wait (pthread_cond_t *cond, pthread_mutex_t *mutex, 
		       char *file, int line)
{
    assert (cond);
    assert (mutex);
    assert (cond->value >= 0);
    assert (mutex->value == 1);
    cond->file = file;
    cond->line = line;
    while (!cond->value) {
	pthread_mutex_unlock (mutex);
        mrt_busy_loop (&cond->value, 0);
	pthread_mutex_lock (mutex);
    }
    return (0);
}

int 
mrt_pthread_cond_signal (pthread_cond_t *cond, char *file, int line)
{
    assert (cond);
    assert (cond->value >= 0);
    cond->value = 1;
    cond->file = file;
    cond->line = line;
    return (0);
}

int 
mrt_pthread_cond_destroy (pthread_cond_t *cond, char *file, int line)
{
    assert (cond);
    cond->value = -1;
    cond->file = file;
    cond->line = line;
    return (0);
}

#endif /* HAVE_LIBPTHREAD */

static void
check_force_exit (void)
{
  if (MRT->force_exit_flag == MRT_FORCE_ABORT) {
    mrt_exit (-1);
  }
  if (MRT->force_exit_flag == MRT_FORCE_EXIT) {
    mrt_exit (0);
  }
  if (MRT->force_exit_flag == MRT_FORCE_REBOOT) {
    mrt_reboot ();
  }
  if (MRT->force_exit_flag != 0) {
    trace (TR_WARN, MRT->trace, "MRT signal (%d) received\n", 
	   MRT->force_exit_flag);
  }
}


#ifndef HAVE_LIBPTHREAD
/* no wait */
static int 
mrt_process_schedule (volatile int *force_exit_flag, int ok)
{
    int i;
    int ret = 0;
#ifdef NT
	MSG msg = { 0, 0, 0, 0 };
#endif /* NT */
    int imax = (MRT->initialization)? 1: 3;

    for (i = 0; i < imax; i++) {
        if (MRT->force_exit_flag != 0 ||
	        (force_exit_flag != NULL && *force_exit_flag != ok))
	    break;
	switch (i) {
	case 0:
	    ret = process_all_schedules ();
	    break;
	case 1:
            mrt_alarm ();
	    break;
	case 2:
#ifdef NT
				mrt_select2 (100);
#else
            mrt_select2 (0); /* no wait select */
#endif /* NT */
	    break;
		}

#ifdef NT
		// use PeekMessage(..), is nonblocking. note that second paramater is zero so we recieve
		// all messages for this thread, I believe that the second and third paramater are simply
		// telling the function to only look for WM_TIMER messages posted to the windows Queue.

		if ( PeekMessage(&msg, 0, WM_TIMER, WM_TIMER, PM_REMOVE) > 0) {
			DispatchMessage( &msg );
	}
#endif /* NT */



    }
    return (ret);
}


void 
mrt_busy_loop (volatile int *force_exit_flag, int ok)
{
    while (TRUE) {
	int i;
	int imax = (MRT->initialization)? 1: 3;

    	for (i = 0; i < imax; i ++) {
	  resume:
            if (MRT->force_exit_flag != 0 ||
	            (force_exit_flag != NULL && *force_exit_flag != ok)) {
  		check_force_exit ();
		return;
	    }
	    switch (i) {
	    case 0:
		/* process shcedule queue first */
      		if (mrt_process_schedule (force_exit_flag, ok))
		    goto resume;
		break;
	    case 1:
        	mrt_select (); /* long wait select */
		break;
	    case 2:
         	mrt_alarm ();
		break;
	    }
	}
    }
}


void mrt_switch_schedule (void) {
    mrt_process_schedule (NULL, 0);
}
#endif /* HAVE_LIBPTHREAD */


int
mrt_update_pid (void)
{
    int pid = getpid ();
    if (MRT->pid != pid) {
	trace (TR_INFO, MRT->trace, "PID updated (%d -> %d)\n",
	      MRT->pid, pid);
	MRT->pid = pid;
    }
    return (pid);
}


void mrt_main_loop (void) {
    /* may someone forget this */
    mrt_update_pid ();
#ifndef HAVE_LIBPTHREAD
   MRT->initialization = 0;
   mrt_busy_loop (NULL, 0);
#else
   /* starts select thread seperately */
   start_select ();
  /* loop */
  /* XXX we need to implement a mechanism to wait mrt threads */
  while (MRT->force_exit_flag == 0) {
    /* main thread catches alarm interrupts by sigwait() */
    mrt_alarm ();
  }
  check_force_exit ();
#endif /* HAVE_LIBPTHREAD */
}


void
mrt_set_force_exit (int code)
{
    MRT->force_exit_flag = code;
    /* simulate sigalarm to wake up the thread */
#ifdef NT
	exit(0);
#else
    kill (getpid (), SIGALRM);
#endif /* NT */
}


/* init_mrt
 * initialize MRT bookeeping structures. *ALL* MRT programs must call this 
 * routine.
 */
#ifdef HAVE_THR_SETCONCURRENCY
#include <thread.h>
#endif /* HAVE_THR_SETCONCURRENCY */

int init_mrt (trace_t *tr) {
   mrt_thread_t *mrt_thread;

   assert (MRT == NULL);
#ifdef HAVE_THR_SETCONCURRENCY
   /* NOTE: Solaris Thread, not Pthread */
   thr_setconcurrency (40);
#endif /* HAVE_THR_SETCONCURRENCY */

#ifndef NT
   signal (SIGPIPE, mrt_process_signal);
   signal (SIGHUP, mrt_process_signal);
   signal (SIGINT, mrt_process_signal);
#endif /* NT */

   MRT = New (mrt_t);
   MRT->start_time = time (NULL);
   MRT->ll_threads = LL_Create (0);
   MRT->ll_trace = LL_Create (0);
   MRT->ll_signal_call_fn = LL_Create (0);
   MRT->ll_gateways = LL_Create (0);
   MRT->trace = tr;
   MRT->config_file_name = NULL;
   MRT->pid = getpid ();
   MRT->version = MRT_VERSION;
   MRT->date = __DATE__;
   MRT->daemon_mode = 0;
   MRT->force_exit_flag = 0;
#ifndef HAVE_LIBPTHREAD
   MRT->initialization = 1;
#endif /* HAVE_LIBPTHREAD */

   pthread_mutex_init (&MRT->mutex_lock, NULL);
   init_schedules (tr);

   mrt_thread = New (mrt_thread_t);
   mrt_thread->name = strdup ("MAIN thread");
   mrt_thread->schedule = NULL;
   mrt_thread->thread = pthread_self ();
   LL_Add (MRT->ll_threads, mrt_thread);

#ifndef HAVE_LIBPTHREAD
   /* only used on non-thread capable machines */
   MRT->threadn = pthread_self ();
#endif /* HAVE_LIBPTHREAD */

   /* init_uii (tr); */
   init_timer (tr);
   init_select (tr);
#ifdef notdef
   /* some commands don't need information on own interfaces */
   init_interfaces (tr);
#endif

  return (1);
}


#undef open
#undef close
#undef socket
#undef accept

int 
mrt_open (const char *path, int flags, mode_t mode, char *s, int l)
{
#ifdef NT
	int r = _open (path, flags, mode);
#else
    int r = open (path, flags, mode);
#endif /* NT */
    trace (TR_TRACE, MRT->trace, "open (%s) -> %d in %s at %d\n", path,
	   r, s, l);
    return (r);
}

int 
mrt_close (int d, char *s, int l)
{

#ifndef NT
	int r = close (d);
#else
	int r = closesocket	(d);
#endif /* NT */

    trace (TR_TRACE, MRT->trace, "close (%d) -> %d in %s at %d\n", d, 
	   r, s, l);
    return (r);
}

int 
mrt_socket (int domain, int type, int protocol, char *s, int l)
{
    int r = socket (domain, type, protocol);
    trace (TR_TRACE, MRT->trace, "socket (%d,%d,%d) -> %d in %s at %d\n", 
	   domain, type, protocol, r, s, l);
    return (r);
}

int 
mrt_accept (int d, struct sockaddr *addr, int *addrlen, char *s, int l)
{
    int r = accept (d, addr, addrlen);
    trace (TR_TRACE, MRT->trace, "accept (%d) -> %d in %s at %d\n", 
	   d, r, s, l);
    return (r);
}


int 
afi2family (int afi)
{
    switch (afi) {
    case AFI_IP:
	return (AF_INET);
#ifdef AF_INET6
    case AFI_IP6:
	return (AF_INET6);
#endif /* AF_INET6 */
    default:
	break;
    }
    return (-1);
}

char * 
afi2string (int afi)
{
    switch (afi) {
    case AFI_IP:
	return ("ip");
#ifdef AFI_IP6
    case AFI_IP6:
	return ("ipv6");
#endif /* AFI_IP6 */
    default:
	break;
    }
    return ("?");
}

char * 
safi2string (int safi)
{
    switch (safi) {
    case SAFI_UNICAST:
	return ("unicast");
#ifdef SAFI_MULTICAST
    case SAFI_MULTICAST:
	return ("multicast");
#endif /* SAFI_MULTICAST */
    default:
	break;
    }
    return ("?");
}

int 
family2afi (int family)
{
    switch (family) {
    case AF_INET:
	return (AFI_IP);
#ifdef AF_INET6
    case AF_INET6:
	return (AFI_IP6);
#endif /* AF_INET6 */
    default:
	break;
    }
    return (-1);
}

#if 0
/* there is in mrt.c */
char * 
family2string (int family)
{
    switch (family) {
    case AF_INET:
	return ("inet");
#ifdef AF_INET6
    case AF_INET6:
	return ("inet6");
#endif /* AF_INET6 */
    default:
	break;
    }
    return ("?");
}
#endif

u_char *
sockunion2char (sockunion_t *u)
{
    if (u->sa.sa_family == AF_INET) {
	return ((u_char *) &u->sin.sin_addr);
    }
#ifdef HAVE_IPV6
    else if (u->sa.sa_family == AF_INET6) {
	return ((u_char *) &u->sin6.sin6_addr);
    }
#endif /* HAVE_IPV6 */
#ifdef AF_LINK
    else if (u->sa.sa_family == AF_LINK) {
	return ((u_char *) u->sdl.sdl_data);
    }
#endif /* AF_LINK */
    return (NULL);
}


int
ifzero (void *ptr, int size)
{
    u_char *p = ptr;
    int zero = 0;
    while (size--) {
        zero |= *p++;
    }
    return (zero == 0);
}


int
ifneg (void *a, void *c, int size)
{
    u_char *p = a;
    u_char *r = c;
    int zero = 0;
    while (size--) {
        zero |= ~(*p);
	if (r) {
            *r++ = ~(*p);
	}
	p++;
    }
    return (zero);
}


int
ifor (void *a, void *b, void *c, int size)
{
    u_char *p = a;
    u_char *q = b;
    u_char *r = c;
    int zero = 0;
    while (size--) {
        zero |= (*p | *q);
	if (r) {
            *r++ = (*p | *q);
	}
	p++;
	q++;
    }
    return (zero);
}


int
ifand (void *a, void *b, void *c, int size)
{
    u_char *p = a;
    u_char *q = b;
    u_char *r = c;
    int zero = 0;
    while (size--) {
        zero |= (*p & *q);
	if (r) {
            *r++ = (*p & *q);
	}
	p++;
	q++;
    }
    return (zero);
}


int
ifxor (void *a, void *b, void *c, int size)
{
    u_char *p = a;
    u_char *q = b;
    u_char *r = c;
    int zero = 0;
    while (size--) {
        zero |= (*p ^ *q);
	if (r) {
            *r++ = (*p ^ *q);
	}
	p++;
	q++;
    }
    return (zero);
}

