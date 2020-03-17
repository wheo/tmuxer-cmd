#include "comm.h"

CCommMgr::CCommMgr()
{
	m_bExit = false;
	m_bIsRunning = false;
	pthread_mutex_init(&m_mutex_comm, 0);
	m_RecvBuf = new char[MAX_RECV_BUFFER];
}

CCommMgr::~CCommMgr()
{
	m_bExit = true;
	Delete();
	pthread_mutex_destroy(&m_mutex_comm);
	if (m_sdRecv > 0)
	{
		close(m_sdRecv);
	}
	if (m_sdSend > 0)
	{
		close(m_sdSend);
	}
	SAFE_DELETE_ARRAY(m_RecvBuf);
}

bool CCommMgr::SetSocket()
{
	int t = 1;
	struct sockaddr_in sin;

	m_sdRecv = socket(PF_INET, SOCK_DGRAM, 0);
	if (m_sdRecv < 0)
	{
		cout << "[COMM] Failed to open rx socket" << endl;
	}

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(m_nPort);

	setsockopt(m_sdRecv, SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));

	if (bind(m_sdRecv, (const sockaddr *)&sin, sizeof(sin)) < 0)
	{
		_d("[COMM] Failed to bind to port %d\n", sin.sin_port);
		return false;
	}

	m_sdSend = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (m_sdSend < 0)
	{
		_d("[COMM] failed to open tx socket\n");
		return false;
	}

	return true;
}

//nType : 0 상시녹화, 1 : 이벤트 녹화
bool CCommMgr::Open(int nType, int nPort, Json::Value attr)
{
	m_nType = nType;
	m_nChannel = 0;
	m_bIsRunning = false;
	m_nPort = nPort;
	m_attr = attr;

	if (!SetSocket())
	{
		// failed
	}
	Start();

	return true;
}

void CCommMgr::Run()
{
	RX();
}

bool CCommMgr::RX()
{
	char buff[READ_SIZE];

	struct sockaddr_in sin;
	int rd_size = 0;
	m_rd_length = 0;

	socklen_t sin_size = sizeof(sin);

	cout << "[COMM] udp RX ready (port) : " << m_nPort << endl;

	while (!m_bExit)
	{
		memset(buff, 0x00, sizeof(buff));
		rd_size = recvfrom(m_sdRecv, buff, READ_SIZE, 0, (struct sockaddr *)&sin, &sin_size);
		memcpy(m_RecvBuf, buff, rd_size);
		//cout << "[COMM] rd_size : " << rd_size << endl;
		//sendto(m_sdSend, buff, sizeof(buff), 0, (struct sockaddr *)&sin, sin_size);
		TX(buff, rd_size);
		//추후 thread 처리
		ReadBuffer();
	}
	_d("[COMM] exit loop\n");
}

bool CCommMgr::TX(char *buff, int size)
{
	if (m_sdSend < 0)
	{
		cout << "[COMM] send socket not created" << endl;
		return false;
	}

	struct sockaddr_in sin;
	socklen_t sin_size = sizeof(sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(m_attr["udp_target_ip"].asString().c_str());
	sin.sin_port = htons(m_attr["udp_target_port"].asInt());

	sendto(m_sdSend, buff, size, 0, (struct sockaddr *)&sin, sin_size);
	cout << "[COMM] ip : " << inet_ntoa(sin.sin_addr) << " port : " << m_nPort << ", send message(" << size << ")" << endl;
}

bool CCommMgr::ReadBuffer()
{
	// memcpy(&m_sin, &sin, sizeof(sin));

	mux_cfg_s *mux_cfg;

	//Json::Value root;
	string strbuf;
	strbuf = (char *)m_RecvBuf;

	if (reader.parse(strbuf, m_root, true))
	{
		//parse success
		if (m_root["cmd"] == "save_start")
		{
			MuxStart();
		}
		else if (m_root["cmd"] == "save_stop")
		{
			MuxStop();
		}
	}
	else
	{
		cout << "[COMM] message is json" << endl;
	}
}

bool CCommMgr::MuxStart()
{
	stringstream sstm;
	if (m_bIsRunning == false)
	{
		m_bIsRunning = true;
		m_nChannel = 0;

		int bit_state = m_root["info"]["bit_state"].asInt();
		int result = 0;
		string file_name = m_root["info"]["file_name"].asString();

		if (file_name.size() > 0)
		{
			m_attr["folder_name"] = file_name;
		}
		else
		{
			m_attr["folder_name"] = get_current_time_and_date();
		}

		//for (auto &value : m_attr["channels"])
		for (int i = 0; i < MAX_ALL_CHANNEL_COUNT; i++)
		{
			//cout << "file_name : " << root["info"]["file_name"].asString() << endl;
			//cout << "m_attr[folder_name] : " << m_attr["folder_name"].asString() << endl;

			if (bit_state > 0)
			{
				m_attr["bit_state"] = bit_state;
				//result = bit_state & (1 << m_nChannel);
			}
			m_CRecv[m_nChannel] = new CRecv();
			if (m_CRecv[m_nChannel]->Create(m_nType, m_attr["channels"][m_nChannel], m_attr, m_nChannel))
			{
				cout << "[COMM] Recv(" << m_nChannel << ") thread is created" << endl;
			}
			else
			{
				cout << "[COMM] Recv(" << m_nChannel << ") thread is failed" << endl;
			}
			m_nChannel++;
		}

		sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << m_attr["folder_name"].asString();
		system(sstm.str().c_str());
		sstm.str("");

		m_root["time"] = get_current_time_and_date();

		sstm << m_attr["file_dst"].asString() << "/" << m_attr["folder_name"].asString() << "/"
			 << "info.json";
		CreateMetaJson(m_root, sstm.str());
		sstm.str("");
	}
	else
	{
		cout << "[COMM] Service is running" << endl;
	}
}

bool CCommMgr::MuxStop()
{
	if (m_bIsRunning == true)
	{
		m_bIsRunning = false;
		Delete();
		_d("[COMM] muxing completed\n");
		m_nChannel = 0;
	}
	else
	{
		cout << "[COMM] Service is not running" << endl;
	}
}

bool CCommMgr::Restart()
{
	if (m_bIsRunning == true)
	{
		MuxStop();
		MuxStart();
		cout << "[COMM] Recv(" << m_nChannel << ") muxer is restarted" << endl;
	}

	return true;
}

void CCommMgr::Delete()
{
	cout << "[COMM] DELETE channel count : " << m_nChannel << endl;
	for (int i = 0; i < MAX_ALL_CHANNEL_COUNT; i++)
	{
		SAFE_DELETE(m_CRecv[i]);
	}
}

CCommBase::CCommBase()
{
	m_sd = -1;
	m_bIsAlive = false;
}

CCommBase::~CCommBase()
{
	if (m_sd >= 0)
	{
		shutdown(m_sd, 2);
		close(m_sd);

		m_sd = -1;
		m_bIsAlive = false;
	}
}

int CCommBase::SR(char *pBuff, int nLen)
{
	int w = 0, local = nLen;

	while (local)
	{
		int r = recv(m_sd, &pBuff[w], local, 0);
		if (r > 0)
		{
			w += r;
			local -= r;
		}
		else
		{
			m_bIsAlive = false;
			return -1;
		}
	}

	return w;
}

int CCommBase::SS(char *pBuff, int nLen)
{
	int w = 0, local = nLen;

	while (local)
	{
		int s = send(m_sd, &pBuff[w], local, 0);
		if (s > 0)
		{
			w += s;
			local -= s;
		}
		else
		{
			m_bIsAlive = false;
			return -1;
		}
	}

	return w;
}
