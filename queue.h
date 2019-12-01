#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 512
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 9

class CQueue
{
public:
	CQueue();
	~CQueue();

	void Create(int nNum);
	void Clear();

	int Put(AVPacket *pkt);

	int Get(AVPacket *pkt);
	void Ret(AVPacket *pkt);
	void Enable();
	bool IsEnable() { return m_bEnable; };

private:
	bool m_bEnable;

	int m_nPacket;

	int m_nMaxQueue;
	int m_nSizeQueue;

	int m_nReadPos;
	int m_nWritePos;

	int m_nReadFramePos;
	int m_nWriteFramePos;

	char *m_pBuffer;

	int m_nChannel;

	pthread_mutex_t m_mutex;

	AVPacket m_pkt[MAX_NUM_QUEUE];
};

#endif //_QUEUE_H_
