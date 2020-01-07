
#include <opencv2/opencv.hpp>
#include <thread>
#include "MRCircularBuffer.h"

using namespace cv;
using namespace std;

#define bufSize 5
struct AVDataBuffer
{
	uint64 timeCode;
	Mat*	img;
	uint64 frameIndex = 0;
};
AVDataBuffer hostBuffer[bufSize];

thread* thread_consumer;
void consumFrame();
MRCircularBuffer<AVDataBuffer*> mAVPool;

bool flagStop = false;

bool readPic = false;

bool holdProduce = false;
int main()
{
	for (size_t i = 0; i < bufSize; i++)
	{
		hostBuffer[i].timeCode = 0;
		hostBuffer[i].img = new Mat;
		mAVPool.Add(hostBuffer);
	}

	thread_consumer = new thread(consumFrame);
	Mat img = imread("monky.jpg");
	Mat myImg(600, 400, CV_8UC3, Scalar(200, 100, 20));
	int frameIndex = 0;
	while (true)
	{
		if (holdProduce)
		{
			this_thread::sleep_for(chrono::milliseconds(90));
		}

		Mat newImg;
		img.copyTo(newImg);
		cv::putText(newImg, to_string(frameIndex), Point(60, 80), FONT_HERSHEY_COMPLEX, 2, cv::Scalar(0, 255, 255));
		AVDataBuffer *	captureData(mAVPool.StartProduceNextBuffer()); //获取池子下一个缓存的指针
		if (captureData)
		{
			newImg.copyTo(*captureData->img);
			captureData->timeCode = chrono::high_resolution_clock::now().time_since_epoch().count();
			captureData->frameIndex = frameIndex;
			mAVPool.EndProduceNextBuffer();
		}

		imshow("main", myImg);
		int key = waitKey(2);
		if (key == 'r')
		{
			readPic = !readPic;
		}
		this_thread::sleep_for(chrono::milliseconds(11));
		frameIndex++;
	}

	flagStop = true;
	thread_consumer->join();
	return 0;
}


void consumFrame()
{
	int frames = 0;
	while (!flagStop)
	{
		if (readPic)
		{
			AVDataBuffer *	pFrameData(mAVPool.StartConsumeNextBuffer());
			if (pFrameData)
			{
				Mat img;
				pFrameData->img->copyTo(img);
				auto tc = pFrameData->timeCode;

				imshow("consumImg", img);
				int key = waitKey(2);
				if (key == 'h')
				{
					cout << "------------- hold pressed" << endl;
					holdProduce = !holdProduce;
				}
				cout << "读取: " << pFrameData->frameIndex << endl;
			}
			mAVPool.EndConsumeNextBuffer();
		}
		this_thread::sleep_for(chrono::milliseconds(5));
	}
}
