#ifndef FILEINPUT_H
#define FILEINPUT_H

#include <vector>
#include <string>

extern "C"{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
    #include "libavdevice/avdevice.h"
}
#ifdef ANDROID
#include <android/log.h>

#define  LOG_TAG    "Debug"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGE( format, ... ) printf( "%s::%s(%d) " format, __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__ )
#endif

class FileInput {

public:
    FileInput(){};
    ~FileInput(){};
    std::vector <double> open_file(const char filename[]);
    int createOutputFile(const char filename[]);
    int closeOutputFile();
    int writeData(std::vector<double> data);

private:
    int decode_packet(std::vector<double> &data, AVCodecContext *fileCodecContext,AVPacket *filePacket, AVFrame *fileFrame, SwrContext *swr);
    AVStream *out_stream;
    AVFormatContext *format_context = NULL;
    AVCodecContext *codec_context;
    size_t sample;
};


#endif //FILEINPUT_H
