#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <libgen.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <semaphore.h>
#include <sys/mman.h>

//C++ lib
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <string>

#include "json/json.h"

extern "C"
{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/random_seed.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
}


using namespace std;

#define __DEBUG 0
#define __DUMP 1

#include "misc.h"

#define PACKET_SIZE 4096

#define MAX_VIDEO_CHANNEL_COUNT 6
#define MAX_AUDIO_CHANNEL_COUNT 2

#if 0
typedef struct {
    int output; //> 0- ts, 1- mp4
    int mux_rate;

    video_cfg_s vid;
}mux_cfg_s;

typedef struct {
    int codec;      //> 0- H264, 1- HEVC
    int profile;
    int level;
    int quality;
    int resolution;
    int width;
    int height;
    int rc;
    int bitrate;
    double fps;
    int min_gop;
    int max_gop;
    int bframes;
    int refs;
    int ratio_x;
    int ratio_y;

    bool is_cabac;
    bool is_interlace;
    bool is_dummy1;
    bool is_dummy2;
}video_cfg_s;

typedef struct {
    TCHAR strIP[100];
    int nPort;
}channel_s;
typedef struct {
    int channel_count;
    channel_s vid_ch[2];    
    channel_s aud_ch[2];
    mux_cfg_s mux;
}channel_cfg_s;

typedef struct {
    TCHAR strIP[100];
    int nPort;
}stream_s;

typedef struct {
    channel_s ctr_channel;
    channel_s vid_channel[MAX_VIDEO_CHANNEL_COUNT];
    channel_s aud_channel[MAX_AUDIO_CHANNEL_COUNT]; 

    int nRecvPort;
}conversion_cfg_s;
#endif

#ifdef DEBUG
#undef DEBUG
#define _d(fmt, args...)     \
    {                        \
        printf(fmt, ##args); \
    }
#else
#undef DEBUG
#define _d(fmt, args...)     \
    {                        \
        printf(fmt, ##args); \
    }
//#define _d(fmt, args...)
#endif

#define MAX_PROFILES 5
#define MAX_AUDIO_TRACKS 2
#define STAT_VIDEO_LOST 0x1000
#define SAFE_DELETE(x) \
    {                  \
        if (x)         \
            delete x;  \
        x = NULL;      \
    }
#endif
