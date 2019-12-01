#include "comm.h"

using namespace std;

CCommMgr::CCommMgr()
{
}

CCommMgr::~CCommMgr()
{
}

bool CCommMgr::Open(int nPort)
{
    m_nPort = nPort;
    Start();

    return true;
}

void CCommMgr::Run()
{
    int t = 1;
    char buff[4];
    struct sockaddr_in sin;

    m_sdListen = socket(PF_INET, SOCK_DGRAM, 0);
    if (m_sdListen < 0)
    {
        _d("[CMGR] Failed to open socket\n");
    }

    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = PF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(m_nPort);

    setsockopt(m_sdListen, SOL_SOCKET, SO_REUSEADDR, (const char *)&t, sizeof(t));

    if (bind(m_sdListen, (const sockaddr *)&sin, sizeof(sin)) < 0)
    {
        _d("[CMGR] Failed to bind to port %d\n", sin.sin_port);
    }

    socklen_t sin_size = sizeof(sin);

    cout << "udp ready port : " << m_nPort << endl;

    while (!m_bExit)
    {
        recvfrom(m_sdListen, buff, 4, 0, (struct sockaddr *)&sin, &sin_size);
        _d("%c %c %c %c\n", buff[0], buff[1], buff[2], buff[3]);

        if (buff[0] == 't' && buff[1] == 'n')
        {
            if (buff[2] == '0')
            {
                if (buff[3] == '1')
                {
                    _d("tn01 실행\n");
                }
                else if (buff[3] == '2')
                {
                    _d("tn02 실행\n");
                }
            }
        }
        else
        {
            _d("[CCT] Unknown sync code...%c %c %c %c\n", buff[0], buff[1], buff[2], buff[3]);
        }
    }

    _d("[CMGR] exit loop\n");
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
