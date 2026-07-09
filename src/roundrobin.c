#include "../include/lwp.h"

static thread head = NULL;
static int num_threads = 0;

void rr_admit(thread new)
{
	if (head == NULL)
	{
		head = new;
		new->sched_one = new;
		new->sched_two = new;
	}
	else // adding onto the end of the list, meaning behind the head
	{
		thread tail = head->sched_two;
		tail->sched_one= new;
		new->sched_two = tail;
		new->sched_one = head;
		new->sched_two = tail;
	}
	num_threads++;
}

void rr_remove(thread victim)
{
	if (num_threads == 0) return;
	if (victim->sched_one == victim) head = NULL; // one thread
	else
	{
		victim->sched_two->sched_one = victim->sched_one; // unlink
		victim->sched_one->sched_two = victim->sched_two;
		if (head == victim) head = victim->sched_one;
	}
	victim->sched_one = victim->sched_two = NULL;
	num_threads--;
}

thread rr_next()
{
	if (head == NULL) return NULL;
	thread ret = head;
	head = head->sched_one;
	return ret;
}

int rr_qlen() {return num_threads;}

struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next, rr_qlen};
scheduler RoundRobin = &rr_publish;
