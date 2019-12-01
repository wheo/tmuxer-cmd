#ifndef _CCORE_H_
#define _CCORE_H_

#define MAX_NUM_TRAPS 32
#define MAX_NUM_CHANNEL 6

#include "recv.h"
#include "comm.h"

class CCore : public PThread
{
public:
	CCore(void);
	~CCore(void);

	bool Create();
	void Delete();

protected:
	pthread_mutex_t m_mutex_core;

private:
	int m_nChannel;
	Json::Value m_root;
	Json::Reader m_reader;
	CRecv *m_CRecv[MAX_NUM_CHANNEL];
	CCommMgr *m_comm;

protected:
	void Run();
	void OnTerminate(){};
};
#endif // _CCORE
