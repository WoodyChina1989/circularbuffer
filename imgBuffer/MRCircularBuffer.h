#pragma once
#include <mutex>
#include <vector>
#include <chrono>
#include "MREvent.h"
using namespace std;

#define TIMEOUT_MS	20
/**
	@brief	I am a circular frame buffer that simplifies implementing a type-safe producer/consumer
			model for processing frame-based streaming media. I can be used with any client-defined
			"frame", be it a struct or class. To use me:
				-#	Instantiate me.
				-#	Initialize me by calling my Add method, adding client-defined frames for me to manage.
				-#	Spawn a producer thread and a consumer thread.
				-#	The producer thread repeatedly calls my StartProduceNextBuffer, puts data in the frame,
					then calls EndProduceNextBuffer when finished.
				-#	The consumer thread repeatedly calls my StartConsumeNextBuffer, processes data in the frame,
					then calls EndConsumeNextBuffer when finished.
**/
typedef timed_mutex MRLocker;

template <typename FrameDataPtr>
class MRCircularBuffer
{
public:
	MRCircularBuffer();
	virtual ~MRCircularBuffer();


	/**
	@brief	Tells me the boolean variable I should monitor such that when it gets set to "true" will cause
			any threads waiting on my events/locks to gracefully exit.
	@param[in]	pAbortFlag	Specifies the valid, non-NULL address of a boolean variable that, when it becomes "true",
							will cause threads waiting on me to exit gracefully.
	**/
	inline void SetAbortFlag(const bool * pAbortFlag)	//为工厂模式两个线程退出准备的
	{
		mAbortFlag = pAbortFlag;
	}

	/**
	@return	The number of frames that I contain.
	*/
	inline unsigned int GetCircBufferCount(void) const
	{
		return mCircBufferCount;
	}

	/**
		@brief	Returns "true" if I'm empty -- i.e., if my tail and head are in the same place.
		@return	True if I contain no frames.
	**/
	inline bool IsEmpty(void) const
	{
		return GetCircBufferCount() == 0;
	}

	/**
		@brief	Returns my frame storage capacity, which reflects how many times my Add method has been called.
		@return	My frame capacity.
	**/
	inline unsigned int GetNumFrames(void) const
	{
		return (unsigned int)mFrames.size();
	}

	bool Add(FrameDataPtr pInFrameData);

	//生产线程推入新的数据，返回池子下一个可存的指针
	FrameDataPtr StartProduceNextBuffer(void);
	
	
	void EndProduceNextBuffer(void);//生产者线程存完数据后调用

	
	FrameDataPtr StartConsumeNextBuffer(void);//消费者线程调用
	

	void EndConsumeNextBuffer(void);// 消费者拿到数据后

	/**
		@brief	Clears my frame collection, their locks, everything.
		@note	This is not thread-safe. Thus, before calling this method, be sure all locks have been released and
				producer/consumer threads using me have terminated.
	**/
	void Clear(void);

private:
	typedef vector<MRLocker*> MRLockVector;
	
	vector<FrameDataPtr> mFrames;	//缓存帧
	MRLockVector	 mLocks;

	unsigned int				mHead;				///< @brief	My current "head" pointer, an index into my frame collection
	unsigned int				mTail;				///< @brief	My current "tail" pointer, an index into my frame collection
	unsigned int				mCircBufferCount;	///< @brief	My current size, the distance between my "head" and "tail"

	MREVent*					mNotFullEvent;		//when I transition from being full to not full
	MREVent*					mNotEmptyEvent;		//when I transition from empty to having at least one frame
	MRLocker					mDataBufferLock;	///< @brief	Protects my "head" and "tail" members

	unsigned int				mFillIndex;			///< @brief	Index where frames are added to me
	unsigned int				mEmptyIndex;		///< @brief	Index where frames are removed from me

	const bool *				mAbortFlag;			///< @brief	Optional pointer to a boolean that clients can set to break threads waiting on me

	/**
		@brief		Waits for the given event with a timeout, and abort the wait if mAbortFlag is set.
		@param[in]	ajaEvent	Specifies a valid, non-NULL AJAEvent object to trigger on.
		@return		True if the event triggers successfully; false otherwise.
	**/
	bool WaitForEventOrAbort(MREVent * mrEvent);


	/**
		@brief		Wait for mutex with a timeout and check to see if mAbortFlag is set.
		@param[in]	ajaEvent	Specifies a valid, non-NULL AJALock (mutex) object to wait on.
		@return		True if mutex is received; otherwise false.
	**/
	bool WaitForLockOrAbort(MRLocker* mrLock);
};



template<typename FrameDataPtr>
MRCircularBuffer<FrameDataPtr>::MRCircularBuffer()
	:mHead(0),
	mTail(0),
	mCircBufferCount(0),
	mFillIndex(0),
	mEmptyIndex(0),
	mAbortFlag(NULL)
{
	mNotEmptyEvent = new MREVent(false, "notEmpty");
	mNotFullEvent =new MREVent(true, "notFull");
}


template<typename FrameDataPtr>
MRCircularBuffer<FrameDataPtr>::~MRCircularBuffer()
{
	Clear();
}


template<typename FrameDataPtr>
inline bool MRCircularBuffer<FrameDataPtr>::Add(FrameDataPtr pInFrameData)
{
	mFrames.push_back(pInFrameData);
	MRLocker* lock = new MRLocker;
	mLocks.push_back(lock);
	return (mFrames.size() == mLocks.size() && lock) ? MR_STATUS_SUCCESS : MR_STATUS_FAIL;
}


template<typename FrameDataPtr>
inline FrameDataPtr MRCircularBuffer<FrameDataPtr>::StartProduceNextBuffer(void)
{
	while (1)
	{
		if (!WaitForLockOrAbort(&mDataBufferLock))	
		{
			cout << "### produce池子被锁住了" << endl;
			return NULL;
		}
		
		static  UINT32 countFull = 0;
		if (mCircBufferCount == mFrames.size())//池子满了
		{
			mDataBufferLock.unlock();//满的，就把池子锁交出去
			if (!WaitForEventOrAbort(mNotFullEvent))	//如果满了没有被取，就等待未满事件
			{
				countFull++;
				if (countFull > 9)
				{
					cout << "[Warining] process extract data too slown." << endl;
					countFull = 0;
				}
				return NULL;
			}
			continue;//因为满的时候把锁交出去了，重新获得锁
		}
		countFull = 0;
		break;
	}

	if (!WaitForLockOrAbort(mLocks[mHead])) 
	{
		mDataBufferLock.unlock();
		return NULL;
	}

	mFillIndex = mHead;
	mHead = (mHead + 1) % ((unsigned int)(mFrames.size()));
	mCircBufferCount++;
	if (mCircBufferCount == mFrames.size())
		mNotFullEvent->SetState(false);	//满了

	mDataBufferLock.unlock();
	return mFrames[mFillIndex];
}



template<typename FrameDataPtr>
inline void MRCircularBuffer<FrameDataPtr>::EndProduceNextBuffer(void)
{
	mLocks[mFillIndex]->unlock();
	mNotEmptyEvent->SetState(true);
}


template<typename FrameDataPtr>
inline FrameDataPtr MRCircularBuffer<FrameDataPtr>::StartConsumeNextBuffer(void)
{
	while (1)
	{
		if (!WaitForLockOrAbort(&mDataBufferLock))	//拿到取数据的锁，锁上	
			return NULL;
		
		static UINT32 countEmpty = 0;
		if (mCircBufferCount == 0)	
		{
			mDataBufferLock.unlock();		//池子是空,交出锁等待,直到不空事件
			if (!WaitForEventOrAbort(mNotEmptyEvent))
			{
				countEmpty++;
				if (countEmpty > 9)	//多次超时等都没有数据就报警
				{
					cout << "[Error] 信号源帧率太低" << endl;
					countEmpty = 0;
				}
				return NULL;	//超时没拿到就返回空指针
			}		
			continue;	//空数据导致交出了锁，重新拿到锁
		}
		countEmpty = 0;
		break;
	}

	if (!WaitForLockOrAbort(mLocks[mTail]))
	{
		mDataBufferLock.unlock();
		return NULL;
	}
	
	mEmptyIndex = mTail;
	mTail = (mTail + 1) % ((unsigned int)mFrames.size());
	mCircBufferCount--;
	if (mCircBufferCount == 0)
		mNotEmptyEvent->SetState(false);//就是empty = !NotEmpty
	mDataBufferLock.unlock();

	return mFrames[mEmptyIndex];
}


template<typename FrameDataPtr>
inline void MRCircularBuffer<FrameDataPtr>::EndConsumeNextBuffer(void)
{
	mLocks[mEmptyIndex]->unlock();
	mNotFullEvent->SetState(true);
}


template<typename FrameDataPtr>
inline void MRCircularBuffer<FrameDataPtr>::Clear(void)
{
	for (MRLockVector::iterator iter(mLocks.begin()); iter != mLocks.end(); ++iter)
		delete *iter;

	mLocks.clear();
	mFrames.clear();

	mHead = mTail = mFillIndex = mEmptyIndex = mCircBufferCount = 0;
	mAbortFlag = NULL;
}


template<typename FrameDataPtr>
inline bool MRCircularBuffer<FrameDataPtr>::WaitForEventOrAbort(MREVent * mrEvent)
{
	const unsigned int timeout = TIMEOUT_MS;
/*
	do {
		MRStatus status = mrEvent->WaitForSignal(timeout);
		if (status == MR_STATUS_TIMEOUT)
			if (mAbortFlag)
				if (*mAbortFlag)
					return false;
		if (status == MR_STATUS_FAIL)
			return false;
		if (status == MR_STATUS_SUCCESS)
			break;
	} while (1);
*/
	MRStatus status = mrEvent->WaitForSignal(timeout);

	if (status == MR_STATUS_SUCCESS)  return true;
	
	return false;
}


template<typename FrameDataPtr>
inline bool MRCircularBuffer<FrameDataPtr>::WaitForLockOrAbort(MRLocker* mrLocker)
{
	const unsigned int timeout = TIMEOUT_MS;
	/*根本没人启动Abort
	do {
		bool successLocked = mrLocker->try_lock_for(chrono::milliseconds(timeout));
		if (successLocked == false)
			if (mAbortFlag)
				if (*mAbortFlag)
					return false;
		if (successLocked)
			break;
	} while (1);
	*/

	bool successLocked = mrLocker->try_lock_for(chrono::milliseconds(timeout));
	if (successLocked == false) 
		return false;

	return true;
}
