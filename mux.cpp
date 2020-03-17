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
	//Delete();
	Terminate();
	_d("[MUX.ch%d] exited...\n", m_nChannel);

	pthread_mutex_destroy(&m_mutex_mux);
}

bool CMux::Create(int nProgramType, Json::Value info, Json::Value attr, int nChannel)
{
	m_nProgramType = nProgramType;
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

	m_file_cnt = 1;

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
	Muxing();
}

void CMux::SetConfig(mux_cfg_s *mux_cfg, int codec_type, int recv_type)
{
	// 1(NTSC), 2(PAL), 3(PANORAMA), 4(FOCUS)
	//cout << "[SetConfig] codec_type (" << codec_type << ") recv_type(" << recv_type << ")" << endl;
	if (recv_type == 1)
	{
		//NTSC
		mux_cfg->output = 1;
		mux_cfg->vid.codec = codec_type;
		mux_cfg->vid.num = 1001;
		mux_cfg->vid.den = 60000;
		mux_cfg->vid.fps = 59.94;
		mux_cfg->vid.max_gop = 30;
		mux_cfg->vid.min_gop = 30;
		mux_cfg->vid.width = 720;
		mux_cfg->vid.height = 288;
	}
	else if (recv_type == 2)
	{
		//PAL
		mux_cfg->output = 1;
		mux_cfg->vid.codec = codec_type;
		mux_cfg->vid.num = 1000;
		mux_cfg->vid.den = 50000;
		mux_cfg->vid.fps = 50;
		mux_cfg->vid.max_gop = 30;
		mux_cfg->vid.min_gop = 30;
		mux_cfg->vid.width = 720;
		mux_cfg->vid.height = 240;
	}
	else if (recv_type == 3 && m_nChannel == 4)
	{
		//PANORAMA
		mux_cfg->output = 1;
		mux_cfg->vid.codec = codec_type;
		mux_cfg->vid.fps = 4;
		mux_cfg->vid.num = 1;
		mux_cfg->vid.den = 4;
		mux_cfg->vid.max_gop = 30;
		mux_cfg->vid.min_gop = 30;
		mux_cfg->vid.width = 1024;
		mux_cfg->vid.height = 1024;
	}
	else if (recv_type == 3 && m_nChannel == 5)
	{
		mux_cfg->output = 1;
		mux_cfg->vid.codec = codec_type;
		mux_cfg->vid.fps = 4;
		mux_cfg->vid.num = 1;
		mux_cfg->vid.den = 4;
		mux_cfg->vid.max_gop = 1;
		mux_cfg->vid.min_gop = 1;
		mux_cfg->vid.width = 1024;
		mux_cfg->vid.height = 1280;
	}
}

bool CMux::Muxing()
{
	stringstream sstm;
	string sub_dir_name = m_attr["folder_name"].asString();
	string group_name;
	string file_dst = m_attr["file_dst"].asString();
	string ip_string = m_info["ip"].asString();

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
	//cout << "[MUX.ch" << m_nChannel << "] expected output file name : " << m_filename << endl;
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

	mux_cfg_s mux_cfg;
	memset(&mux_cfg, 0x00, sizeof(mux_cfg_s));

	Json::Value meta;
	Json::Value mux_files;

#if 0
	mux_cfg.output = 1;
	mux_cfg.vid.codec = m_attr["codec"].asInt(); // 0 : H264, 1 : HEVC

	mux_cfg.vid.num = 1001;
	mux_cfg.vid.den = 60000;
	mux_cfg.vid.fps = 59.94;
	mux_cfg.vid.width = 720;
	mux_cfg.vid.height = 288;
	mux_cfg.vid.max_gop = 30;

	meta["filename"] = m_filename;
	meta["channel"] = m_nChannel;
	meta["codec"] = mux_cfg.vid.codec;
	meta["type"] = m_type;
	CreateMetaJson(meta, group_name);
#endif

	string channel;
	FILE *es = NULL;
	FILE *pAudio = NULL;

#if __DEBUG
	es = fopen(m_es_name.c_str(), "wb");
#endif
	m_nRecSec = m_attr["rec_sec"].asInt();

	Json::Value root;
	Json::Value info;
	int size;
	int ret = 0;
	string strbuf;
	int64_t file_first_pts = -1;
	int64_t file_last_pts = -1;
	bool is_first_pkt = false;
	int recv_type = 1;
	int codec_type = 0;

	while (!m_bExit)
	{
		if (m_type == "video")
		{
			if (m_bExit == true)
			{
				_d("[MUX.ch.%d] buffercnt : %d\n", m_nChannel, m_queue->GetVideoBufferCount());
				if (m_queue->GetVideoBufferCount() < 1)
				{
					break;
				}
			}
			AVPacket pkt;
			av_init_packet(&pkt);
			if (m_queue->Get(&pkt, &codec_type, &recv_type) > 0)
			{
				SetConfig(&mux_cfg, codec_type, recv_type);
				if (is_first_pkt == false)
				{
					m_pMuxer->CreateOutput(m_filename.c_str(), &mux_cfg);
					file_first_pts = pkt.pts;
					is_first_pkt = true;
				}
				m_current_pts = pkt.pts;
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
					m_pMuxer->put_data(&pkt);
#if __DEBUG
					fwrite(pPkt->data, 1, pPkt->size, es);
#endif
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
#if 1
				//_d("[MUX.ch.%d] current frame : %d (%lld), buffercnt : %d\n", m_nChannel, m_nFrameCount, m_current_pts, m_queue->GetVideoBufferCount());
#endif
				//int nDstFrame = (m_nRecSec + 1) * mux_cfg.vid.fps;
				//if (m_nFrameCount >= nDstFrame && m_queue->IsNextKeyFrame())
				if (m_current_pts - file_last_pts > (int64_t)(m_nRecSec * AV_TIME_BASE) + 300000) // 300000은 설정된 초를 약간 넘기기위한 보정값
				{
					file_last_pts = m_current_pts;
					m_file_cnt++;
					int finalFrameCount = 0;
					finalFrameCount = m_nFrameCount;
					string final_filename = m_filename;
					m_nFrameCount = 0;

					cout << "[MUX.ch." << m_nChannel << "] " << m_filename << " mux completed, totalframe(" << finalFrameCount << "), (" << m_current_pts << "), (" << m_nRecSec << "), (" << mux_cfg.vid.fps << ")" << endl;
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

					is_first_pkt = false;
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
					mux_files["first_pts"] = file_first_pts;
					mux_files["last_pts"] = file_last_pts;
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
				if (is_first_pkt == false)
				{
					pAudio = fopen(m_audio_name.c_str(), "wb");
					file_first_pts = m_audio_pts;
					is_first_pkt = true;
				}
				memcpy(&m_audio_pts, pe->p, 8);
				//cout << "[MUX.ch" << m_nChannel << "] audio pts : " << m_audio_pts << endl;
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
				int nDstFrame = (m_nRecSec + 1) * AUDIO_FRAME_LEN;
				if (m_nAudioCount >= nDstFrame)
				{
					int finalAudioCount = 0;
					finalAudioCount = m_nAudioCount;
					string final_filename = m_audio_name;
					file_last_pts = m_audio_pts;

					m_nAudioCount = 0;

					cout << "[MUX.ch." << m_nChannel << "] " << m_audio_name << " save completed" << endl;
					m_file_idx++;

					fclose(pAudio);

					mux_files["name"] = final_filename;
					mux_files["frame"] = finalAudioCount;
					mux_files["first_pts"] = file_first_pts;
					mux_files["last_pts"] = file_last_pts;
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
					is_first_pkt = false;
					m_audio_name = sstm.str();
					pAudio = fopen(m_audio_name.c_str(), "wb");
				}
			}
		}
	}

	if (m_type == "video")
	{
		if (m_nFrameCount > 0)
		{
			mux_files["name"] = m_filename;
			mux_files["frame"] = m_nFrameCount;
			mux_files["first_pts"] = file_first_pts;
			mux_files["last_pts"] = m_current_pts;
			meta["files"].append(mux_files);
			CreateMetaJson(meta, group_name);
			SAFE_DELETE(m_pMuxer);
		}
	}
	else if (m_type == "audio")
	{
		if (m_nAudioCount > 0)
		{
			mux_files["name"] = m_audio_name;
			mux_files["frame"] = m_nAudioCount;
			mux_files["first_pts"] = file_first_pts;
			mux_files["last_pts"] = m_audio_pts;
			meta["files"].append(mux_files);
			CreateMetaJson(meta, group_name);
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
	//m_queue->Clear();
	//SAFE_DELETE(m_queue);
}
