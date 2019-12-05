#include "comm.h"

using namespace std;

CCommMgr::CCommMgr()
{
    m_bExit = false;
    pthread_mutex_init(&m_mutex_comm, 0);
    m_RecvBuf = new char[MAX_RECV_BUFFER];
}

CCommMgr::~CCommMgr()
{
    m_bExit = true;
    Delete();
    pthread_mutex_destroy(&m_mutex_comm);
    if ( m_sdRecv > 0 ) {
        close(m_sdRecv);
    }
    if ( m_sdSend > 0 ) {
        close(m_sdSend);
    }
    SAFE_DELETE(m_RecvBuf);
}

bool CCommMgr::SetSocket() {
    int t = 1;
    struct sockaddr_in sin;

    m_sdRecv = socket(PF_INET, SOCK_DGRAM, 0);
    if (m_sdRecv < 0)
    {
        _d("[COMM] Failed to open rx socket\n");
    }

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(m_nPort);

    setsockopt(m_sdRecv, SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));

    if (bind(m_sdRecv, (const sockaddr *)&sin, sizeof(sin)) < 0)
    {
        _d("[COMM] Failed to bind to port %d\n", sin.sin_port);
    }

    m_sdSend = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if ( m_sdSend < 0 ) {
        _d("[COMM] failed to open tx socket\n");
    }

    return EXIT_SUCCESS;
}

bool CCommMgr::Open(int nPort, Json::Value attr)
{
    m_nChannel = 0;
    m_bIsRunning = false;
    m_nPort = nPort;
    m_attr = attr;

    SetSocket();
    Start();

    return true;
}

bool CCommMgr::ReadBuffer() {
    // memcpy(&m_sin, &sin, sizeof(sin));
    char *buff = m_RecvBuf;
    mux_cfg_s *mux_cfg;

    stringstream sstm;
    //memset(mux_cfg, 0x00, sizeof(mux_cfg_s)); // 혹시 segfault 원인?

    _d("%c %c %d %d state : %d\n", buff[0], buff[1], buff[2], buff[3], buff[4]);
    if (buff[0] == 'T' && buff[1] == 'N')
    {
        if (buff[2] == 0x00)
        {
            if (buff[3] == 0x01)
            {
                if (!m_bIsRunning)
                {
                    m_bIsRunning = true;
                    m_nChannel = 0;
                    int nStructSize = sizeof(mux_cfg_s);
                    int nBuffSize = 0;
                    // buff[4] is enable bit 연산
                    nBuffSize += 5;

                    char *pData = &buff[5];
                    for (int i=0;i < MAX_VIDEO_CHANNEL_COUNT; i++) {
                        memcpy(pData, &mux_cfg[i], nStructSize);
                        pData += nStructSize;
                        nBuffSize += nStructSize;
                    }
                    char strName[260] = {0,};
                    memcpy(strName, pData, sizeof(strName));
                    nBuffSize += sizeof(strName);

                    _d("[COMM] received event filename : %s\n", strName);

                    int json_channel_info;
                    json_channel_info = m_nChannel;

                    for (auto &value : m_attr["channels"])
                    {
                        if (strlen (strName) > 0 ) {
                            m_attr["folder_name"] = strName;
                        } else {
                            m_attr["folder_name"] = get_current_time_and_date();
                        }
                        int bit_state = 0;
                        int result = 0;

                        if ( buff[4] > 0 ) {
                            m_attr["bit_state"] = buff[4];
                            bit_state = m_attr["bit_state"].asInt();
                            result = bit_state & (1 << m_nChannel);
                        }
                        m_CRecv[m_nChannel] = new CRecv();
                        if (m_CRecv[m_nChannel]->Create(m_attr["channels"][m_nChannel], m_attr, m_nChannel) ) {
                            cout << "[COMM] Recv(" << m_nChannel << ") thread is created" << endl;
                        } else {
                            cout << "[COMM] Recv(" << m_nChannel << ") thread is failed" << endl;
                        }
                        if ( result > 0 ) {
                            m_attr["channel_info"].append(m_nChannel);
                        }
                        m_nChannel++;
                    }

                    sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << m_attr["folder_name"].asString() << "/" << m_nChannel;
                    system(sstm.str().c_str());
                    sstm.str("");

                    sstm << m_attr["file_dst"].asString() << "/" << m_attr["folder_name"].asString() << "/" << "info.json";

                    Json::Value info;
                    info["channel_info"] = m_attr["channel_info"];
                    CreateMetaJson(info, sstm.str());
                    sstm.str("");
                }
                else
                {
                    cout << "[COMM] Running...." << endl;
                }
            }
            else if (buff[3] == 0x02)
            {
                if (m_bIsRunning)
                {
                    Delete();
                    _d("[COMM] muxing 종료\n");
                    m_nChannel = 0;
                    m_bIsRunning = false;
                }
                else
                {
                    cout << "[COMM] Service is not running" << endl;
                }
            }
        }
    }
    else
    {
        _d("[COMM] Unknown sync code...%c %c %c %c\n", buff[0], buff[1], buff[2], buff[3]);
    }
}

bool CCommMgr::RX() {
    char buff[READ_SIZE];

    struct sockaddr_in sin;
    int rd_size=0;
    m_rd_length=0;

    socklen_t sin_size = sizeof(sin);

    cout << "[COMM] udp RX ready (port) : " << m_nPort << endl;

    while (!m_bExit)
    {
        rd_size = recvfrom(m_sdRecv, buff, READ_SIZE, 0, (struct sockaddr *)&sin, &sin_size);
        memcpy(m_RecvBuf, buff, rd_size);
        cout << "rd_size : " << rd_size << endl;
        //추후 thread 처리
        ReadBuffer();
    }
    _d("[COMM] exit loop\n");
}

bool CCommMgr::TX(char *buff) {
    if ( m_sdSend < 0 ) {
        cout << "[COMM] send socket not created" << endl;
        return false;
    }

    struct sockaddr_in sin;
    socklen_t sin_size = sizeof(sin);

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(m_attr["uep_muxer_ip"].asString().c_str());
    sin.sin_port = m_nPort;

    sendto(m_sdRecv, buff, sizeof(buff), 0, (struct sockaddr *)&sin, sin_size);
    cout << "ip : " << inet_ntoa(sin.sin_addr) << " send message : " << buff[0] << buff[1] << buff[2] << buff[3] << endl;
}

void CCommMgr::Run()
{
    RX();
}

void CCommMgr::Delete()
{
    for (int i = 0; i < m_nChannel; i++)
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
