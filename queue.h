#ifndef _QUEUE_H_
#define _QUEUE_H_

#define MAX_NUM_QUEUE 60
#define MAX_NUM_AUDIO_QUEUE 64
#define QUE_INFINITE -1
#define MIN_BUF_FRAME 10

class CQueue
{
public:
	CQueue();
	~CQueue();

	void SetInfo(int nChannel, string type);
	void Clear();

	bool Put(AVPacket *pkt, int codec_type, int recv_type);
	int PutAudio(char *pData, int nSize);

	bool IsNextKeyFrame();
	int Get(AVPacket *pkt, int *codec_type, int *recv_type);
	void *GetAudio();

	void Ret(AVPacket *pkt);
	void RetAudio(void *p);

	int GetVideoBufferCount() { return m_nPacket; }

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

	int m_recv_type;
	int m_codec_type;

	pthread_mutex_t m_mutex;

	AVPacket *m_pkt[MAX_NUM_QUEUE];
	ELEM m_e[MAX_NUM_AUDIO_QUEUE];
};

#endif //_QUEUE_H_
