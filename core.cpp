#include "main.h"
#include "core.h"

extern char __BUILD_DATE;
extern char __BUILD_NUMBER;

CCore::CCore(void)
{
	m_bExit = false;
	pthread_mutex_init(&m_mutex_core, 0);
}

CCore::~CCore(void)
{
	m_bExit = true;

	_d("[CORE] Trying to exit thread\n");
	Terminate();
	//Terminate is Thread Join Completed
	_d("[CORE] Exited...\n");

	Delete();

	pthread_mutex_destroy(&m_mutex_core);
}

bool CCore::Create(int type)
{
	m_counter = 0;
	m_nChannel = 0;
	m_type = type;
	ifstream ifs("./setting.json", ifstream::binary);

	if (!ifs.is_open())
	{
		_d("\n ******************************************* \n setting.json file is not found\n Put in setting.json file in your directory\n ******************************************* \n\n");
	}
	else
	{
		//check done
		//ifs >> m_root;
		if (!m_reader.parse(ifs, m_root, true))
		{
			ifs.close();
			_d("[CORE] Failed to parse setting.json configuration\n%s\n", m_reader.getFormatedErrorMessages().c_str());
			_d("[CORE] Exit code\n");
			exit(1);
		}
		else
		{
			ifs.close();
			_d("version : %s\n", m_root["version"].asString().c_str());
			cout << "file_dst : " << m_root["file_dst"].asString() << endl;

			// file_dst 디렉토리 생성
			stringstream sstm;
			sstm << "mkdir -p " << m_root["file_dst"].asString();
			system(sstm.str().c_str());

			Json::Value attr;

			attr = m_root;
#if 0
			attr["channels"] = m_root["channels"];
			attr["file_dst"] = m_root["file_dst"];
			attr["make_folder_interval"] = m_root["make_folder_interval"];
			attr["rec_sec"] = m_root["rec_sec"];
			attr["codec"] = m_root["codec"];
			attr["udp_port"] = m_root["udp_port"];

			for (auto &value : m_root["channels"])
			{
				/*
				cout << value["ip"].asString() << endl;
				cout << value["port"].asInt() << endl;
				cout << value["rec_sec"].asInt() << endl;
				*/

				m_CRecv[m_nChannel] = new CRecv();
				m_CRecv[m_nChannel]->Create(m_root["channels"][m_nChannel], attr, m_nChannel);
				m_nChannel++;
			}
#endif
			m_comm = new CCommMgr();
			m_comm->Open(m_root["udp_muxer_port"][m_type].asInt(), attr);
		}
	}

	Start();
	return true;
}

void CCore::Run()
{
	while (!m_bExit)
	{
#if __DEFRECATED
		cout << "[CORE] Thread is alive" << endl;
#endif
		sleep(1);
		//cout << "[CORE] " << m_root["make_folder_interval"].asUInt() << ", counter : " << m_counter << endl;
		if (m_type == 0) // 상시녹화
		{
			if (m_comm->isRunning() == true)
			{
				m_counter++;
			}

			if (m_root["make_folder_interval"].asUInt() < m_root["rec_sec"].asUInt())
			{
				//not work
			}
			else
			{
				if (m_counter > m_root["make_folder_interval"].asUInt())
				{
					_d("[CORE] force Restart\n", m_counter);
					m_comm->Restart();
					m_counter = 0;
				}
			}
		}
	}
	Delete();
	_d("[CORE] Thread has been exited\n");
}

void CCore::Delete()
{
#if 0
	for (int i = 0; i < m_nChannel; i++)
	{
		SAFE_DELETE(m_CRecv[i]);
	}
#endif
	SAFE_DELETE(m_comm);
}
