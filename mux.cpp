#include "mux.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4
#define MAX_frame_size 1024 * 1024 // 1MB

CMux::CMux(void)
{
	m_bExit = false;
	pthread_mutex_init(&m_mutex_mux, 0);
}

CMux::~CMux(void)
{
	m_bExit = true;

	_d("[MUX.ch%d] Trying to exit thread\n", m_nChannel);
	Terminate();
	Delete();
	_d("[MUX.ch%d] exited...\n", m_nChannel);

	pthread_mutex_destroy(&m_mutex_mux);
}

void CMux::log(int type, int state)
{
}

bool CMux::Create(Json::Value info, Json::Value attr, int nChannel)
{
	m_info = info;
	m_nChannel = nChannel;
	m_attr = attr;
	m_file_idx = 0;

#if 0
	Json::FastWriter writer;
	string str_info = writer.write(m_info);
	cout << "json info : " << str_info << endl;
	string str_attr = writer.write(m_attr);
	cout << "json attr : " << str_attr << endl;
#endif

	m_pMuxer = new CTSMuxer();
	//pthread_mutex_lock(&m_mutex_recv);
	if (m_queue)
	{
		cout << "[CMUX.ch" << nChannel << "] Worker created" << endl;
		Start();
	}
	else
	{
		cout << "[CMUX.ch" << nChannel << "] Not yet set mux worker" << endl;
		return false;
	}
	//pthread_mutex_unlock(&m_mutex_recv);
	return true;
}

bool CMux::SetQueue(CQueue **queue, int nChannel)
{
	m_queue = *queue;
	m_nChannel = nChannel;
}

void CMux::Run()
{
	while (!m_bExit)
	{
		if (!Muxing())
		{
			//error
			// m_bExit = true;
		}
		else
		{
			// return true
		}
	}
	//_d("[MUX.ch%d] thread exit\n", m_nChannel);
}

bool CMux::Muxing()
{
	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));

	string sub_dir_name = get_current_time_and_date();
	stringstream sstm;
	sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel;
	system(sstm.str().c_str());
	// clear method is not working then .str("") correct
	sstm.str("");

	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
	m_filename = sstm.str();
	cout << "[MUX.ch." << m_nChannel << "] expected output file name : " << m_filename << endl;

	mux_cfg.output = 1;
	mux_cfg.vid.codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	mux_cfg.vid.bitrate = 6000000;
	mux_cfg.vid.fps = m_info["fps"].asDouble();
	mux_cfg.vid.width = 720;
	mux_cfg.vid.height = 480;
	m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);

	while (!m_bExit)
	{
		m_nRecSec = m_attr["rec_sec"].asInt();
		AVPacket pPkt;
		if (m_queue->Get(&pPkt) > 0)
		{
			m_pMuxer->put_data(&pPkt);
			m_queue->Ret(&pPkt);
		}
		else
		{
			cout << "[CMUX] queue size is 0" << endl;
			usleep(50000);
			continue;
		}

		if (m_nRecSec > 0)
		{
			m_nFrameCount++;
#if __DEBUG
			_d("current frame : %d\n", m_nFrameCount);
#endif
			int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
			if (m_nFrameCount >= nDstFrame)
			{
				m_nFrameCount = 0;

				cout << "[MUX.ch." << m_nChannel << "] " << m_filename << " mux completed" << endl;

				SAFE_DELETE(m_pMuxer);
				m_pMuxer = new CTSMuxer();

				m_file_idx++;
				sstm.str("");
				sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
				m_filename = sstm.str();
				m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);
			}
		}
	}
	return true;
}

void CMux::Delete()
{
	SAFE_DELETE(m_pMuxer);
#if __DEBUG
	cout << "sock " << m_sock << " closed" << endl;
#endif
}