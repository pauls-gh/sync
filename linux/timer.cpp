#include <iostream>
#include <sys/time.h>
#include "Atomic.h"
#include "sync.h"

using u32 = uint32_t;
using func_ptr = void (*)(uint64_t timeout);


void test_usleep(uint64_t usec)
{
    usleep(usec);
}

void test_nsleep(uint64_t usec)
{
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = usec * 1000;
    nanosleep(&req, nullptr);
}


void rpcs_sleep_usec(uint64_t _timeout)
{
    atomic_t<u32> m_value{0};
    u32 _old = 0;

	if (!_timeout)
	{
		//verify(HERE), m_value--;
		return;
	}

	timespec timeout;
	timeout.tv_sec  = _timeout / 1000000;
	timeout.tv_nsec = (_timeout % 1000000) * 1000;

	for (u32 value = _old + 1;; value = m_value)
	{
		const int err = futex((int*)&m_value.raw(), FUTEX_WAIT_PRIVATE, value, &timeout, nullptr, 0) == 0
			? 0
			: errno;

		// Normal or timeout wakeup
		if (!err || (err == ETIMEDOUT))
		{
			// Cleanup (remove waiter)
			//verify(HERE), m_value--;
			return;
		}

		// Not a wakeup
		//verify(HERE), err == EAGAIN;
	}

}


void test_sleep(func_ptr sleepfunc)
{
    for (int time = 1; time < 5000; time = time << 1) {
        std::cout << "Testing usec = " << time << "\n";

        const int num_iter = 1000;
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        for (int i = 0; i < num_iter; i++) {
            sleepfunc(time);
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        u64 start_usec = start.tv_sec * 1000000 + start.tv_nsec / 1000;
        u64 end_usec = end.tv_sec * 1000000 + end.tv_nsec / 1000;
        
        std::cout << "   Actual = " << (end_usec - start_usec) / num_iter << " (usec, average)\n";

    }

}
int main()
{
    struct timespec res;
    clock_getres(CLOCK_MONOTONIC, &res);
    std::cout << "Clock Resolution. sec = " << res.tv_sec << ", nsec = " << res.tv_nsec << "\n";



    std::cout << "Test rpcs3 usleep\n";
    test_sleep(rpcs_sleep_usec);

    std::cout << "Test usleep\n";
    test_sleep(test_usleep);

    std::cout << "Test nano_sleep\n";
    test_sleep(test_nsleep);

    return 0;
}