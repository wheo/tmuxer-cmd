#include "main.h"
#include "core.h"

#include <sys/time.h>
#include <sys/resource.h>

bool exit_flag_main = false;

pthread_mutex_t sleepMutex;
pthread_cond_t sleepCond;

void sigfunc(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        exit_flag_main = true;
    }
    pthread_cond_signal(&sleepCond);
}

int main(int argc, char *argv[])
{
    struct rlimit rlim;
    signal(SIGINT, sigfunc);
    signal(SIGTERM, sigfunc);
    signal(SIGHUP, sigfunc);

#if 0
    getrlimit(RLIMIT_STACK, &rlim);
    rlim.rlim_cur = (4096*1024*10);
    rlim.rlim_max = (4096*1024*10);
    setrlimit(RLIMIT_STACK, &rlim);
#endif
    if (argc < 2)
    {
        cout << "[MAIN] ./tmuxer 0 or 1 ( 0 is 24 hour Rec, 1 is Event Rec)" << endl;
        exit(0);
    }

    if (argv[1][0] != '0' && argv[1][0] != '1')
    {
        cout << "[MAIN] This program's parametar is only 0 or 1" << endl;
        exit(0);
    }

    //setting.json check
    ifstream ifs("./setting.json");

    if (!ifs.is_open())
    {
        _d("\n ******************************************* \n setting.json file is not found\n Put in setting.json file in your directory\n ******************************************* \n\n");
        exit(0);
    }
    else
    {
        //check done
        ifs.close();
    }

    //void av_register_all()’ is deprecated [-Wdeprecated-declarations]
    //av_register_all();
    //avfilter_register_all();
    _d("[MAIN] Started...\n");

    av_log_set_level(AV_LOG_DEBUG);

    int type = atoi(argv[1]);
    CCore *core = new CCore();
    core->Create(type);

    while (!exit_flag_main)
    {
        pthread_mutex_lock(&sleepMutex);
        pthread_cond_wait(&sleepCond, &sleepMutex);
        pthread_mutex_unlock(&sleepMutex);
    }
    _d("[MAIN] Exiting...\n");

    SAFE_DELETE(core);

    _d("[MAIN] Exited\n");
    return 0;
}
