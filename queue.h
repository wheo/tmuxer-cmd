#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 120
#define MAX_NUM_AUDIO_QUEUE 128
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 30

class CQueue
{
public:
	CQueue();
	~CQueue();

	void SetInfo(int nChannel, string type);
	void Clear();

	bool Put(AVPacket *pkt);
	int PutAudio(char *pData, int nSize);

	bool IsNextKeyFrame();
	int Get(AVPacket *pkt);
	void *GetAudio();

	void Ret(AVPacket *pkt);
	void RetAudio(void *p);

	void Enable();
	void EnableAudio();
	bool IsEnable() { return m_bEnable; };
	bool IsAudioEnable() { return m_bAudioEnable; };

private:
	bool m_bEnable;
	bool m_bAudioEnable;

	int m_nPacket;
	int m_nAudio;

	int m_nMaxQueue;
	int m_nMaxAudioQueue;

	int m_nSizeQueue;

	int m_nReadPos;
	int m_nWritePos;

	int m_nReadAudioPos;
	int m_nWriteAudioPos;

	int m_nChannel;
	string m_type;

	pthread_mutex_t m_mutex;

	AVPacket *m_pkt[MAX_NUM_QUEUE];
	ELEM m_e[MAX_NUM_AUDIO_QUEUE];
};

#endif //_QUEUE_H_
