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
	Delete();
	Terminate();
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
    m_type = info["type"].asString();

	m_pMuxer = new CTSMuxer();
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
	return true;
}

bool CMux::SetQueue(CQueue **queue, int nChannel)
{
	m_queue = *queue;
	m_nChannel = nChannel;
    return EXIT_SUCCESS;
}

void CMux::Run()
{
	while (!m_bExit)
	{
		if (!Muxing())
		{
			//error
            break;
		}
		else
		{
            //success
		}
	}
}


bool CMux::Muxing()
{
	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));

    int bit_state = m_attr["bit_state"].asInt();
    int result = bit_state & (1 << m_nChannel);

	string sub_dir_name;
    string group_name;

    if ( m_attr["folder_name"].asString().size() > 0 ) {
        sub_dir_name = m_attr["folder_name"].asString();
    } else {
        sub_dir_name = "error";
    }
    if ( m_attr["bit_state"].asString().size() > 0 ) {
        cout << "channel : " << m_nChannel << ", bit state is " << bit_state << ", result : " << result << endl;
        if ( result < 1 ) {
            cout << "channel " << m_nChannel << " is not use anymore" << endl;
            return false;
        }
    }
	stringstream sstm;
	sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel;
	system(sstm.str().c_str());
	// clear method is not working then .str("") correct
	sstm.str("");

	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << "meta.json";
    group_name = sstm.str();
    sstm.str("");

	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
	m_filename = sstm.str();
	cout << "[MUX.ch." << m_nChannel << "] expected output file name : " << m_filename << endl;
    sstm.str("");

#if __DUMP
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".es";
    m_es_name = sstm.str();
#endif

	mux_cfg.output = 1;
	mux_cfg.vid.codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	//mux_cfg.vid.bitrate = 6000000;
	mux_cfg.vid.fps = m_info["fps"].asDouble();
	mux_cfg.vid.width = m_info["width"].asInt();
	mux_cfg.vid.height = m_info["height"].asInt();

    Json::Value meta;

    meta["codec"] = mux_cfg.vid.codec;
    meta["fps"] = mux_cfg.vid.fps;
    meta["width"] = mux_cfg.vid.width;
    meta["height"] = mux_cfg.vid.height;
    meta["type"] = m_type;

    CreateMetaJson(meta, group_name);

    if ( m_type == "video" ) {
        m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);
    }

#if __DUMP
    string channel;
    FILE *es = fopen(m_es_name.c_str(), "wb");
#endif

	while (!m_bExit)
	{
		m_nRecSec = m_attr["rec_sec"].asInt();
		AVPacket pPkt;
		if (m_queue->Get(&pPkt) > 0)
		{
#if __DUMP
            fwrite(pPkt.data, 1, pPkt.size, es);
#endif
            if ( m_type == "video" ) {
                m_pMuxer->put_data(&pPkt);
            }
			m_queue->Ret(&pPkt);
		}
		else
		{
			cout << "[CMUX.ch" << m_nChannel << "] queue size is 0" << endl;
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

                if ( m_type == "video" ) {
                    SAFE_DELETE(m_pMuxer);
                    m_pMuxer = new CTSMuxer();

                    m_file_idx++;
                    sstm.str("");
                    sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
                    m_filename = sstm.str();
                    m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);
                }
#if __DUMP
                fclose(es);
                sstm.str("");
				sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".es";
                m_es_name = sstm.str();
                es = fopen(m_es_name.c_str(), "wb");
#endif
			}
		}
	}
#if __DUMP
    fclose(es);
#endif
    cout << "[MUX.ch" << m_nChannel << "] loop out" << endl;
	return true;
}

void CMux::Delete()
{
	SAFE_DELETE(m_pMuxer);
#if __DEBUG
	cout << "sock " << m_sock << " closed" << endl;
#endif
}
