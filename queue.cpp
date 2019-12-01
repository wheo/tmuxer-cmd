#include "main.h"
#include "queue.h"

CQueue::CQueue()
{
	m_nMaxQueue = MAX_NUM_QUEUE;
	m_nSizeQueue = 0;
	m_pBuffer = NULL;

	m_bEnable = false;
	m_nPacket = 0;
	pthread_mutex_init(&m_mutex, NULL);
}

CQueue::~CQueue()
{
	if (m_pBuffer)
	{
		free(m_pBuffer);
		m_pBuffer = NULL;
	}
	pthread_mutex_destroy(&m_mutex);
}

void CQueue::SetChannel(int nChannel)
{
	m_nChannel = nChannel;
}

void CQueue::Clear()
{
	for (int i = 0; i < m_nMaxQueue; i++)
	{
		//avpacket unref 해야 함
	}

	m_nReadPos = 0;
	m_nWritePos = 0;

	m_nReadFramePos = 0;
	m_nWriteFramePos = 0;

	m_bEnable = false;
	m_nPacket = 0;
}

void CQueue::Enable()
{
	m_bEnable = true;

	//m_nReadPos = m_nWritePos;
	//_d("[QUEUE] Enable packet outputing now...%d\n", m_nPacket);
	cout << "[QUEUE.ch" << m_nChannel << "] Enable packet outputing now... " << m_nPacket << endl;
}

int CQueue::Put(AVPacket *pkt)
{
	int nCount = 0;

	while (true)
	{
		pthread_mutex_lock(&m_mutex);
		if (pkt->size > 0)
		{
			av_init_packet(&m_pkt[m_nWritePos]);
			av_packet_ref(&m_pkt[m_nWritePos], pkt);
			av_packet_unref(pkt);
#if __DEBUG
			if (m_nChannel == 0)
			{
				_d("put pos : ( %d ), stream_index : %d, data ( %p ), size ( %d )\n", m_nWritePos, pkt->stream_index, m_pkt[m_nWritePos].data, m_pkt[m_nWritePos].size);
			}
#endif
			m_nWritePos++;
			if (m_nWritePos >= m_nMaxQueue)
			{
				m_nWritePos = 0;
			}
			m_nPacket++;
			pthread_mutex_unlock(&m_mutex);
			return m_pkt[m_nWritePos - 1].size;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);

		nCount++;
		if (nCount >= 100000)
		{
			//_d("Timeout\n");
			break;
		}
	}
	return 0;
}

int CQueue::Get(AVPacket *pkt)
{
	if (m_bEnable == false)
	{
		return NULL;
	}

	while (true)
	{
		pthread_mutex_lock(&m_mutex);

		if (m_pkt[m_nReadPos].size > 0)
		{
			av_init_packet(pkt);
			av_packet_ref(pkt, &m_pkt[m_nReadPos]);
#if 0
			if (m_nPacket < m_nDelay)
			{
				pthread_mutex_unlock(&m_mutex);
				return NULL;
			}
#endif
#if __DEBUG
			if (m_nChannel == 0)
			{
				_d("get pos ( %d ), size ( %d ), data ( %p ),  type : %d, m_nPacket : %d\n", m_nReadPos, m_pkt[m_nReadPos].size, m_pkt[m_nReadPos].data, m_pkt[m_nReadPos].stream_index, m_nPacket);
			}
#endif
			av_packet_unref(&m_pkt[m_nReadPos]);
			pthread_mutex_unlock(&m_mutex);
			return pkt->size;
		}
		pthread_mutex_unlock(&m_mutex);
		//usleep(5);
	}
}

void CQueue::Ret(AVPacket *pkt)
{
	pthread_mutex_lock(&m_mutex);

	av_packet_unref(pkt);

	m_nReadPos++;
#if __DEBUG
	if (m_nChannel == 0)
	{
		cout << "m_nReadPos : " << m_nReadPos << endl;
	}
#endif
	if (m_nReadPos >= m_nMaxQueue)
	{
		m_nReadPos = 0;
	}
	m_nPacket--;
	pthread_mutex_unlock(&m_mutex);
}