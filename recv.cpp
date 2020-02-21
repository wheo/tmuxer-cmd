#include "main.h"
#include "recv.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4

CRecv::CRecv(void)
{
	m_bExit = false;
	m_mux = NULL;
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
	m_pts = -1;

	//_d("[RECV.ch%d] alloc : %x\n", m_nChannel, m_frame_buf);

	int bit_state = m_attr["bit_state"].asInt();
	int result = bit_state & (1 << m_nChannel);

	cout << "[MUX.ch." << m_nChannel << "] bit_state : " << bit_state << ", result : " << result << endl;

	string file_dst = m_attr["file_dst"].asString();
	string sub_dir_name = m_attr["folder_name"].asString();
	string group_name;

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
			CreateMetaJson(meta, group_name);
			return false;
		}
	}

	if (!SetSocket())
	{
		cout << "[RECV.ch" << m_nChannel << "] SetSocket() is failed" << endl;
		return false;
	}
	else
	{
		cout << "[RECV.ch" << m_nChannel << "] SetSocket() is succeed" << endl;
	}

	m_type = GetChannelType(m_nChannel);

	info["type"] = m_type;
	m_queue = new CQueue();
	m_queue->SetInfo(m_nChannel, m_type);

	Start();
	//m_queue->Enable();
	m_mux = new CMux();
	m_mux->SetQueue(&m_queue, m_nChannel);
	m_mux->Create(info, attr, m_nChannel);

	return true;
}

void CRecv::Run()
{
	Receive();
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

string CRecv::GetChannelType(int nChannel)
{
	string ret_type = "";
	if (nChannel < 6)
	{
		ret_type = "video";
	}
	else if (nChannel >= 6)
	{
		ret_type = "audio";
	}
	return ret_type;
}

bool CRecv::Receive()
{
	unsigned char buff_rcv[PACKET_HEADER_EXTEND_SIZE + PACKET_SIZE];

	bool is_first_time_recv = false;
	int64_t first_time_pts = -1;

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

	int64_t recv_pts = -1;

	cout << "[RECV.ch" << m_nChannel << "] type : " << m_type << endl;
	m_queue->Enable();

	m_begin = high_resolution_clock::now();

	while (!m_bExit)
	{
		rd = recvfrom(m_sock, buff_rcv, PACKET_HEADER_EXTEND_SIZE + PACKET_SIZE, 0, NULL, 0);
#if __DEBUG
		_d("[RECV.ch%d] rd : %d\n", m_nChannel, rd);
#endif
		if (rd < 1)
		{
			usleep(10);
			continue;
		}

		if (m_type == "audio")
		{
			void *p;
			int64_t audio_pts = -1;
			p = buff_rcv;
			memcpy(&recv_pts, p, sizeof(recv_pts));
			if (is_first_time_recv == false)
			{
				first_time_pts = recv_pts;
				is_first_time_recv = true;
			}
			if (recv_pts > 0)
			{
				// 0부터 시작
				audio_pts = (recv_pts - first_time_pts) / 1000;
				memcpy(p, &audio_pts, 8);
				//_d("[RECV.ch%d] (%d) audio pts(%lld/%lld)\n", m_nChannel, rd, recv_pts, audio_pts);
			}
			m_queue->PutAudio((char *)p, rd);
		}
		else if (m_type == "video")
		{
			memcpy(&recv_nTotalStreamSize, buff_rcv, sizeof(recv_nTotalStreamSize));
			memcpy(&recv_nCurStreamSize, buff_rcv + 4, sizeof(recv_nCurStreamSize));
			memcpy(&nTotalPacketNum, buff_rcv + 8, sizeof(nTotalPacketNum));
			memcpy(&nCurPacketNum, buff_rcv + 12, sizeof(nCurPacketNum));
			memcpy(&recv_type, buff_rcv + 16, 1);
			memcpy(&recv_codec_type, buff_rcv + 17, 1);
			memcpy(&recv_frame_type, buff_rcv + 18, 1);
			memcpy(&recv_reserve, buff_rcv + 19, sizeof(recv_reserve));

			if (recv_reserve[0] == 1)
			{
				//확장일 경우
				memcpy(&recv_pts, buff_rcv + 30, sizeof(recv_pts));
			}
#if 0
			for (int i = 0; i < PACKET_HEADER_SIZE; i++)
			{
				_d("offset : %d, mem[%d] : %x\n", i, i, buff_rcv[i]);
			}
#endif
			if ((nPrevPacketNum + 1) != nCurPacketNum)
			{
#if 0
				_d("\n[RECV.ch%d] TotalPacketNum(%d)PrevPacketNum(%d)/nCurPacketNum(%d)\n\n", m_nChannel, nTotalPacketNum, nPrevPacketNum, nCurPacketNum);
#endif
			}
			nPrevPacketNum = nCurPacketNum;
#if __DEBUG
			//_d("nTotalPacketNum(%d)/nCurPacketNum(%d), recv_nestedStreamSize(%d)\n", nTotalPacketNum, nCurPacketNum, recv_nestedStreamSize);
			cout << "[RECV.ch." << m_nChannel << "] nTotalPacketNum(" << nTotalPacketNum << "/" << nCurPacketNum << ", " << recv_nestedStreamSize << ")" << endl;
#endif
			//_d("add : %x, size : %d\n", m_frame_buf, MAX_frame_size);
			memcpy(m_frame_buf + recv_nestedStreamSize, buff_rcv + PACKET_HEADER_EXTEND_SIZE, recv_nCurStreamSize);
			recv_nestedStreamSize += recv_nCurStreamSize;
#if __DEBUG
			_d("[RECV.ch%d] (%d/%d)\n", m_nChannel, recv_nCurStreamSize, recv_nestedStreamSize);
#endif
			if (nTotalPacketNum == nCurPacketNum)
			{
#if 0
				m_end = high_resolution_clock::now();
				tick_diff = duration_cast<microseconds>(m_end - m_begin).count();
#endif
				m_nFrameCount++;
				// 1 frame 만들어 졌을 때
#if 0
				_d("[RECV.ch%d] recv_nTotalStreamSize(%d)/recv_nestedStreamSize(%d)\n", m_nChannel, recv_nTotalStreamSize, recv_nestedStreamSize);
				_d("[RECV.ch%d] 1 (%d) frame created\n", m_nChannel, recv_nestedStreamSize);
#endif
#if 0
				fwrite(m_frame_buf, 1, recv_nestedStreamSize, es);
#endif
				AVPacket pkt;
				av_init_packet(&pkt);

				if (is_first_time_recv == false)
				{
					first_time_pts = recv_pts;
					is_first_time_recv = true;
				}

				pkt.data = m_frame_buf;
				pkt.size = recv_nestedStreamSize;
#if 0
				pkt.pts = m_pts;
#endif
				if (recv_pts > 0)
				{
					// 0부터 시작
					pkt.pts = (recv_pts - first_time_pts) / 1000;
					m_pts = pkt.pts;
					tick_diff = pkt.pts;
				}

				if (recv_frame_type == 1)
				{
					pkt.flags = AV_PKT_FLAG_KEY;
				}
#if 0
				_d("[RECV.ch%d][%d] size : %d, type : %d, pkt.flags : %d, pts :  %lld, dur : %lld\n", m_nChannel, m_nFrameCount, pkt.size, recv_frame_type, pkt.flags, pkt.pts, tick_diff - old_tick_diff);
#endif
				m_queue->Put(&pkt, (int)recv_codec_type, (int)recv_type);
				old_tick_diff = tick_diff;

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
	if (m_sock > 0)
	{
		close(m_sock); // release socket
		cout << "[RECV.ch" << m_nChannel << "] sock " << m_sock << " is closed" << endl;
	}

	SAFE_DELETE(m_mux);
	m_mux = NULL;
}