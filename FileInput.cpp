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

    //LOGE("opening the input file (%s) and loading format (container) header", filename);

    if (avformat_open_input(&fileFormatContext, filename, nullptr, nullptr) != 0) {
        LOGE("ERROR could not open the file");
        return {};
    }
    // now we have access to some information about our file
    // since we read its header we can say what format (container) it's
    // and some other information related to the format itself.
    //LOGE("format %s, duration %ld us, bit_rate %ld", fileFormatContext->iformat->name, fileFormatContext->duration, fileFormatContext->bit_rate);
    //LOGE("finding stream info from format");

    if (avformat_find_stream_info(fileFormatContext, nullptr) < 0) {
        LOGE("ERROR could not get the stream info");
        return {};
    }
    // loop though all the streams and print its main information
    for (uint i = 0; i < fileFormatContext->nb_streams; i++)
    {
        AVCodecParameters *pLocalCodecParameters = nullptr;
        pLocalCodecParameters = fileFormatContext->streams[i]->codecpar;


        //LOGE("finding the proper decoder (CODEC)");

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
            //LOGE("\tCodec %s ID %d bit_rate %ld", pLocalCodec->name, pLocalCodec->id, fileCodecParameters->bit_rate);
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
    //downsampling to 16.0kHz and downmixing to mono
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

    //LOGE("File opened! Channels: %d", fileCodecContext->channels);

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
int FileInput::createOutputFile(const char filename[]){
    int ret = 0;

    avformat_alloc_output_context2(&format_context, nullptr, nullptr, filename);
    if (!format_context) {
        LOGE("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return AVERROR_UNKNOWN;
    }

    if (!(format_context->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_context->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("Could not open output file '%s'",filename);
            return ret;
        }
    }

    if (!(format_context->oformat = av_guess_format(NULL, filename, NULL))) {
        LOGE("Could not find output file format");
    }

    // Add the file pathname to the output context
    if (!(format_context -> url = av_strdup(filename))) {
        LOGE("Could not process file path name");
    }

    // Guess the encoder for the file
    AVCodecID codec_id = av_guess_codec(
            format_context -> oformat,
            NULL,
            filename,
            NULL,
            AVMEDIA_TYPE_AUDIO);

    // Find an encoder based on the codec
    AVCodec * output_codec;
    if (!(output_codec = avcodec_find_encoder(codec_id))) {
        LOGE("Could not open codec");
    }

    out_stream = avformat_new_stream(format_context, nullptr);
    if (!out_stream) {
        LOGE("Could not create new stream for output file\n");
        return -1;
    }
    codec_context = NULL;
    // Allocate an encoding context
    if (!(codec_context = avcodec_alloc_context3(output_codec))) {
        LOGE("Could not allocate an encoding context");
    }
    // Set the parameters of the stream
    codec_context -> channels = 1;
    codec_context -> channel_layout = av_get_default_channel_layout(1);
    codec_context -> sample_rate = 16000;
    codec_context -> sample_fmt = output_codec -> sample_fmts[0];
    codec_context -> bit_rate = 256000;

    // Set the sample rate of the container
    out_stream -> time_base.den = 16000;
    out_stream -> time_base.num = 1;
    sample = 0;

    // Add a global header if necessary
    if (format_context -> oformat -> flags & AVFMT_GLOBALHEADER)
        codec_context -> flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Open the encoder for the audio stream to use
    if ((avcodec_open2(codec_context, output_codec, NULL)) < 0) {
        LOGE("Could not open output codec");
    }
    // Make sure everything has been initialized correctly
    ret = avcodec_parameters_from_context(out_stream->codecpar, codec_context);
    if (ret < 0) {
        LOGE("Could not initialize stream parameters ");
    }
    ret = avformat_write_header(format_context, nullptr);
    if (ret < 0) {
        LOGE("Error occurred when opening output file\n");
        return ret;
    }
    return 0;
}

int FileInput::writeData(std::vector<double> data) {
    int ret = 0;
    int bytes_per_sample = 2;

    AVPacket *capturePacket = av_packet_alloc();

    if (!capturePacket)
    {
        LOGE("failed to allocated memory for AVPacket");
        return -1;
    }

    AVFrame *frame = av_frame_alloc();

    if (!frame)
    {
        av_packet_unref(capturePacket);
        LOGE("failed to allocated memory for AVFrame");
        return -1;
    }

    size_t dst_nb_samples = data.size();
    int16_t *dst_samples_data = (int16_t *)malloc(dst_nb_samples);
    if (codec_context -> frame_size <= 0) {
        codec_context -> frame_size = 8000;
    }

    frame -> pts = sample;

    size_t dst_samples_size = dst_nb_samples * bytes_per_sample;
    frame -> nb_samples     = codec_context -> frame_size;
    frame -> channel_layout = codec_context -> channel_layout;
    frame -> format         = codec_context -> sample_fmt;
    frame -> sample_rate    = codec_context -> sample_rate;


    for(size_t i = 0;i<dst_nb_samples;i++){
        dst_samples_data[i] = (int16_t)(data[i]*32767);
    }
    // Allocate the samples in the frame
    if (av_frame_get_buffer(frame, 0) < 0) {
        LOGE("Could not allocate output frame samples");
        av_frame_free(&frame);
        av_packet_unref(capturePacket);
        return -1;
    }
    // Construct a packet for the encoded frame
    av_init_packet(capturePacket);
    capturePacket->data = NULL;
    capturePacket->size = 0;

    avcodec_fill_audio_frame(frame, codec_context->channels, codec_context->sample_fmt,
                             (const uint8_t *)dst_samples_data, dst_samples_size, 0);

    // Send a frame to the encoder to encode
    if ((ret = avcodec_send_frame(codec_context, frame)) < 0) {
        LOGE("Could not send packet for encoding");
        return -1;
    }

    // Receive the encoded frame from the encoder
    while ((ret = avcodec_receive_packet(codec_context, capturePacket)) == 0) {
        // Write the encoded frame to the file
        if ((ret = av_interleaved_write_frame(format_context, capturePacket)) < 0) {
            LOGE("Could not write frame");
        }
    }
    sample += dst_nb_samples;
    free(dst_samples_data);
    av_frame_free(&frame);
    av_packet_unref(capturePacket);
    return 0;
}

int FileInput::closeOutputFile(){
    av_write_trailer(format_context);
    avcodec_close(codec_context);
    avcodec_free_context(&codec_context);
    avio_close(format_context->pb);
    avformat_free_context(format_context);
    return 0;
}