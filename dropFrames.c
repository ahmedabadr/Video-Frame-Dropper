// dropFrames.c
// Build
// gcc dropFrames.c -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames
// Use
// ./dropFrames [inputFiles] [outputFile] [frameType] [dropRate] [logFile]

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>

//static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;

static int open_input_file(const char *filename, AVFormatContext *ifmt_ctx)
{
    int ret;
    unsigned int i;

    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0)
    {
        printf("Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0)
    {
        printf("Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0)
            {
                printf("Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }

    //av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

static int open_output_file(const char *filename, AVFormatContext *ifmt_ctx)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;

    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    //avformat_alloc_output_context2(&ofmt_ctx, NULL, "mpegts", filename);
    if (!ofmt_ctx)
    {
        printf("Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++)
    {

        if (ifmt_ctx->streams[i]->discard == AVDISCARD_ALL)
        {
            printf("Stream[%d] discarded\n", i);
            continue;
        }
        printf("Stream[%d] not discarded\n", i);

        in_stream = ifmt_ctx->streams[i];
        dec_ctx = in_stream->codec;

        printf("Copy Codec Context ... \n");
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            out_stream->id = ofmt_ctx->nb_streams - 1;
            if (!out_stream)
            {
                printf("Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }
            enc_ctx = out_stream->codec;

            /* in this example, we choose transcoding to same codec */
            ret = avcodec_copy_context(enc_ctx, dec_ctx);
            if (ret < 0)
            {
                printf("Copying stream context failed\n");
                return ret;
            }
        }

        //else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
        //  av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
        //  return AVERROR_INVALIDDATA;
        //}
        //else {
        //  /* if this stream must be remuxed */
        //  ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec, ifmt_ctx->streams[i]->codec);
        //  if (ret < 0) {
        //    av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
        //    return ret;
        //  }
        //}

        printf("Add Flag Global Headers ... \n");
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    //printf("Dump Format ... \n");
    //av_dump_format(ofmt_ctx, 0, filename, 1);

    printf("avio_open ... \n");
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open output file '%s'", filename);
            return ret;
        }
    }

    printf("Write Headers ... \n");
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0)
    {
        printf("Error occurred when opening output file\n");
        //return ret;
    }

    printf("No Of Streams[%d] = %d\n", ofmt_ctx, ofmt_ctx->nb_streams);

    printf("Returning ... \n");

    return 0;
}

int main(int argc, char *argv[])
{
    av_log_set_level(AV_LOG_FATAL);
    // Initalizing these to NULL prevents segfaults!
    AVFormatContext *pFormatCtx = NULL;
    AVFormatContext *pFormatCtx2 = NULL;
    AVFormatContext *oFormatCtx = NULL;
    int i, videoStream, audioStream;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx = NULL;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGB = NULL;
    AVPacket packet;
    int numBytes;
    uint8_t *buffer = NULL;
    struct SwsContext *sws_ctx = NULL;

    if (argc < 5)
    {
        printf("./dropFrames [inputFiles] [outputFile] [frameType] [dropRate]\n");
        return -1;
    }

    char *inputFileName = argv[1];
    char *outputFileName = argv[2];
    char *frameType = argv[3];
    long frameLossRate = strtol(argv[4], NULL, 10);

    char *logFileName;
    if (argc >= 6)
        logFileName = argv[5];

    enum AVPictureType frameTypeToDrop;
    if ( strcmp(frameType, "I") == 0 )
        frameTypeToDrop = AV_PICTURE_TYPE_I;
    else if ( strcmp(frameType, "P") == 0 )
        frameTypeToDrop = AV_PICTURE_TYPE_P;
    else if ( strcmp(frameType, "B") == 0 )
        frameTypeToDrop = AV_PICTURE_TYPE_B;
    else
        frameTypeToDrop = AV_PICTURE_TYPE_NONE;


    printf("Input = %s, Output = %s, frameType = %s(%d), dropRate = %d, Log = %s\n", inputFileName, outputFileName, frameType, frameTypeToDrop, frameLossRate, logFileName);

    // Register all formats and codecs
    av_register_all();
    avformat_network_init();

    // Open video file
    AVDictionary *d = NULL;
    //av_dict_set_int(&d, "selected_reps", "3,4", 0);
    //av_dict_set(&d, "selected_reps", "0", 0);
    pFormatCtx = avformat_alloc_context();
    pFormatCtx->probesize = 5 * 1000000;            // bytes
    pFormatCtx->max_analyze_duration = 5 * 1000000; // microseconds
    //pFormatCtx->fps_probe_size = 3000;
    if (avformat_open_input(&pFormatCtx, inputFileName, NULL, &d) != 0) {
        printf("Cannot Open Input: %s\n", inputFileName);
        return -1; // Couldn't open file
    }

    int ii;
    for (ii = 0; ii < pFormatCtx->nb_streams; ii++)
        printf("StreamID[%d] = %d\n", ii, pFormatCtx->streams[ii]->id);

    printf("Show %d Streams \n", pFormatCtx->nb_streams);
    int streamIndex;
    for (streamIndex = 0; streamIndex < pFormatCtx->nb_streams; streamIndex++)
        printf("Discard[%d] = %d\n", streamIndex, pFormatCtx->streams[streamIndex]->discard);
    printf("Done Showing Streams \n");

    AVProgram *program;
    printf("Show %d Programs \n", pFormatCtx->nb_programs);
    for (ii = 0; ii < pFormatCtx->nb_programs; ii++)
    {
        if (pFormatCtx->programs[ii])
        {
            program = pFormatCtx->programs[ii];
            AVDictionaryEntry *name = av_dict_get(program->metadata, "service_name", NULL, 0);
            if (name)
                printf("Program[%d]: Name = %s\n", ii, name->value);
            else
                printf("Program[%d]: Name = %s\n", ii, "NULL");
        }
        else
        {
            printf("Program[%d]: NULL\n");
        }
    }
    printf("Done Showing Programs \n");

    // Retrieve stream information
    printf("Find Stream Info Start ... \n");
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        return -1; // Couldn't find stream information
    printf("Find Stream Info Done\n");

    // Dump information about file onto standard error
    //printf("Dump Stream Format Start ... \n");
    //av_dump_format(pFormatCtx, 0, argv[1], 0);
    //printf("Dump Stream Format Done\n");

    //usleep(2 * 1000000);

    // Find the first video stream
    videoStream = -1;
    audioStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
        } else if  (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = i;
        }
    }
    if (videoStream == -1) printf("No Video Stream\n");
    if (audioStream == -1) printf("No Audio Stream\n");
    
    open_output_file(outputFileName, pFormatCtx);
    
    // Open Log File
    FILE *f = fopen(logFileName, "w");
    if (argc == 5) {
        if (f == NULL)
        {
            printf("Error opening file!\n");
            exit(1);
        }
    }


    /*
    int jj;
    av_log(NULL, AV_LOG_WARNING, "pFormatCtx = %d\n", pFormatCtx);
    for (ii = 0; ii < pFormatCtx->nb_programs; ii++)
    {
        av_log(NULL, AV_LOG_WARNING, "Program[%d]: #Streams = %d\n", ii, pFormatCtx->programs[ii]->nb_stream_indexes);
        for (jj = 0; jj < pFormatCtx->programs[ii]->nb_stream_indexes; jj++)
            av_log(NULL, AV_LOG_WARNING, "Program[%d]: stream_index = %d\n", ii, jj, pFormatCtx->programs[ii]->stream_index[jj]);
    }

    av_log(NULL, AV_LOG_WARNING, "pFormatCtx = %d\n", pFormatCtx);
    for (ii = 0; ii < pFormatCtx->nb_streams; ii++)
    {
        av_log(NULL, AV_LOG_WARNING, "Stream[%d]->id = %d\n", ii, pFormatCtx->streams[ii]->id);
    }

    av_log(NULL, AV_LOG_WARNING, "ofmt_ctx = %d\n", ofmt_ctx);
    for (ii = 0; ii < ofmt_ctx->nb_streams; ii++)
    {
        av_log(NULL, AV_LOG_WARNING, "Stream[%d]->id = %d\n", ii, ofmt_ctx->streams[ii]->id);
    }
    */

    i = 0;
    int numOfPackets = 0;
    int numOfVideoPackets = 0, numOfAudioPackets = 0, numOfUnknownPackets = 0;
    int numOfIFrames = 0, numOfPFrames = 0, numOfBFrames = 0, numOfOtherFrames = 0;
    int numOfDroppedIFrames = 0, numOfDroppedPFrames = 0, numOfDroppedBFrames = 0, numOfDroppedOtherFrames = 0;
    
    int randomNumber = 0;
    int numOfDroppedPackets = 0;
    enum AVMediaType mediaType;
    int drop; // boolean

    AVFrame *frame = NULL;
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    int frameFinished;

    int dtsToDrop[100];
    for (i = 0; i < 100; i++)
        dtsToDrop[i] = -1;

    // ---- VIDEO DECODING ---- 
    // Find the decoder for the video stream
    codec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codec->codec_id);
    if (codec == NULL) {
        fprintf(stderr, "Unsupported Codec!\n");
        return -1; // Codec not found
    }
    // Copy context
    codecCtx = avcodec_alloc_context3(codec);
    if (avcodec_copy_context(codecCtx, pFormatCtx->streams[videoStream]->codec) != 0) {
        fprintf(stderr, "Couldn't copy codec context");
        return -1; // Error copying codec context
    }

    // Open codec
    if (avcodec_open2(codecCtx, codec, NULL) < 0)
        return -1; // Could not open codec

    // Allocate video frame
    frame = av_frame_alloc();
    // ---- VIDEO DECODING ---- 

    printf("Start reading frames ... \n");
    
    srand(time(NULL));


    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {

        drop = 0;
        mediaType = pFormatCtx->streams[packet.stream_index]->codec->codec_type;

        /*
        if ( (numOfPackets % 100) == 0 ) {
            printf("Packet[%d] (%d, a = %d, v = %d) => %d\n", numOfPackets, packet.stream_index, audioStream, videoStream, mediaType);
            printf("Packet: Duration = %d, PTS = %" PRId64 ", DTS = %" PRId64 "\n", packet.duration, packet.pts, packet.dts);
            printf("Random Number = %d\n", randomNumber);
        }
        */

        if (mediaType == AVMEDIA_TYPE_VIDEO) {
            //usleep( int( 1000000.0 / 30.0) );
            // ---- VIDEO DECODING ---- 
            avcodec_decode_video2(codecCtx, frame, &frameFinished, &packet);
            //printf("DTS = (%d)%" PRId64 " / (%d)%" PRId64 "\n", frame->pict_type, frame->pts, packet.flags & AV_PKT_FLAG_KEY, packet.pts);
            if (frameFinished) {
                ;
                //printf("Packet[%d] = %d vs %d\n", numOfVideoPackets, packet.flags & AV_PKT_FLAG_KEY, frame->pict_type);
            } else {
                printf("Incomplete Frame\n");
                //continue;
            }
            // ---- VIDEO DECODING ---- 
            numOfVideoPackets++;
            randomNumber = (rand() % 100);
            if (frame->pict_type == AV_PICTURE_TYPE_I) {
                numOfIFrames++;
                if ( (randomNumber < frameLossRate) && ( (frameTypeToDrop == AV_PICTURE_TYPE_I) || (frameTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
                    numOfDroppedIFrames++;
                    numOfDroppedPackets++;
                    drop = 1;
                }
                if (drop){
                    printf("\n-");
                    if (f) fprintf(f, "\n-");
                } else {
                    printf("\nI");
                    if (f) fprintf(f, "\nI");
                }
            } else if (frame->pict_type == AV_PICTURE_TYPE_P) {
                numOfPFrames++;
                if ( (randomNumber < frameLossRate) && ( (frameTypeToDrop == AV_PICTURE_TYPE_P) || (frameTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
                    numOfDroppedPFrames++;
                    numOfDroppedPackets++;
                    drop = 1;
                }
                if (drop) {
                    printf("-");
                    if (f) fprintf(f, "-");
                } else {
                    printf("P");
                    if (f) fprintf(f, "P");
                }
            } else if (frame->pict_type == AV_PICTURE_TYPE_B) {
                numOfBFrames++;
                if ( (randomNumber < frameLossRate) && ( (frameTypeToDrop == AV_PICTURE_TYPE_B) || (frameTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
                    numOfDroppedBFrames++;
                    numOfDroppedPackets++;
                    drop = 1;
                }
                if (drop) {
                    printf("-");
                    if (f) fprintf(f, "-");
                } else {
                    printf("B");
                    if (f) fprintf(f, "B");
                }
            } else {
                numOfOtherFrames++;
                if ( (randomNumber < frameLossRate) && (frameTypeToDrop == AV_PICTURE_TYPE_NONE) ) {
                    numOfDroppedOtherFrames++;
                    numOfDroppedPackets++;
                    drop = 1;
                }
                if (drop) {
                    printf("-");
                    if (f) fprintf(f, "-");
                }
                else {
                    printf("O");
                    if (f) fprintf(f, "O");
                }
            }
        } else if (mediaType == AVMEDIA_TYPE_AUDIO) {
            numOfAudioPackets++;
            drop = 0;
        } else {
            numOfUnknownPackets++;
            drop = 0;
        }

        if (!drop) {
            if (av_interleaved_write_frame(ofmt_ctx, &packet) < 0) {
                printf("Error writing frame to %s\n", outputFileName);
                return -1;
            }
        }

        numOfPackets++;
            
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    av_write_trailer(ofmt_ctx);

    printf("\nStatistics\n");
    if (f) fprintf(f, "\nStatistics\n");
    printf("-----------\n");
    if (f) fprintf(f, "-----------\n");
    printf("Num Of Frames = %d (Video = %d + Audio = %d)\n", numOfPackets, numOfVideoPackets, numOfAudioPackets);
    if (f) fprintf(f, "Num Of Frames = %d (Video = %d + Audio = %d)\n", numOfPackets, numOfVideoPackets, numOfAudioPackets);
    printf("Video Frame Types: \n");
    if (f) fprintf(f, "Video Frame Types: \n");
    printf(" => I-Frames: %d/%d = %3.2f %%\n", numOfDroppedIFrames, numOfIFrames, 100 * ((double)numOfDroppedIFrames)/((double)numOfIFrames));
    if (f) fprintf(f, " => I-Frames: %d/%d = %3.2f %%\n", numOfDroppedIFrames, numOfIFrames, 100 * ((double)numOfDroppedIFrames)/((double)numOfIFrames));
    printf(" => P-Frames: %d/%d = %3.2f %%\n", numOfDroppedPFrames, numOfPFrames, 100 * ((double)numOfDroppedPFrames)/((double)numOfPFrames));
    if (f) fprintf(f, " => P-Frames: %d/%d = %3.2f %%\n", numOfDroppedPFrames, numOfPFrames, 100 * ((double)numOfDroppedPFrames)/((double)numOfPFrames));
    printf(" => B-Frames: %d/%d = %3.2f %%\n", numOfDroppedBFrames, numOfBFrames, 100 * ((double)numOfDroppedBFrames)/((double)numOfBFrames));
    if (f) fprintf(f, " => B-Frames: %d/%d = %3.2f %%\n", numOfDroppedBFrames, numOfBFrames, 100 * ((double)numOfDroppedBFrames)/((double)numOfBFrames));
    printf(" => O-Frames: %d/%d = %3.2f %%\n", numOfDroppedOtherFrames, numOfOtherFrames, 100 * ((double)numOfDroppedOtherFrames)/((double)numOfOtherFrames));
    if (f) fprintf(f, " => O-Frames: %d/%d = %3.2f %%\n", numOfDroppedOtherFrames, numOfOtherFrames, 100 * ((double)numOfDroppedOtherFrames)/((double)numOfOtherFrames));
    printf("-------------------------------------\n");
    if (f) fprintf(f, "-------------------------------------\n");
    printf(" => Total   : %d/%d = %f %%\n", numOfDroppedPackets, numOfVideoPackets, 100 * ((double)numOfDroppedPackets)/((double)numOfVideoPackets));
    if (f) fprintf(f, " => Total   : %d/%d = %f %%\n", numOfDroppedPackets, numOfVideoPackets, 100 * ((double)numOfDroppedPackets)/((double)numOfVideoPackets));
    // Free the RGB image
    av_free(buffer);
    av_frame_free(&pFrameRGB);

    // Free the YUV frame
    av_frame_free(&pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    // Close the video file
    avformat_close_input(&pFormatCtx);

    return 0;
}