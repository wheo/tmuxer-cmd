#include "main.h"
#include "queue.h"

CQueue::CQueue()
{
	m_nMaxQueue = MAX_NUM_QUEUE;
	m_nMaxAudioQueue = MAX_NUM_AUDIO_QUEUE;

#if 0
	for (int i = 0; i < m_nMaxQueue; i++)
	{
		m_pkt[i] = av_packet_alloc();
		av_init_packet(m_pkt[i]);
		m_pkt[i]->data = NULL;
		m_pkt[i]->size = 0;
	}
#endif

	for (int i = 0; i < m_nMaxAudioQueue; i++)
	{
		m_e[i].p = NULL;
		m_e[i].len = 0;
	}

	m_nSizeQueue = 0;

	m_bEnable = false;
	m_nPacket = 0;
	m_nAudio = 0;

	m_nReadPos = 0;
	m_nWritePos = 0;
	m_nReadAudioPos = 0;
	m_nWriteAudioPos = 0;

	pthread_mutex_init(&m_mutex, NULL);
}

CQueue::~CQueue()
{
	pthread_mutex_destroy(&m_mutex);
}

void CQueue::SetInfo(int nChannel, string type)
{
	m_nChannel = nChannel;
	m_type = type;
}

void CQueue::Clear()
{
#if 0
	for (int i = 0; i < m_nMaxQueue; i++)
	{
		//av_packet_unref(m_pkt[i]);
	}
#endif

	m_nReadPos = 0;
	m_nWritePos = 0;

	m_nReadAudioPos = 0;
	m_nWriteAudioPos = 0;

	m_bEnable = false;
	m_nPacket = 0;
	m_nAudio = 0;
}

void CQueue::Enable()
{
	m_bEnable = true;

	//m_nReadPos = m_nWritePos;
	//_d("[QUEUE] Enable packet outputing now...%d\n", m_nPacket);
	cout << "[QUEUE.ch" << m_nChannel << "] Enable packet outputing now... " << m_nPacket << endl;
}

bool CQueue::Put(AVPacket *pkt)
{
	int nCount = 0; // timeout 위한 용도

	while (true)
	{
		pthread_mutex_lock(&m_mutex);
		if (pkt->size > 0)
		{
			//m_pkt[m_nWritePos] = av_packet_alloc();
			av_init_packet(&m_pkt[m_nWritePos]);
			av_packet_ref(&m_pkt[m_nWritePos], pkt);
			av_packet_unref(pkt);
			//av_packet_free(&pkt);
#if __DEBUG
			_d("[QUEUE.ch%d] put pos : ( %d ), stream_index : %d, data ( %p ), size ( %d )\n", m_nChannel, m_nWritePos, m_pkt[m_nWritePos]->stream_index, m_pkt[m_nWritePos]->data, m_pkt[m_nWritePos]->size);
#endif
			m_nWritePos++;
			if (m_nWritePos >= m_nMaxQueue)
			{
				m_nWritePos = 0;
			}
			m_nPacket++;
			pthread_mutex_unlock(&m_mutex);
			return true;
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
	return false;
}

int CQueue::PutAudio(char *pData, int nSize)
{
	int nCount = 0;

	while (true)
	{
		ELEM *pe = &m_e[m_nWriteAudioPos];
		pe->p = new char[nSize];
		pthread_mutex_lock(&m_mutex);
		if (pe->len == 0)
		{
			memcpy(pe->p, pData, nSize);
			pe->len = nSize;

			m_nWriteAudioPos++;
			if (m_nWriteAudioPos >= m_nMaxAudioQueue)
			{
				m_nWriteAudioPos = 0;
			}
			m_nAudio++;
			//cout << "[QUEUE.ch" << m_nChannel << "] m_nWriteAudioPos : " << m_nWriteAudioPos << endl;
			pthread_mutex_unlock(&m_mutex);
			return 0;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);

		nCount++;
		if (nCount >= 10000)
		{
			cout << "[QUEUE.ch" << m_nChannel << " ] Timeout" << endl;
			break;
		}
	}
}
bool CQueue::IsNextKeyFrame()
{
	if (m_pkt[m_nReadPos + 1].flags == AV_PKT_FLAG_KEY)
	{
		return true;
	}
	else
	{
		return false;
	}
}

int CQueue::Get(AVPacket *pkt)
{
	if (m_bEnable == false)
	{
		return NULL;
	}

	if (m_nPacket < MIN_BUF_FRAME)
	{
		return NULL;
	}

	pthread_mutex_lock(&m_mutex);

	if (m_pkt[m_nReadPos].size > 0)
	{
#if __DEBUG
		cout << "[QUEUE.ch" << m_nChannel << "] m_nReadPos : " << m_nReadPos << ", size : " << m_pkt[m_nReadPos]->size << endl;
#endif
		//av_init_packet(pkt);
		av_packet_ref(pkt, &m_pkt[m_nReadPos]);
#if 0
			if (m_nPacket < m_nDelay)
			{
				pthread_mutex_unlock(&m_mutex);
				return NULL;
			}
#endif
#if __DEBUG
		_d("[QUEUE.ch%d] get pos ( %d ), size ( %d ), data ( %p ),  pts ( %lld ), type : %d, m_nPacket : %d\n", m_nChannel, m_nReadPos, m_pkt[m_nReadPos]->size, m_pkt[m_nReadPos]->data,
		   m_pkt[m_nReadPos]->pts, m_pkt[m_nReadPos]->stream_index, m_nPacket);
#endif
		av_packet_unref(&m_pkt[m_nReadPos]);
		//av_packet_free(m_pkt[m_nReadPos]);
		pthread_mutex_unlock(&m_mutex);
		return pkt->size;
	}
	else
	{
		pthread_mutex_unlock(&m_mutex);
		return 0;
	}
	//pthread_mutex_unlock(&m_mutex);
	//usleep(5);
}

void *CQueue::GetAudio()
{
	if (m_bEnable == false)
	{
		return NULL;
	}
#if 0
	if (m_nPacket < MIN_BUF_FRAME)
	{
		return NULL;
	}
#endif

	while (true)
	{
		ELEM *pe = &m_e[m_nReadAudioPos];

		pthread_mutex_lock(&m_mutex);
		if (pe->len)
		{
#if __DEBUG
			cout << "[QUEUE.ch" << m_nChannel << "] m_nReadAudioPos : " << m_nReadAudioPos << ", pe->len : " << pe->len << endl;
#endif
			pthread_mutex_unlock(&m_mutex);
			return pe;
		}
		pthread_mutex_unlock(&m_mutex);
		usleep(10);
		//break;
		return NULL;
	}
}

void CQueue::Ret(AVPacket *pkt)
{
	pthread_mutex_lock(&m_mutex);

	av_packet_unref(pkt);
	av_packet_free(&pkt);

	m_nReadPos++;
#if __DEBUG
	if (m_nChannel == 0)
	{
		cout << "[QUEUE.ch" << m_nChannel << ":RET] m_nReadPos : " << m_nReadPos << endl;
	}
#endif
	if (m_nReadPos >= m_nMaxQueue)
	{
		m_nReadPos = 0;
	}
	m_nPacket--;
	pthread_mutex_unlock(&m_mutex);
}

void CQueue::RetAudio(void *p)
{
	ELEM *pe = (ELEM *)p;
	pthread_mutex_lock(&m_mutex);

	pe->len = 0;
	m_nReadAudioPos++;
	if (m_nReadAudioPos >= m_nMaxAudioQueue)
	{
		m_nReadAudioPos = 0;
	}
	m_nAudio--;
	if (pe->p)
	{
		delete pe->p;
		pe->p = NULL;
		pe->len = 0;
	}
	pthread_mutex_unlock(&m_mutex);
}
