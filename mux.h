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
	bool TX(char *buff, int size);
	bool Create(int nProgramType, Json::Value info, Json::Value attr, int nChannel);
	void Delete();
	bool SetQueue(CQueue **queue, int nChannel);

	bool Muxing();
	void SetConfig(mux_cfg_s *mux_cfg, int codec_type, int recv_type);

protected:
	int m_nProgramType; // 0 상시녹화, 1 이벤트녹화
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute

	char m_strShmPrefix[32];

	pthread_mutex_t m_mutex_mux;

private:
	//mux_cfg_s m_mux_cfg;

	int m_nRecSec;			// 얼마나 녹화를 할 것인가
	uint64_t m_nFrameCount; // 한 파일의 프레임
	uint64_t m_nAudioCount; // 한 파일의 오디오

	uint64_t m_nTotalFrameCount; // 전체 프레임
	uint64_t m_nTotalAudioCount; // 전체 오디오
	int64_t m_current_pts;		 //현재 패킷의 pts
	int64_t m_audio_pts;		 //현재 오디오 pts
	int m_file_cnt;				 //파일의 개수

	int m_file_idx; // 파일 인덱스 번호
	int m_is_intra;
	string m_filename;
	string m_es_name;
	string m_bin_name;
	string m_audio_name;
	//int m_sock;
	int m_sdSend;

	Json::Value json;

	string m_type;

	CTSMuxer *m_pMuxer;
	CQueue *m_queue;
	bool is_old_intra;

	bool SetSocket();

protected:
	void Run();
	void OnTerminate(){};
};

#endif // _MUX