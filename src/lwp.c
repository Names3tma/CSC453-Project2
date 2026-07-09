#include "../include/lwp.h"
#include <sys/resource.h> // for things like rlimit
#include <sys/mman.h> // PROT 
#include <unistd.h>

static tid_t next_tid = 1;

static thread all_threads = NULL;
static thread current_thread = NULL;

static thread term_head = NULL;
static thread term_tail = NULL;
static thread wait_head = NULL;
static thread wait_tail = NULL;

extern scheduler RoundRobin;
static scheduler current_scheduler = NULL;

scheduler lwp_get_scheduler(void) 
{
	if (current_scheduler == NULL) {current_scheduler = RoundRobin;}
	return current_scheduler;
}

void lwp_set_scheduler(scheduler fun) {current_scheduler = fun;}

static void lwp_wrap(lwpfun fun, void *arg)
{
	int rval;
	rval = fun(arg);
	lwp_exit(rval);
}

void add_lib_list(thread new)
{
	if (all_threads == NULL)
	{
		all_threads = new;
		new->lib_one = new;
		new->lib_two = new;
	}
	else
	{
		thread tail = all_threads->lib_two;
		tail->lib_one = new;
		new->lib_two = tail;
		new->lib_one = all_threads;
		all_threads->lib_two = new;
	}
}

tid_t lwp_create(lwpfun function, void *argument)
{
	thread new = malloc(sizeof(context));
	long page_size; 
	size_t howbig;
	struct rlimit rl;

	// finding stack size
	page_size = sysconf(_SC_PAGE_SIZE);	
	if (getrlimit(RLIMIT_STACK, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) 
	{
		howbig = rl.rlim_cur;
	}
	else 
	{
		howbig = 8 * 1024 * 1024; // default value
	}
	howbig = (howbig + page_size - 1) / page_size * page_size; // round up
	// stack initialization, read and write but no execute
	new->stack = mmap(NULL, howbig, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1, 0);
	if (new->stack == MAP_FAILED)
	{
		free(new);
		return NO_THREAD;
	}
	
	// building stack frame, address of the stack
	unsigned long *stack_top = (unsigned long *)((char*)new->stack + howbig);
	stack_top[-1] = 0; // dead value, more for compile of lwp_wrap
	stack_top[-2] = (unsigned long)lwp_wrap; // reads ret into rip
	stack_top[-3] = 0; // reads leave into rbp

	// placeholder values
	new->state.rax = new->state.rbx = new->state.rcx = new->state.rdx = 0;
	new->state.r8 = 0;
	new->state.r9 = 0;
	new->state.r10 = 0;
	new->state.r11 = 0;
	new->state.r12 = 0;
	new->state.r13 = 0;
	new->state.r14 = 0;
	new->state.r15 = 0;

	// lwp_wrap arguements and functions
	new->state.rsi = (unsigned long) argument;
	new->state.rdi = (unsigned long) function;

	// stack and base pointers
	new->state.rbp = (unsigned long) &stack_top[-3];
	new->state.rsp = new->state.rbp;
	new->state.fxsave=FPU_INIT;

	new->tid = next_tid++;
	new->status = MKTERMSTAT(LWP_LIVE, 0);
	new->sched_one = new->sched_two = NULL;
	new->lib_one = new->lib_two = NULL;
	new->exited = NULL;
	
	add_lib_list(new);

	lwp_get_scheduler()->admit(new); // gets the current scheduler, runns their admit

	return new->tid;
	
}

void remove_lib_list(thread victim)
{
	if (victim->lib_one == victim) {all_threads = NULL;}
	else
	{
		victim->lib_two->lib_one = victim->lib_one;
		victim->lib_one->lib_two = victim->lib_two;
		if(all_threads == victim) {all_threads = victim->lib_one;}
	}
	victim->lib_one = victim->lib_two = NULL;
}

thread tid2thread(tid_t tid)
{
	if (all_threads == NULL) {return NULL;}
	thread cur = all_threads;
	do // use do since because the list is circular, we want to end at head(2nd time)
	{
		if (cur->tid == tid) {return cur;}
		cur = cur->lib_one;
	} while (cur != all_threads);
	return NULL;
}

void lwp_yield(void)
{
	thread old = current_thread;
	thread next = lwp_get_scheduler()->next();

	if (next == NULL) {exit(LWPTERMSTAT(old->status));} // exit with old status, from spec

	current_thread = next;
	swap_rfiles(&old->state, &next->state);
}

void lwp_start(void)
{
	thread start = malloc(sizeof(context));
	
	start->tid = next_tid++;
	start->stacksize = 0;
	start->status = MKTERMSTAT(LWP_LIVE, 0);
	start->stack = NULL; 
	start->lib_one = start->lib_two = start->sched_one = start->sched_two = start->exited = NULL;
	
	add_lib_list(start);
	lwp_get_scheduler()->admit(start);

	current_thread = start;

	lwp_yield();
}

tid_t lwp_gettid(void)
{
	if (current_thread == NULL) {return NO_THREAD;}
	return current_thread->tid;
}

void lwp_exit(int status)
{
	thread cur = current_thread;

	cur->status = MKTERMSTAT(LWP_TERM, status); // kill thread
	lwp_get_scheduler()->remove(cur); // remove from schedule

	if (wait_head != NULL) // someone is waiting for a thread
	{
		thread waiter = wait_head;
		wait_head = waiter->sched_one;
		if (wait_head == NULL) {wait_tail = NULL;}
		waiter->sched_one = waiter->sched_two = NULL;

		waiter->exited = cur;
		lwp_get_scheduler()->admit(waiter); // continue queued wait
	}
	else // no one is waiting, save at term, lwp_wait will handle
	{
		cur->sched_one = NULL;
		cur->sched_two = term_tail;
		if (term_tail != NULL) {term_tail->sched_one = cur;}
		term_tail = cur;
		if (term_head == NULL) {term_head = cur;}
	}
	lwp_yield();
}

tid_t lwp_wait(int *status)
{
	thread finish;
	
	if (term_head != NULL) // when something term, procceed with cleanup
	{
		finish = term_head;
		term_head = finish->sched_one;
		if (term_head == NULL) {term_tail = NULL;}
		else {term_head->sched_two = NULL;}
		finish->sched_one = finish->sched_two = NULL;
	}	
	else 
	{
		// no termination, no block
		if (lwp_get_scheduler()->qlen() <= 1) {return NO_THREAD;}
		
		// block, join the queue
		thread cur = current_thread;
		lwp_get_scheduler()->remove(cur);
		
		cur->sched_one = NULL;
		cur->sched_two = wait_tail;
		if (wait_tail != NULL) {wait_tail->sched_one = cur;}
		wait_tail = cur;
		if(wait_head == NULL) {wait_head = cur;}
	
		lwp_yield();

		finish = cur->exited; // grabs the thread that woke up the current (done by exit) 
	}
	
	tid_t tid = finish->tid;
	if (status != NULL) {*status = finish->status;} // user wants the exit status
	
	// free but for mmap, just frees the area of the stack
	if (finish->stack != NULL) {munmap(finish->stack, finish->stacksize);}

	remove_lib_list(finish);
	free(finish);
	
	return tid;
}





