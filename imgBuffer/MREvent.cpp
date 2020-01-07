#include "MREvent.h"

MREVent::MREVent(bool manualReset, const std::string& name)
	:mManualReset(manualReset)
{
	if (name == "")
	{
		mEvent = CreateEvent(NULL, manualReset ? TRUE : FALSE, FALSE, NULL);
	}
	else
	{
		mEvent = CreateEvent(NULL, manualReset ? TRUE : FALSE, FALSE, name.c_str());
	}
}

MREVent::~MREVent()
{
	if (mEvent != NULL)
	{
		CloseHandle(mEvent);
	}
}

MRStatus MREVent::Signal()
{
	// check for open
	if (mEvent == NULL)
	{
		return MR_STATUS_INITIALIZE;
	}

	// signal event
	return ::SetEvent(mEvent) ? MR_STATUS_SUCCESS : MR_STATUS_FAIL;
}

MRStatus MREVent::Clear()
{
	if (mEvent == NULL)
	{
		return MR_STATUS_INITIALIZE;
	}
	return ::ResetEvent(mEvent) ? MR_STATUS_SUCCESS : MR_STATUS_FAIL;
}


MRStatus MREVent::SetState(bool signaled)
{
	MRStatus status;

	if (signaled)
	{
		status = Signal();
	}
	else
	{
		status = Clear();
	}

	return status;
}


MRStatus MREVent::GetState(bool * pSignaled)
{
	if (pSignaled != NULL)
	{
		MRStatus status = WaitForSignal(0);//立刻获取信号
				// the event was signaled
		if (status == MR_STATUS_SUCCESS)
		{
			*pSignaled = true;
		}
		else if (status == MR_STATUS_TIMEOUT)
		{
			*pSignaled = false;
		}
		else
		{
			return status;
		}
	}
	return MR_STATUS_SUCCESS;
}


MRStatus MREVent::SetManualReset(bool manualReset)
{
	mManualReset = manualReset;
	return MR_STATUS_SUCCESS;
}


MRStatus MREVent::GetManualReset(bool * pManualReset)
{
	if (pManualReset)
	{
		*pManualReset = mManualReset;
		return MR_STATUS_SUCCESS;
	}
	else
		return MR_STATUS_FAIL;
}


MRStatus MREVent::WaitForSignal(uint32_t timeout)
{
	// check for open
	if (mEvent == NULL)
	{
		return MR_STATUS_INITIALIZE;
	}

	// check for infinite timeout
	if (timeout == 0xffffffff)
	{
		timeout = INFINITE;
	}

	// wait for the event to be signaled
	DWORD retCode = WaitForSingleObject(mEvent, (DWORD)timeout);

	// the event was signaled
	if (retCode == WAIT_OBJECT_0)
	{
		return MR_STATUS_SUCCESS;
	}
	// the wait timed out
	else if (retCode == WAIT_TIMEOUT)
	{
		return MR_STATUS_TIMEOUT;
	}

	return MR_STATUS_FAIL;
}


MRStatus MREVent::GetEventObject(uint64_t * pEventObject)
{
	if (pEventObject != NULL)
	{
		if (mEvent != INVALID_HANDLE_VALUE)
		{
			*pEventObject = (uint64_t)mEvent;
		}
		else
		{
			return MR_STATUS_OPEN;
		}
	}

	return MR_STATUS_SUCCESS;
}
