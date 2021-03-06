// sync.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"

#include <iostream>
#include <thread>
#include <mutex>
#include <algorithm>    // std::min_element, std::max_element

#include "fast_mutex.h"
#include "rpcs3_mutex.h"

static const uint64_t num_threads = 8;
static const uint64_t num_iterations = (1ull << 20);
static const uint64_t increment = 1;

static uint64_t g_counter = 0;

// std mutex
std::mutex g_std_mutex;

// mutex
HANDLE ghMutex;

// critical section
CRITICAL_SECTION g_crit_section;

// Slim reader/writer lock
SRWLOCK g_srw_lock;

// Fast mutex
fast_mutex g_fast_mutex;

// RPCS3 - mutex
shared_mutex g_rpcs3_mutex;

using func_ptr = void(*)();
using wait_func_ptr = void(*)(uint64_t usec);

void thread_func_no_mutex()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		g_counter += increment;
	}

}

void thread_func_std_mutex() 
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		std::lock_guard<std::mutex> guard(g_std_mutex);
		g_counter += increment;
	}

}

void thread_func_mutex()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		WaitForSingleObject(
			ghMutex,    // handle to mutex
			INFINITE);
		g_counter += increment;
		ReleaseMutex(ghMutex);
	}

}

void thread_func_critical_section()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		EnterCriticalSection(&g_crit_section);
		g_counter += increment;
		LeaveCriticalSection(&g_crit_section);
	}

}

void thread_func_srw_read()
{
	uint64_t a = 0;
	for (uint64_t i = 0; i < num_iterations; i++) {
		AcquireSRWLockShared(&g_srw_lock);
		a += g_counter;
		ReleaseSRWLockShared(&g_srw_lock);
	}

}

void thread_func_srw_write()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		AcquireSRWLockExclusive(&g_srw_lock);
		g_counter += increment;
		ReleaseSRWLockExclusive(&g_srw_lock);
	}

}

void thread_func_fast_mutex()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		fast_mutex_lock(&g_fast_mutex);
		g_counter += increment;
		fast_mutex_unlock(&g_fast_mutex);
	}

}

void thread_func_rpcs3_mutex_read()
{
	uint64_t a = 0;
	for (uint64_t i = 0; i < num_iterations; i++) {
		g_rpcs3_mutex.lock_shared();
		a += g_counter;
		g_rpcs3_mutex.unlock_shared();
	}

}

// lock - write lock
void thread_func_rpcs3_mutex_write()
{
	for (uint64_t i = 0; i < num_iterations; i++) {
		g_rpcs3_mutex.lock();
		g_counter += increment;
		g_rpcs3_mutex.unlock();
	}

}

// Test read shared mutex performance
void test_read(func_ptr threadFunc)
{
	std::thread t[num_threads];
	g_counter = 0;

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	//Launch a group of threads
	for (int i = 0; i < num_threads; ++i) {
		t[i] = std::thread(threadFunc);
	}

	//Join the thread with the main thread
	for (int i = 0; i < num_threads; ++i) {
		t[i].join();
	}

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	uint64_t delta = end.QuadPart - start.QuadPart;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	uint64_t timemsec = delta * 1000 / freq.QuadPart;
	std::cout << timemsec << " msec\n";

}


// Test write exclusive mutex performance
void test_write(func_ptr threadFunc)
{
	std::thread t[num_threads];
	g_counter = 0;

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	//Launch a group of threads
	for (int i = 0; i < num_threads; ++i) {
		t[i] = std::thread(threadFunc);
	}

	//Join the thread with the main thread
	for (int i = 0; i < num_threads; ++i) {
		t[i].join();
	}

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	//std::cout << "Counter = " << g_counter << "\n";
	//std::cout << "Should be = " << num_iterations * num_threads * increment << "\n";

	if (g_counter == (num_iterations * num_threads * increment))
	{
		//std::cout << "Match\n";
	}
	else {
		std::cout << "ERROR - no match\n";
	}

	uint64_t delta = end.QuadPart- start.QuadPart;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	uint64_t timemsec = delta * 1000 / freq.QuadPart;
	std::cout << timemsec << " msec\n";

}

// Test read/write mutex performance

void test_read_write(func_ptr threadReadFunc, func_ptr threadWriteFunc)
{
	std::thread t[num_threads];
	g_counter = 0;

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	//Launch a group of threads
	for (int i = 0; i < num_threads; ++i) {
		t[i] = std::thread(threadReadFunc);
	}

	// One writer thread
	std::thread w(threadWriteFunc);

	//Join the thread with the main thread
	for (int i = 0; i < num_threads; ++i) {
		t[i].join();
	}

	w.join();

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	//std::cout << "Counter = " << g_counter << "\n";
	//std::cout << "Should be = " << num_iterations * increment << "\n";

	if (g_counter == (num_iterations * increment))
	{
		//std::cout << "Match\n";
	}
	else {
		std::cout << "ERROR - no match\n";
	}

	uint64_t delta = end.QuadPart - start.QuadPart;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	uint64_t timemsec = delta * 1000 / freq.QuadPart;
	std::cout << timemsec << " msec\n";

}

void test_all_mutex()
{
	InitializeCriticalSection(&g_crit_section);
	InitializeSRWLock(&g_srw_lock);
	init_fast_mutex_handle();
	fast_mutex_init(&g_fast_mutex);

	std::cout << "Basic Mutex Tests\n";
	std::cout << "=================\n";
	std::cout << "No Mutex\n";
	test_write(thread_func_no_mutex);

	std::cout << "Std Mutex\n";
	test_write(thread_func_std_mutex);

#if 0	// very slow
	ghMutex = CreateMutex(
		NULL,              // default security attributes
		FALSE,             // initially not owned
		NULL);

	std::cout << "Mutex\n";
	test(thread_func_mutex);

	CloseHandle(ghMutex);
#endif


#if 1	// faster than mutex, but still slow
	std::cout << "Critical Section\n";
	test_write(thread_func_critical_section);
#endif


	std::cout << "Fast mutex lock\n";
	test_write(thread_func_fast_mutex);

	std::cout << "\nR/W Lock Tests (read)\n";
	std::cout << "=======================\n";

	std::cout << "Slim reader/writer lock (read)\n";
	test_read(thread_func_srw_read);

	std::cout << "rpcs3 r/w lock (read)\n";
	test_read(thread_func_rpcs3_mutex_read);

	std::cout << "\nR/W Lock Tests (write)\n";
	std::cout << "========================\n";

	std::cout << "Slim reader/writer lock (write)\n";
	test_write(thread_func_srw_write);

	std::cout << "rpcs3 mutex lock (write)\n";
	test_write(thread_func_rpcs3_mutex_write);


	std::cout << "\nR/W Lock Tests (read/write)\n";
	std::cout << "=============================\n";

	std::cout << "Slim reader/writer lock (read/write)\n";
	test_read_write(thread_func_srw_read, thread_func_srw_write);

	std::cout << "rpcs3 mutex lock (read/write)\n";
	test_read_write(thread_func_rpcs3_mutex_read, thread_func_rpcs3_mutex_write);
}


void wait_sleep_for(uint64_t usec)
{
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}


#include "dynamic_library.h"
DYNAMIC_IMPORT("ntdll.dll", NtWaitForKeyedEvent, NTSTATUS(HANDLE Handle, PVOID Key, BOOLEAN Alertable, PLARGE_INTEGER Timeout));

void wait_rpcs3(uint64_t usec)
{

#if 0
	atomic_t<u32> m_value{ 0 };

	LARGE_INTEGER timeout;
	timeout.QuadPart = usec * -10;

	HRESULT rc = NtWaitForKeyedEvent(nullptr, &m_value, false, &timeout);
#else
	atomic_t<u32> m_value{ 1 };

	LARGE_INTEGER timeout;
	timeout.QuadPart = usec * -10;

	if (HRESULT rc = NtWaitForKeyedEvent(nullptr, &m_value, false, &timeout))
	{
		verify(HERE), rc == WAIT_TIMEOUT;

		// Retire
		while (!m_value.fetch_op([](u32& value) { if (value) value--; }))
		{
			timeout.QuadPart = 0;

			if (HRESULT rc2 = NtWaitForKeyedEvent(nullptr, &m_value, false, &timeout))
			{
				verify(HERE), rc2 == WAIT_TIMEOUT;
				SwitchToThread();
				continue;
			}

			return;
		}

		return;
	}

#endif


}

// Test wait performance
void test_wait(wait_func_ptr waitFunc, uint64_t usec)
{
	const int num_iterations = 10;
	uint64_t results[num_iterations];
	uint64_t sum = 0;

	for (int i = 0; i < num_iterations; i++) {
		LARGE_INTEGER start;
		QueryPerformanceCounter(&start);

		waitFunc(usec);

		LARGE_INTEGER end;
		QueryPerformanceCounter(&end);

		uint64_t delta = end.QuadPart - start.QuadPart;
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		results[i] = delta * 1000000 / freq.QuadPart;
		sum += results[i];
	}

	std::cout << "   average = " << sum / num_iterations << " usec\n";
	std::cout << "   min = " << *std::min_element(results, results + num_iterations) << " usec\n";
	std::cout << "   max = " << *std::max_element(results, results + num_iterations) << " usec\n";
}

void test_all_wait()
{
	for (int usec = 1000; usec >= 0 ; usec = usec - 50)
	{
		std::cout << "\n\nTesting wait for usec = " << usec << "\n";

		//std::cout << "\n   sleep_for\n";
		//test_wait(wait_sleep_for, usec);

		std::cout << "\n   rpcs3 wait\n";
		test_wait(wait_rpcs3, usec);

	}
}


void test_busy_wait()
{
	std::cout << "Testing busy_wait - 4000 cycles\n";

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	for (int i = 0; i < 1000; i++) {
		busy_wait(4000);
	}


	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	uint64_t delta = end.QuadPart - start.QuadPart;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	uint64_t ns = delta * 1000000000 / freq.QuadPart;

	std::cout << "Average time = " << ns << " ns\n";

}

int main()
{
	//test_all_mutex();

	test_all_wait();

	test_busy_wait();


    return 0;
}
