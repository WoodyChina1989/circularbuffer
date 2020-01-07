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
	inline void SetAbortFlag(const bool * pAbortFlag)	//Ϊ����ģʽ�����߳��˳�׼����
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

	//�����߳������µ����ݣ����س�����һ���ɴ��ָ��
	FrameDataPtr StartProduceNextBuffer(void);
	
	
	void EndProduceNextBuffer(void);//�������̴߳������ݺ����

	
	FrameDataPtr StartConsumeNextBuffer(void);//�������̵߳���
	

	void EndConsumeNextBuffer(void);// �������õ����ݺ�

	/**
		@brief	Clears my frame collection, their locks, everything.
		@note	This is not thread-safe. Thus, before calling this method, be sure all locks have been released and
				producer/consumer threads using me have terminated.
	**/
	void Clear(void);

private:
	typedef vector<MRLocker*> MRLockVector;
	
	vector<FrameDataPtr> mFrames;	//����֡
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
			cout << "### produce���ӱ���ס��" << endl;
			return NULL;
		}
		
		static  UINT32 countFull = 0;
		if (mCircBufferCount == mFrames.size())//��������
		{
			mDataBufferLock.unlock();//���ģ��Ͱѳ���������ȥ
			if (!WaitForEventOrAbort(mNotFullEvent))	//�������û�б�ȡ���͵ȴ�δ���¼�
			{
				countFull++;
				if (countFull > 9)
				{
					cout << "[Warining] process extract data too slown." << endl;
					countFull = 0;
				}
				return NULL;
			}
			continue;//��Ϊ����ʱ���������ȥ�ˣ����»����
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
		mNotFullEvent->SetState(false);	//����

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
		if (!WaitForLockOrAbort(&mDataBufferLock))	//�õ�ȡ���ݵ���������	
			return NULL;
		
		static UINT32 countEmpty = 0;
		if (mCircBufferCount == 0)	
		{
			mDataBufferLock.unlock();		//�����ǿ�,�������ȴ�,ֱ�������¼�
			if (!WaitForEventOrAbort(mNotEmptyEvent))
			{
				countEmpty++;
				if (countEmpty > 9)	//��γ�ʱ�ȶ�û�����ݾͱ���
				{
					cout << "[Error] �ź�Դ֡��̫��" << endl;
					countEmpty = 0;
				}
				return NULL;	//��ʱû�õ��ͷ��ؿ�ָ��
			}		
			continue;	//�����ݵ��½��������������õ���
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
		mNotEmptyEvent->SetState(false);//����empty = !NotEmpty
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
	/*����û������Abort
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
