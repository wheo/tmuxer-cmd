#include "main.h"
#include "recv.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4

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
	delete[] m_frame_buf;

	pthread_mutex_destroy(&m_mutex_recv);
}

bool CRecv::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;
	m_Is_iframe = false;
	m_frame_buf = new unsigned char[MAX_frame_size];
	m_pts = 0;
	//_d("[RECV.ch%d] alloc : %x\n", m_nChannel, m_frame_buf);

	if (!SetSocket())
	{
		cout << "[RECV.ch" << m_nChannel << "] SetSocket() is failed" << endl;
		return false;
	}
	else
	{
		cout << "[RECV.ch" << m_nChannel << "] SetSocket() is succeed" << endl;
	}

	int bit_state = m_attr["bit_state"].asInt();
	int result = bit_state & (1 << m_nChannel);

	cout << "[MUX.ch." << m_nChannel << "] bit_state : " << bit_state << ", result : " << result << endl;

	string file_dst = m_attr["file_dst"].asString();
	string sub_dir_name = m_attr["folder_name"].asString();
	string group_name;
	string type = m_info["type"].asString();

	stringstream sstm;
	sstm << "mkdir -p " << file_dst << "/" << sub_dir_name << "/" << m_nChannel;
	system(sstm.str().c_str());
	// clear method is not working then .str("") correct
	sstm.str("");

	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/"
		 << "meta.json";
	group_name = sstm.str();
	sstm.str("");

	Json::Value meta;

	if (bit_state > 0)
	{
		cout << "channel : " << m_nChannel << ", bit state is " << bit_state << ", result : " << result << endl;
		if (result < 1)
		{
			cout << "channel " << m_nChannel << " is not use anymore" << endl;
			meta["filename"] = "";
			meta["channel"] = m_nChannel;
			meta["type"] = m_type;
			meta["num"] = m_info["num"].asInt();
			meta["den"] = m_info["den"].asInt();
			CreateMetaJson(meta, group_name);
			return false;
		}
	}

	m_queue = new CQueue();
	m_queue->SetInfo(m_nChannel, type);

	Start();
	//m_queue->Enable();
	m_mux = new CMux();
	m_mux->SetQueue(&m_queue, m_nChannel);
	m_mux->Create(info, attr, m_nChannel);

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
	string ip = m_info["ip"].asString();
	int port = m_info["port"].asInt();

	memset(&mcast_group, 0x00, sizeof(mcast_group));
	mcast_group.sin_family = AF_INET;
	mcast_group.sin_port = htons(port);
	mcast_group.sin_addr.s_addr = inet_addr(ip.c_str());
	//mcast_group.sin_addr.s_addr = inet_addr(INADDR_ANY);

	m_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == m_sock)
	{
		_d("[RECV.ch.%d] socket creation error\n", m_nChannel);
		return false;
	}
	else
	{
		_d("[RECV.ch.%d] socket creation (%d)\n", m_nChannel, m_sock);
	}

	uint reuse = 1;
	state = setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (state < 0)
	{
		perror("[RECV] Setting SO_REUSEADDR error\n");
		return false;
	}

	if (-1 == bind(m_sock, (struct sockaddr *)&mcast_group, sizeof(mcast_group)))
	{
		perror("[RECV] bind error");
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
	unsigned char buff_rcv[PACKET_HEADER_SIZE + PACKET_SIZE];

	int rd;
	int fd = 0;
	int recv_nTotalStreamSize = 0;
	int recv_nCurStreamSize = 0;
	int recv_nestedStreamSize = 0;
	char recv_type = 0;		  // 1(NTSC), 2(PAL), 3(PANORAMA), 4(FOCUS)
	char recv_codec_type = 0; // 1(HEVC), 2(H264)
	char recv_frame_type = 0; // 0(P frame), 1(I frame)
	char recv_reserve[5] = {
		0,
	};

	int nTotalPacketNum = 0;
	int nCurPacketNum = 0;
	int nPrevPacketNum = 0;
#if 0
	stringstream sstm;
	sstm << "test_" << m_nChannel << ".hevc";

	string dumpname = sstm.str();

	FILE *es = fopen(dumpname.c_str(), "wb");
#endif
#if 0
	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));
#endif
	//memset(m_frame_buf, 0x00, sizeof(m_frame_buf));

	cout << "[RECV.ch" << m_nChannel << "] type : " << m_info["type"].asString() << endl;
	m_queue->Enable();

	high_resolution_clock::time_point begin;
	high_resolution_clock::time_point end;
	int64_t tick_diff = 0;

	begin = high_resolution_clock::now();
	while (!m_bExit)
	{
		rd = recvfrom(m_sock, buff_rcv, PACKET_HEADER_SIZE + PACKET_SIZE, 0, NULL, 0);
#if __DEBUG
		_d("[RECV.ch%d] rd : %d\n", m_nChannel, rd);
#endif
		if (rd < 1)
		{
			usleep(10);
			continue;
		}
#if 0
		// 최초 recv 받고 나서 enable
		if (m_info["type"].asString() == "audio" || m_info["type"].asString() == "video")
		{
			m_queue->Enable();
		}
#endif
		if (m_info["type"].asString() == "audio")
		{
			void *p;
			p = buff_rcv;
			m_queue->PutAudio((char *)p, rd);
		}
		else if (m_info["type"].asString() == "video")
		{
			memcpy(&recv_nTotalStreamSize, buff_rcv, sizeof(recv_nTotalStreamSize));
			memcpy(&recv_nCurStreamSize, buff_rcv + 4, sizeof(recv_nCurStreamSize));
			memcpy(&nTotalPacketNum, buff_rcv + 8, sizeof(nTotalPacketNum));
			memcpy(&nCurPacketNum, buff_rcv + 12, sizeof(nCurPacketNum));
			memcpy(&recv_type, buff_rcv + 16, 1);
			memcpy(&recv_codec_type, buff_rcv + 17, 1);
			memcpy(&recv_frame_type, buff_rcv + 18, 1);
			memcpy(&recv_reserve, buff_rcv + 19, sizeof(recv_reserve));
#if 0
			for (int i = 0; i < PACKET_HEADER_SIZE; i++)
			{
				_d("offset : %d, mem[%d] : %x\n", i, i, buff_rcv[i]);
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
			//_d("add : %x, size : %d\n", m_frame_buf, MAX_frame_size);
			memcpy(m_frame_buf + recv_nestedStreamSize, buff_rcv + PACKET_HEADER_SIZE, recv_nCurStreamSize);
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
#if 0
				fwrite(m_frame_buf, 1, recv_nestedStreamSize, es);
#endif
				AVPacket *pPkt;
				//AVPacket pPkt;
				pPkt = av_packet_alloc();
				av_init_packet(pPkt);

				m_pts += 1001;

				pPkt->data = m_frame_buf;
				pPkt->size = recv_nestedStreamSize;
				pPkt->pts = m_pts;

				if (recv_frame_type == 1)
				{
					pPkt->flags = AV_PKT_FLAG_KEY;
				}

				end = high_resolution_clock::now();
				tick_diff = duration_cast<microseconds>(end - begin).count();
#if __DEBUG
				_d("[RECV.ch%d][%lld] Putted !!!!! frame buf(%x), size : %d, type : %d, pkt.flags : %d\n", m_nChannel, tick_diff, pPkt->data, pPkt->size, recv_frame_type, pPkt->flags);
#endif
				m_queue->Put(pPkt);

				begin = end;
				tick_diff = 0;

				recv_nestedStreamSize = 0;
				nPrevPacketNum = 0;
			}
		}
		//_d("write completed : %d(%d)\n", rd, fd);
		//write(fd, buff_rcv, 16);
	}
#if 0
	fclose(es);
#endif
	//cout << "[RECV] thread loop out" << endl;
	return true;
}

void CRecv::Delete()
{
	close(m_sock); // release socket
	cout << "[RECV.ch" << m_nChannel << "] sock " << m_sock << " is closed" << endl;
	SAFE_DELETE(m_mux);
	m_mux = NULL;
}