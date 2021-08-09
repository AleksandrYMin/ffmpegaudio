#include "FileInput.h"

std::vector <double> FileInput::open_file(const char filename[]){

    AVFormatContext *fileFormatContext = avformat_alloc_context();
    if (!fileFormatContext) {
        LOGE("ERROR could not allocate memory for Format Context");
        return {};
    }
    // the component that knows how to enCOde and DECode the stream
    // it's the codec (audio or video)
    // http://ffmpeg.org/doxygen/trunk/structAVCodec.html
    AVCodec *fileCodec = nullptr;
    // this component describes the properties of a codec used by the stream i
    // https://ffmpeg.org/doxygen/trunk/structAVCodecParameters.html
    AVCodecParameters *fileCodecParameters = nullptr;

    // https://ffmpeg.org/doxygen/trunk/structAVFrame.html
    AVFrame *fileFrame = av_frame_alloc();
    if (!fileFrame)
    {
        LOGE("failed to allocated memory for AVFrame");
        return {};
    }

    // https://ffmpeg.org/doxygen/trunk/structAVPacket.html
    AVPacket *filePacket = av_packet_alloc();
    if (!filePacket)
    {
        LOGE("failed to allocated memory for AVPacket");
        return {};
    }
    int audio_stream_index = -1;

    LOGE("opening the input file (%s) and loading format (container) header", filename);

    if (avformat_open_input(&fileFormatContext, filename, nullptr, nullptr) != 0) {
        LOGE("ERROR could not open the file");
        return {};
    }
    // now we have access to some information about our file
    // since we read its header we can say what format (container) it's
    // and some other information related to the format itself.
    LOGE("format %s, duration %ld us, bit_rate %ld", fileFormatContext->iformat->name, fileFormatContext->duration, fileFormatContext->bit_rate);
    LOGE("finding stream info from format");

    if (avformat_find_stream_info(fileFormatContext, nullptr) < 0) {
        LOGE("ERROR could not get the stream info");
        return {};
    }
    // loop though all the streams and print its main information
    for (uint i = 0; i < fileFormatContext->nb_streams; i++)
    {
        AVCodecParameters *pLocalCodecParameters = nullptr;
        pLocalCodecParameters = fileFormatContext->streams[i]->codecpar;


        LOGE("finding the proper decoder (CODEC)");

        AVCodec *pLocalCodec = nullptr;

        // finds the registered decoder for a codec ID
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga19a0ca553277f019dd5b0fec6e1f9dca
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);

        if (pLocalCodec == nullptr) {
            LOGE("ERROR unsupported codec!");
            return {};
        }

        // when the stream is a audio we store its index, codec parameters and codec
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (audio_stream_index == -1) {
                audio_stream_index = i;
                fileCodec = pLocalCodec;
                fileCodecParameters = pLocalCodecParameters;
            }
            // print its name, id and bitrate
            LOGE("\tCodec %s ID %d bit_rate %ld", pLocalCodec->name, pLocalCodec->id, fileCodecParameters->bit_rate);
            break;
        }
    }
    // https://ffmpeg.org/doxygen/trunk/structAVCodecContext.html
    AVCodecContext *fileCodecContext = avcodec_alloc_context3(fileCodec);
    if (!fileCodecContext)
    {
        LOGE("failed to allocated memory for AVCodecContext");
        return {};
    }
    // Fill the codec context based on the values from the supplied codec parameters
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#gac7b282f51540ca7a99416a3ba6ee0d16
    if (avcodec_parameters_to_context(fileCodecContext, fileCodecParameters) < 0)
    {
        return {};
    }
    // Initialize the AVCodecContext to use the given AVCodec.
    // https://ffmpeg.org/doxygen/trunk/group__lavc__core.html#ga11f785a188d7d9df71621001465b0f1d
    if (avcodec_open2(fileCodecContext, fileCodec, nullptr) < 0)
    {
        LOGE("failed to open codec through avcodec_open2");
        return {};
    }

    //setup conversion from sample format to interleaved signed 16 - bit integer,
    //downsampling from 48kHz to 16.0kHz and downmixing to mono
    SwrContext *swr = swr_alloc();
    av_opt_set_int(swr, "in_channel_count", fileCodecContext->channels, 0);
    av_opt_set_int(swr, "out_channel_count", 1, 0);
    av_opt_set_int(swr, "in_channel_layout", fileCodecContext->channel_layout, 0);
    av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(swr, "in_sample_rate", fileCodecContext->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", 16000, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", fileCodecContext->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    swr_init(swr);

    if (!swr_is_initialized(swr)) {
        LOGE("Resampler has not been properly initialized\n");
        return {};
    }

    LOGE("File opened! Channels: %d", fileCodecContext->channels);

    std::vector <double> data;

    // fill the Packet with data from the Stream
    // https://ffmpeg.org/doxygen/trunk/group__lavf__decoding.html#ga4fdb3084415a82e3810de6ee60e46a61
    while (av_read_frame(fileFormatContext, filePacket) >= 0)
    {
        // if it's the audio stream
        if (filePacket->stream_index == audio_stream_index) {
            int response = decode_packet(data, fileCodecContext,filePacket, fileFrame, swr);
            if (response < 0){
                break;
            }
        }
        // https://ffmpeg.org/doxygen/trunk/group__lavc__packet.html#ga63d5a489b419bd5d45cfd09091cbcbc2
        av_packet_unref(filePacket);
    }
    //free resources
    avformat_close_input(&fileFormatContext);
    avformat_free_context(fileFormatContext);
    avcodec_free_context(&fileCodecContext);
    av_frame_free(&fileFrame);
    av_packet_free(&filePacket);
    swr_free(&swr);

    return data;
}

int FileInput::decode_packet(std::vector<double> &data, AVCodecContext *fileCodecContext,AVPacket *filePacket, AVFrame *fileFrame, SwrContext *swr)
{
    // Supply raw packet data as input to a decoder
    // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga58bc4bf1e0ac59e27362597e467efff3
    int response = avcodec_send_packet(fileCodecContext, filePacket);

    if (response < 0) {
        LOGE("Error while sending a packet to the decoder");
        return 0;
    }

    while (response >= 0)
    {
        // Return decoded output data (into a frame) from a decoder
        // https://ffmpeg.org/doxygen/trunk/group__lavc__decoding.html#ga11e6542c4e66d3028668788a1a74217c
        response = avcodec_receive_frame(fileCodecContext, fileFrame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            LOGE("Error while receiving a frame from the decoder");
            return response;
        }

        // resample frames
        int16_t* buffer;
        av_samples_alloc((uint8_t**)&buffer, nullptr, 1, fileFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
        //resample to 16kHz, 16 bits
        int frame_count = swr_convert(swr, (uint8_t**)&buffer, fileFrame->nb_samples, (const uint8_t**)fileFrame->data, fileFrame->nb_samples);


        for (int i = 0; i < frame_count; i++) {
            data.push_back((double)buffer[i]/32767);//16 bits to double{-1;1}
        }

        av_free(buffer);
    }

    return 0;
}
