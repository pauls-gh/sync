#include "stdafx.h"
#include "windows.h"

#include <mutex>
#include <intrin.h>  
#include "dynamic_library.h"
#include "fast_mutex.h"

namespace utils
{
	dynamic_library::dynamic_library(const std::string &path)
	{
		load(path);
	}

	dynamic_library::~dynamic_library()
	{
		close();
	}

	bool dynamic_library::load(const std::string &path)
	{
#ifdef _WIN32
		m_handle = LoadLibraryA(path.c_str());
#else
		m_handle = dlopen(path.c_str(), RTLD_LAZY);
#endif
		return loaded();
	}

	void dynamic_library::close()
	{
#ifdef _WIN32
		FreeLibrary((HMODULE)m_handle);
#else
		dlclose(m_handle);
#endif
		m_handle = nullptr;
	}

	void *dynamic_library::get_impl(const std::string &name) const
	{
#ifdef _WIN32
		return (void*)GetProcAddress((HMODULE)m_handle, name.c_str());
#else
		return dlsym(m_handle, (char *)name.c_str());
#endif
	}

	bool dynamic_library::loaded() const
	{
		return !m_handle;
	}

	dynamic_library::operator bool() const
	{
		return loaded();
	}

	void* get_proc_address(const char* lib, const char* name)
	{
#ifdef _WIN32
		return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(lib), name));
#else
		return dlsym(dlopen(lib, RTLD_NOLOAD), name);
#endif
	}
}


DYNAMIC_IMPORT("ntdll.dll", NtCreateKeyedEvent, NTSTATUS(OUT PHANDLE KeyedEventHandle, IN ACCESS_MASK DesiredAccess, IN void *ObjectAttributes OPTIONAL, IN ULONG Reserved));

DYNAMIC_IMPORT("ntdll.dll", NtWaitForKeyedEvent, NTSTATUS(HANDLE Handle, PVOID Key, BOOLEAN Alertable, PLARGE_INTEGER Timeout));
DYNAMIC_IMPORT("ntdll.dll", NtReleaseKeyedEvent, NTSTATUS(HANDLE Handle, PVOID Key, BOOLEAN Alertable, PLARGE_INTEGER Timeout));

#define FAST_MUTEX_INITIALIZER {0}

HANDLE mhandle;

/* Allows up to 2^23-1 waiters */
#define FAST_M_WAKE		256
#define FAST_M_WAIT		512

void init_fast_mutex_handle(void)
{
	NtCreateKeyedEvent(&mhandle, -1, NULL, 0);
}

void fast_mutex_init(fast_mutex *m)
{
	m->waiters = 0;
}

void fast_mutex_lock(fast_mutex *m)
{
	/* Try to take lock if not owned */
	while (m->owned || _interlockedbittestandset(&m->waiters, 0))
	{
		unsigned long waiters = m->waiters | 1;

		/* Otherwise, say we are sleeping */
		if (_InterlockedCompareExchange(&m->waiters, waiters + FAST_M_WAIT, waiters) == waiters)
		{
			/* Sleep */
			NtWaitForKeyedEvent(mhandle, m, 0, NULL);

			/* No longer in the process of waking up */
			//_InterlockedAdd(&m->waiters, -FAST_M_WAKE);
			InterlockedExchangeAdd(&m->waiters, -FAST_M_WAKE);
		}
	}
}

int fast_mutex_trylock(fast_mutex *m)
{
	if (!m->owned && !_interlockedbittestandset(&m->waiters, 0)) return 0;

	return EBUSY;
}

void fast_mutex_unlock(fast_mutex *m)
{
	/* Atomically unlock without a bus-locked instruction */
	_ReadWriteBarrier();
	m->owned = 0;
	_ReadWriteBarrier();

	/* While we need to wake someone up */
	while (1)
	{
		unsigned long waiters = m->waiters;

		if (waiters < FAST_M_WAIT) return;

		/* Has someone else taken the lock? */
		if (waiters & 1) return;

		/* Someone else is waking up */
		if (waiters & FAST_M_WAKE) return;

		/* Try to decrease wake count */
		if (_InterlockedCompareExchange(&m->waiters, waiters - FAST_M_WAIT + FAST_M_WAKE, waiters) == waiters)
		{
			/* Wake one up */
			NtReleaseKeyedEvent(mhandle, m, 0, NULL);
			return;
		}

		YieldProcessor();
	}
}