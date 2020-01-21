#include "mux.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

#define MAX_NUM_bitstream 4

CMux::CMux(void)
{
	m_bExit = false;
	m_pMuxer = NULL;
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
	m_is_intra = false;

	m_nFrameCount = 0;
	m_nAudioCount = 0;

	m_nTotalFrameCount = 0;
	m_nTotalAudioCount = 0;
	is_old_intra = false;

	SetSocket();

	if (m_type == "video")
	{
		m_pMuxer = new CTSMuxer();
	}
	else
	{
		m_pMuxer = NULL;
	}

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

bool CMux::SetSocket()
{
	m_sdSend = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (m_sdSend < 0)
	{
		cout << "[COMM] failed to open tx socket" << endl;
		return false;
	}

	return true;
}

bool CMux::SetQueue(CQueue **queue, int nChannel)
{
	m_queue = *queue;
	m_nChannel = nChannel;
	return true;
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
	//cout << "[MUX.ch" << m_nChannel << "] loop out" << endl;
}

bool CMux::Muxing()
{
	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));

// recv로 이동
#if 0
	int bit_state = m_attr["bit_state"].asInt();
	int result = bit_state & (1 << m_nChannel);

	cout << "[MUX.ch." << m_nChannel << "] bit_state : " << bit_state << ", result : " << result << endl;

	string sub_dir_name;
	string group_name;
	sub_dir_name = m_attr["folder_name"].asString();

	stringstream sstm;
	sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel;
	system(sstm.str().c_str());
	// clear method is not working then .str("") correct
	sstm.str("");

	if (bit_state > 0)
	{
		cout << "channel : " << m_nChannel << ", bit state is " << bit_state << ", result : " << result << endl;
		if (result < 1)
		{
			cout << "channel " << m_nChannel << " is not use anymore" << endl;
			return false;
		}
	}
#else
	stringstream sstm;
	string sub_dir_name = m_attr["folder_name"].asString();
	string group_name;
	string file_dst = m_attr["file_dst"].asString();
	string ip_string = m_info["ip"].asString();
#endif

	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/"
		 << "meta.json";
	group_name = sstm.str();
	sstm.str("");

#if __IP_FILE_NAME
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".mp4";
#else
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".mp4";
#endif
	m_filename = sstm.str();
	cout << "[MUX.ch." << m_nChannel << "] expected output file name : " << m_filename << endl;
	sstm.str("");

#if __IP_FILE_NAME
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".es";
#else
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".es";
#endif
	m_es_name = sstm.str();
	sstm.str("");

#if __IP_FILE_NAME
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".audio";
#else
	sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".audio";
#endif
	m_audio_name = sstm.str();

	mux_cfg.output = 1;
	mux_cfg.vid.codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	int num = m_info["num"].asInt();
	int den = m_info["den"].asInt();
	mux_cfg.vid.num = num;
	mux_cfg.vid.den = den;
	mux_cfg.vid.fps = (double)den / (double)num;
	mux_cfg.vid.width = m_info["width"].asInt();
	mux_cfg.vid.height = m_info["height"].asInt();
	mux_cfg.vid.max_gop = 30;

	_d("[MUX.ch%d] fps : %.3f\n", m_nChannel, mux_cfg.vid.fps);

	Json::Value meta;
	Json::Value mux_files;

	meta["filename"] = m_filename;
	meta["channel"] = m_nChannel;
	meta["codec"] = mux_cfg.vid.codec;
	meta["fps"] = mux_cfg.vid.fps;
	meta["width"] = mux_cfg.vid.width;
	meta["height"] = mux_cfg.vid.height;
	meta["type"] = m_type;
	meta["num"] = m_info["num"].asInt();
	meta["den"] = m_info["den"].asInt();

	CreateMetaJson(meta, group_name);

	if (m_type == "video")
	{
		m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);
	}

	string channel;

	FILE *es = NULL;
	FILE *pAudio = NULL;

#if __DEBUG
	es = fopen(m_es_name.c_str(), "wb");
#endif

	if (m_type == "audio")
	{
		pAudio = fopen(m_audio_name.c_str(), "wb");
	}

	m_nRecSec = m_attr["rec_sec"].asInt();

	Json::Value root;
	Json::Value info;
	int size;
	int ret = 0;
	string strbuf;

	while (!m_bExit)
	{
		if (m_type == "video" && !m_bExit)
		{
			AVPacket pkt;
			av_init_packet(&pkt);
			if (m_queue->Get(&pkt) > 0)
			{
				//es write
				if (pkt.flags == AV_PKT_FLAG_KEY)
				{
					m_is_intra = true;
				}
#if __INTRA_FRAME_FIRST
				if (m_is_intra == true)
#else
				if (true)
#endif
				{
#if __DEBUG
					fwrite(pPkt->data, 1, pPkt->size, es);
#endif
					m_pMuxer->put_data(&pkt);
				}
				m_queue->Ret(&pkt);
			}
			else
			{
				//cout << "[CMUX.ch" << m_nChannel << "] queue size is 0" << endl;
				usleep(10);
				continue;
			}

			if (m_nRecSec > 0)
			{
				m_nFrameCount++;
				m_nTotalFrameCount++;
#if __DEBUG
				_d("[MUX.ch.%d] current frame : %d\n", m_nChannel, m_nFrameCount);
#endif
				int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
				if (m_nFrameCount >= nDstFrame && m_queue->IsNextKeyFrame())
				{
					int finalFrameCount = 0;
					finalFrameCount = m_nFrameCount;
					string final_filename = m_filename;
					m_nFrameCount = 0;

					cout << "[MUX.ch." << m_nChannel << "] " << m_filename << " mux completed" << endl;
					m_file_idx++;

					SAFE_DELETE(m_pMuxer);

					info["filename"] = m_filename;
					size = getFilesize(m_filename.c_str());
					info["size"] = size;
					root["info"] = info;
					root["cmd"] = "get_file_info";

					Json::StreamWriterBuilder builder;
					builder["commentStyle"] = "None";
					builder["indentation"] = "";

					strbuf = Json::writeString(builder, root);
					TX((char *)strbuf.c_str(), strbuf.size());

					m_pMuxer = new CTSMuxer();
					sstm.str("");
#if __IP_FILE_NAME
					sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".mp4";
#else
					sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".mp4";
#endif
					m_filename = sstm.str();
					m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg, m_nTotalFrameCount);

#if __DEBUG
					fclose(es);
#endif
					m_is_intra = false;

					mux_files["name"] = final_filename;
					mux_files["frame"] = finalFrameCount;
					meta["files"].append(mux_files);
					CreateMetaJson(meta, group_name);

					sstm.str("");
#if __IP_FILE_NAME
					sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".es";
#else
					sstm << file_dst << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".es";
#endif
					m_es_name = sstm.str();
#if __DEBUG
					es = fopen(m_es_name.c_str(), "wb");
#endif
				}
			}
		}
		else if (m_type == "audio" && !m_bExit)
		{
			ELEM *pe = (ELEM *)m_queue->GetAudio();
			if (pe && pe->len > 0)
			{
				fwrite(pe->p, 1, pe->len, pAudio);
				m_queue->RetAudio(pe);
			}
			else
			{
				//cout << "[MUX.ch" << m_nChannel << "] queue size is 0" << endl;
				continue;
			}
			if (m_nRecSec > 0)
			{
				m_nAudioCount++;
				m_nTotalAudioCount++;
#if __DEBUG
				_d("current frame : %d\n", m_nFrameCount);
#endif
				int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
				if (m_nAudioCount >= nDstFrame)
				{
					int finalAudioCount = 0;
					finalAudioCount = m_nAudioCount;
					string final_filename = m_audio_name;

					m_nAudioCount = 0;

					cout << "[MUX.ch." << m_nChannel << "] " << m_audio_name << " save completed" << endl;
					m_file_idx++;

					fclose(pAudio);

					mux_files["name"] = final_filename;
					mux_files["frame"] = finalAudioCount;
					meta["files"].append(mux_files);
					CreateMetaJson(meta, group_name);

					// audio 이름 및 사이즈 전송
					info["filename"] = final_filename;
					size = getFilesize(final_filename.c_str());
					info["size"] = size;
					root["info"] = info;
					root["cmd"] = "get_file_info";

					Json::StreamWriterBuilder builder;
					builder["commentStyle"] = "None";
					builder["indentation"] = "";

					strbuf = Json::writeString(builder, root);

					TX((char *)strbuf.c_str(), strbuf.length());

					sstm.str("");
#if __IP_FILE_NAME
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << ip_string << "_" << m_file_idx << ".audio";
#else
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".audio";
#endif
					m_audio_name = sstm.str();
					pAudio = fopen(m_audio_name.c_str(), "wb");
				}
			}
		}
	}

	if (es)
	{
		fclose(es);
	}

	if (pAudio)
	{
		fclose(pAudio);
	}
	cout << "[MUX.ch" << m_nChannel << "] loop out" << endl;
	return true;
}

bool CMux::TX(char *buff, int size)
{
	if (m_sdSend < 0)
	{
		cout << "[CMUX] send socket not created" << endl;
		return false;
	}

	struct sockaddr_in sin;
	socklen_t sin_size = sizeof(sin);
	string ip = m_attr["udp_target_ip"].asString();
	int port = m_attr["udp_target_port"].asInt();

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(ip.c_str());
	sin.sin_port = htons(port);

	sendto(m_sdSend, buff, size, 0, (struct sockaddr *)&sin, sin_size);
	cout << "[CMUX] ip : " << inet_ntoa(sin.sin_addr) << " port : " << port << ", send message : " << buff << endl;
}

void CMux::Delete()
{

	SAFE_DELETE(m_pMuxer);

	//만약 m_queue를 삭제 안하면?
	m_queue->Clear();
	//SAFE_DELETE(m_queue);
}
