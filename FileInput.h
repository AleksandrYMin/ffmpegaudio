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

#define LOGE( format, ... ) printf( "%s::%s(%d) " format, __FILE__, __FUNCTION__,  __LINE__, __VA_ARGS__ )

class FileInput {

public:
    FileInput(){};
    ~FileInput(){};
    std::vector <double> open_file(const char filename[]);

private:
    int decode_packet(std::vector<double> &data, AVCodecContext *fileCodecContext,AVPacket *filePacket, AVFrame *fileFrame, SwrContext *swr);
};


#endif //FILEINPUT_H
