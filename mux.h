#ifndef _MUX_H_
#define _MUX_H_

#include "main.h"
#include "tsmuxer.h"
#include "queue.h"

class CMux : public PThread
{
public:
	CMux(void);
	~CMux(void);

	//int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool SetSocket();
	bool TX(char *buff, int size);
	bool Create(Json::Value info, Json::Value attr, int nChannel);
	void Delete();
	bool SetQueue(CQueue **queue, int nChannel);

	bool Muxing();

	void log(int type, int state);

protected:
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute

	char m_strShmPrefix[32];

	pthread_mutex_t m_mutex_mux;

private:
	//mux_cfg_s m_mux_cfg;

	int m_nRecSec;	 // 얼마나 녹화를 할 것인가
	int m_nFrameCount; // 프레임 수
	int m_nAudioCount; // 오디오 수
	int m_file_idx;	// 파일 인덱스 번호
	string m_filename;
	string m_es_name;
	string m_audio_name;
	//int m_sock;
	int m_sdSend;

	Json::Value json;

	string m_type;

	CTSMuxer *m_pMuxer;
	CQueue *m_queue;

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _MUX
