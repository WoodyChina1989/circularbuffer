#pragma once
#include <Windows.h>

#include <string>

#define MAX_EVENTS 64

enum MRStatus
{
	MR_STATUS_SUCCESS = 0,
	MR_STATUS_FAIL = -1,
	MR_STATUS_TIMEOUT = -3,
	MR_STATUS_INITIALIZE = -5,
	MR_STATUS_OPEN = -7
};


using std::string;


class MREVent
{
public:
	MREVent() = default;
	MREVent(bool manualReset, const std::string& name);
	~MREVent();

	MRStatus Signal();
	MRStatus Clear();

	MRStatus SetState(bool signaled = true);
	MRStatus GetState(bool* pSignaled);

	MRStatus SetManualReset(bool manualReset);
	MRStatus GetManualReset(bool* pManualReset);

	MRStatus WaitForSignal(uint32_t timeout = 0xffffffff);

	MRStatus GetEventObject(uint64_t* pEventObject);

	HANDLE mEvent;

private:
	bool   mManualReset;
};


