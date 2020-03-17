#ifndef _RECV_H_
#define _RECV_H_

#include "main.h"
#include "tsmuxer.h"
#include "videoglobal.h"
#include "queue.h"
#include "mux.h"

class CRecv : public PThread
{
public:
	CRecv(void);
	~CRecv(void);

	bool send_bitstream(uint8_t *stream, int size);

	int ReadSocket(uint8_t *buffer, unsigned bufferSize);
	bool SetSocket();
	bool Create(int nType, Json::Value info, Json::Value attr, int nChannel);
	void Delete();

	bool Receive();
	string GetChannelType(int nChannel);

protected:
	int m_nProgramType; // 0 : 상시녹화, 1 : 이벤트녹화
	int m_nChannel;		// 현재 채널 넘버
	Json::Value m_info; // 채널 정보 json
	Json::Value m_attr; // 채널 공유 속성 attribute

	char m_strShmPrefix[32];
	int m_nRead;
	int m_nWrite;

	string m_type;

	pthread_mutex_t m_mutex_recv;

private:
	//mux_cfg_s m_mux_cfg;

	int m_nRecSec;	 // 얼마나 녹화를 할 것인가
	int m_nFrameCount; // 프레임 수
	int m_file_idx;	// 파일 인덱스 번호
	int64_t m_pts;	 // 패킷 수신받은 시간
	string m_filename;
	int m_sock;
	bool m_Is_iframe;
	unsigned char *m_frame_buf;
	int64_t tick_diff;
	int64_t old_tick_diff;

	high_resolution_clock::time_point m_begin;
	high_resolution_clock::time_point m_end;

	Json::Value json;

	CTSMuxer *m_pMuxer;
	CQueue *m_queue;
	CMux *m_mux;

protected:
	void Run();
	void OnTerminate(){};
};

extern CRecv *ipc;

#endif // _RECV
