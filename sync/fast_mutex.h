#pragma once

typedef union fast_mutex fast_mutex;
union fast_mutex
{
	long waiters;
	unsigned char owned;
};

void init_fast_mutex_handle(void);
void fast_mutex_init(fast_mutex *m);
void fast_mutex_lock(fast_mutex *m);
int fast_mutex_trylock(fast_mutex *m);
void fast_mutex_unlock(fast_mutex *m);


