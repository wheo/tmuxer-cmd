#include "main.h"
#include "recv.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CRecv::CRecv(void)
{
	m_bExit = false;
	pthread_mutex_init(&m_mutex_recv, 0);
}

CRecv::~CRecv(void)
{
	m_bExit = true;

	_d("[RECV.ch%d] Trying to exit thread\n", m_nChannel);
	Delete();
	Terminate();
	_d("[RECV.ch%d] exited...\n", m_nChannel);

	pthread_mutex_destroy(&m_mutex_recv);
}

void CRecv::log(int type, int state)
{
}

bool CRecv::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;

	if (!SetSocket())
	{
		cout << "[RECV] SetSocket() is failed" << endl;
		return false;
	}
	else
	{
		cout << "[RECV] SetSocket() is succeed" << endl;
	}

	m_queue = new CQueue();
	m_queue->SetInfo(m_nChannel, m_info["type"].asString());
	//m_queue->Enable();
	m_mux = new CMux();
	m_mux->SetQueue(&m_queue, m_nChannel);
	m_mux->Create(info, attr, m_nChannel);

	Start();
	return true;
}

void CRecv::Run()
{
	while (!m_bExit)
	{
		if (!Receive())
		{
			//error
		}
		else
		{
			// success
		}
	}
	//_d("[RECV.ch%d] thread exit\n", m_nChannel);
}

bool CRecv::SetSocket()
{
	struct sockaddr_in mcast_group;
	struct ip_mreq mreq;

	int state;

	memset(&mcast_group, 0x00, sizeof(mcast_group));
	mcast_group.sin_family = AF_INET;
	mcast_group.sin_port = htons(m_info["port"].asInt());
	mcast_group.sin_addr.s_addr = inet_addr(m_info["ip"].asString().c_str());

	m_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == m_sock)
	{
		_d("[RECV.ch.%d] socket creation error\n", m_nChannel);
		return false;
	}

	if (-1 == bind(m_sock, (struct sockaddr *)&mcast_group, sizeof(mcast_group)))
	{
		perror("[RECV] bind error");
		return false;
	}

	uint reuse = 1;
	state = setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (state < 0)
	{
		perror("[RECV] Setting SO_REUSEADDR error\n");
		return false;
	}

	mreq.imr_multiaddr = mcast_group.sin_addr;
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if (setsockopt(m_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
	{
		perror("[RECV] add membership setsocket opt");
		return false;
	}

	struct timeval read_timeout;
	read_timeout.tv_sec = 1;
	read_timeout.tv_usec = 0;
	if (setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout)) < 0)
	{
		perror("[RECV] set timeout error");
		return false;
	}
	return true;
}

bool CRecv::Receive()
{
	unsigned char buff_rcv[PACKET_SIZE + 16];
	unsigned char frame_buf[MAX_frame_size];
	int rd;
	int fd = 0;
	int recv_nTotalStreamSize = 0;
	int recv_nCurStreamSize = 0;
	int recv_nestedStreamSize = 0;

	int nTotalPacketNum = 0;
	int nCurPacketNum = 0;
	int nPrevPacketNum = 0;

#if 0
	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));
#endif
	memset(&frame_buf, 0x00, sizeof(&frame_buf));

	cout << "[RECV.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;
	if (m_info["type"].asString() == "audio" || m_info["type"].asString() == "video")
	{
		m_queue->Enable();
	}
	while (!m_bExit)
	{
		/* code */
		rd = recvfrom(m_sock, buff_rcv, PACKET_SIZE + 16, 0, NULL, 0);
#if __DEBUG
		_d("[RECV.ch%d] rd : %d\n", m_nChannel, rd);
#endif
		if (rd < 1)
		{
			usleep(10);
			continue;
		}
		if (m_info["type"].asString() == "audio")
		{
			void *p;
			p = buff_rcv;
			m_queue->PutAudio((char *)p, rd);
		}
		else
		{
#if 0
			if (m_info["type"].asString() == "video")
			{
				continue;
			}
#endif
			memcpy(&recv_nTotalStreamSize, buff_rcv, 4);
			memcpy(&recv_nCurStreamSize, buff_rcv + 4, 4);
			memcpy(&nTotalPacketNum, buff_rcv + 8, 4);
			memcpy(&nCurPacketNum, buff_rcv + 12, 4);
#if 0
		for (int i=0;i<16;i++) {
			_d("offset : %d, mem[%d] : %x\n",i, i, buff_rcv[i] );
		}
#endif
			if ((nPrevPacketNum + 1) != nCurPacketNum)
			{
#if __DEBUG
				_d("\n[RECV.ch%d] TotalPacketNum(%d)PrevPacketNum(%d)/nCurPacketNum(%d)\n\n", m_nChannel, nTotalPacketNum, nPrevPacketNum, nCurPacketNum);
#endif
			}
			nPrevPacketNum = nCurPacketNum;
#if __DEBUG
			//_d("nTotalPacketNum(%d)/nCurPacketNum(%d), recv_nestedStreamSize(%d)\n", nTotalPacketNum, nCurPacketNum, recv_nestedStreamSize);
			cout << "[RECV.ch." << m_nChannel << "] nTotalPacketNum(" << nTotalPacketNum << "/" << nCurPacketNum << ", " << recv_nestedStreamSize << ")" << endl;
#endif
			memcpy(frame_buf + recv_nestedStreamSize, buff_rcv + 16, recv_nCurStreamSize);
			recv_nestedStreamSize += recv_nCurStreamSize;
#if __DEBUG
			_d("[RECV.ch%d] (%d/%d)\n", m_nChannel, recv_nCurStreamSize, recv_nestedStreamSize);
#endif
			if (nTotalPacketNum == nCurPacketNum)
			{
				// 1 frame 만들어 졌을 때
#if __DEBUG
				_d("[RECV.ch%d] recv_nTotalStreamSize(%d)/recv_nestedStreamSize(%d)\n", m_nChannel, recv_nTotalStreamSize, recv_nestedStreamSize);
				_d("[RECV.ch%d] 1 (%d) frame created\n", m_nChannel, recv_nestedStreamSize);
#endif
				AVPacket *pPkt;
				pPkt = av_packet_alloc();

				pPkt->data = frame_buf;
				pPkt->size = recv_nestedStreamSize;
				//pPkt->pts = 1000;
				m_queue->Put(pPkt);
				av_packet_free(&pPkt);

				recv_nestedStreamSize = 0;
				nPrevPacketNum = 0;
			}
		}
		//_d("write completed : %d(%d)\n", rd, fd);
		//write(fd, buff_rcv, 16);
	}
	//cout << "[RECV] thread loop out" << endl;
	return true;
}

void CRecv::Delete()
{
	close(m_sock);
	SAFE_DELETE(m_mux);
#if __DEBUG
	cout << "sock " << m_sock << " closed" << endl;
#endif
}
