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
	SetSocket();

	if (m_type == "video")
	{
		m_pMuxer = new CTSMuxer();
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

	int bit_state = m_attr["bit_state"].asInt();
	int result = bit_state & (1 << m_nChannel);

	string sub_dir_name;
	string group_name;

	if (m_attr["folder_name"].asString().size() > 0)
	{
		sub_dir_name = m_attr["folder_name"].asString();
	}
	else
	{
		sub_dir_name = get_current_time_and_date();
	}
	if (m_attr["bit_state"].asString().size() > 0)
	{
		cout << "channel : " << m_nChannel << ", bit state is " << bit_state << ", result : " << result << endl;
		if (result < 1)
		{
			cout << "channel " << m_nChannel << " is not use anymore" << endl;
			return false;
		}
	}
	stringstream sstm;
	sstm << "mkdir -p " << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel;
	system(sstm.str().c_str());
	// clear method is not working then .str("") correct
	sstm.str("");

	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/"
		 << "meta.json";
	group_name = sstm.str();
	sstm.str("");
#if __IP_FILE_NAME
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
#else
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".mp4";
#endif
	m_filename = sstm.str();
	cout << "[MUX.ch." << m_nChannel << "] expected output file name : " << m_filename << endl;
	sstm.str("");

#if __IP_FILE_NAME
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".es";
#else
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".es";
#endif
	m_es_name = sstm.str();
	sstm.str("");

	//sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".audio";
	sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".audio";
	m_audio_name = sstm.str();

	mux_cfg.output = 1;
	mux_cfg.vid.codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC
	//mux_cfg.vid.bitrate = 6000000;
	//mux_cfg.vid.fps = m_info["fps"].asDouble();
	double num = m_info["num"].asDouble();
	double den = m_info["den"].asDouble();
	mux_cfg.vid.fps = den / num;
	mux_cfg.vid.width = m_info["width"].asInt();
	mux_cfg.vid.height = m_info["height"].asInt();

	Json::Value meta;
	Json::Value mux_files;

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

	FILE *es = fopen(m_es_name.c_str(), "wb");

	FILE *pAudio = fopen(m_audio_name.c_str(), "wb");

	m_nRecSec = m_attr["rec_sec"].asInt();

	Json::Value root;
	Json::Value info;
	int size;
	string strbuf;

	while (!m_bExit)
	{
		if (m_type == "video" && !m_bExit)
		{
			AVPacket pPkt;
			if (m_queue->Get(&pPkt) > 0)
			{
				//es write
				fwrite(pPkt.data, 1, pPkt.size, es);

				m_pMuxer->put_data(&pPkt);
				m_queue->Ret(&pPkt);
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
#if __DEBUG
				_d("current frame : %d\n", m_nFrameCount);
#endif
				int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
				if (m_nFrameCount >= nDstFrame)
				{
					int finalFrameCount = 0;
					finalFrameCount = m_nFrameCount;
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
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".mp4";
#else
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".mp4";
#endif
					m_filename = sstm.str();
					m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);

					fclose(es);
					mux_files["name"] = m_es_name;
					mux_files["frame"] = finalFrameCount;
					meta["files"].append(mux_files);
					CreateMetaJson(meta, group_name);

					sstm.str("");
#if __IP_FILE_NAME
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".es";
#else
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".es";
#endif
					m_es_name = sstm.str();
					es = fopen(m_es_name.c_str(), "wb");
				}
			}
		}
		else if (m_type == "audio" && !m_bExit)
		{
			ELEM *pe = (ELEM *)m_queue->GetAudio();
			if (pe)
			{
				fwrite(pe->p, 1, pe->len, pAudio);
				m_queue->RetAudio(pe);
			}
			else
			{
				//cout << "[CMUX.ch" << m_nChannel << "] queue size is 0" << endl;
				continue;
			}
			if (m_nRecSec > 0)
			{
				m_nAudioCount++;
#if __DEBUG
				_d("current frame : %d\n", m_nFrameCount);
#endif
				int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
				if (m_nAudioCount >= nDstFrame)
				{
					int finalAudioCount = 0;
					finalAudioCount = m_nAudioCount;

					m_nAudioCount = 0;

					cout << "[MUX.ch." << m_nChannel << "] " << m_audio_name << " save completed" << endl;
					m_file_idx++;

					fclose(pAudio);
					mux_files["name"] = m_audio_name;
					mux_files["frame"] = finalAudioCount;
					meta["files"].append(mux_files);
					CreateMetaJson(meta, group_name);

					// audio 이름 및 사이즈 전송
					info["filename"] = m_audio_name;
					size = getFilesize(m_audio_name.c_str());
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
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_info["ip"].asString() << "_" << m_file_idx << ".audio";
#else
					sstm << m_attr["file_dst"].asString() << "/" << sub_dir_name << "/" << m_nChannel << "/" << m_file_idx << ".audio";
#endif
					m_audio_name = sstm.str();
					pAudio = fopen(m_audio_name.c_str(), "wb");
				}
			}
		}
	}

	fclose(es);

	// 포인터 점검해야함
	fclose(pAudio);
	//cout << "[MUX.ch" << m_nChannel << "] loop out" << endl;
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

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(m_attr["udp_target_ip"].asString().c_str());
	sin.sin_port = htons(m_attr["udp_target_port"].asInt());

	sendto(m_sdSend, buff, size, 0, (struct sockaddr *)&sin, sin_size);
	cout << "[CMUX] ip : " << inet_ntoa(sin.sin_addr) << " port : " << m_attr["udp_target_port"].asInt() << ", send message : " << buff << endl;
}

void CMux::Delete()
{
	SAFE_DELETE(m_pMuxer);
	//만약 m_queue를 삭제 안하면?
	m_queue->Clear();
	//SAFE_DELETE(m_queue);
}
