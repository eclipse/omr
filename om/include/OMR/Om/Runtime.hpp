#if !defined(OMR_OM_RUNTIME_HPP_)
#define OMR_OM_RUNTIME_HPP_

#include "OMR_VMThread.hpp"
#include "StartupManagerImpl.hpp"
#include "mminitcore.h"
#include "omr.h"
#include "omrgcstartup.hpp"
#include "omrport.h"
#include "omrprofiler.h"
#include "omrrasinit.h"
#include "omrutil.h"
#include "omrvm.h"
#include "thread_api.h"
#include <StartupManagerImpl.hpp>
#include <omr.h>
#include <omrgcstartup.hpp>
#include <omrport.h>
#include <thread_api.h>

#include <stdexcept>
#include <stdlib.h>
#include <string.h>

namespace OMR
{
namespace Om
{
class PlatformError : public std::exception
{
  public:
	PlatformError(int error) : error_(error) {}

  private:
	int error_;
};

/// OMR Thread library wrapper
class ThreadInterface
{
  public:
	static void init()
	{
		auto e = omrthread_init_library();
		if (e != 0)
		{
			throw PlatformError(e);
		}
	}

	static void tearDown() noexcept { omrthread_shutdown_library(); }

	ThreadInterface() { init(); }

	~ThreadInterface() noexcept { tearDown(); }

  private:
};

/// omrthread wrapper.
class Thread
{
  public:
	Thread(const ThreadInterface &ithread)
	{
		auto e = omrthread_attach_ex(&self_, J9THREAD_ATTR_DEFAULT);
		if (e != 0)
		{
			throw PlatformError(e);
		}
	}

	~Thread() noexcept { omrthread_detach(self_); }

	omrthread_t data() { return self_; }

	const omrthread_t data() const { return self_; }

  private:
	omrthread_t self_;
};

/// OMR Thread and Port Library Wrapper
class PlatformInterface
{
  public:
	PlatformInterface()
	{
		Thread self(threadInterface_);

		auto e = omrport_init_library(&library(), sizeof(OMRPortLibrary));
		if (e != 0)
		{
			throw PlatformError(e);
		}
	}

	~PlatformInterface() noexcept
	{
		Thread self(threadInterface_);
		library().port_shutdown_library(&library());
	}

	OMRPortLibrary &library() noexcept { return portLibrary_; }

	const OMRPortLibrary &library() const noexcept { return portLibrary_; }

	const ThreadInterface &thread() const noexcept { return threadInterface_; }

  private:
	ThreadInterface threadInterface_;
	OMRPortLibrary portLibrary_;
};

/// Process-wide initialization and tear down.
class ProcessRuntime
{
  public:
	/// Initialize the process runtime.
	ProcessRuntime()
	{
		memset(&omrRuntime_, 0, sizeof(OMR_Runtime));
		omrRuntime_._configuration._maximum_vm_count = 0;
		omrRuntime_._vmCount = 0;
		omrRuntime_._portLibrary = &platform().library();

		auto e = omr_initialize_runtime(&omrRuntime_);
		if (e != 0)
		{
			throw PlatformError(e);
		}
	}

	~ProcessRuntime() noexcept { omr_destroy_runtime(&omrRuntime_); }

	PlatformInterface &platform() { return platform_; }

	const PlatformInterface &platform() const { return platform_; }

	OMR_Runtime &omrRuntime() { return omrRuntime_; }

  private:
	PlatformInterface platform_;
	OMR_Runtime omrRuntime_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_RUNTIME_HPP_
