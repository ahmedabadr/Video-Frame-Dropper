// dropFrames.c
// Build
// gcc dropFrames.c -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames
// gcc dropFrames.c -L../../Work/sqm-ffmpeg/bld/lib -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames -Wl,-rpath=../../Work/sqm-ffmpeg/bld/lib
// Use
// ./dropFrames [inputFiles] [outputFile] [frameType] [dropRate] [logFile]

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>

#define PACKET_BUFFER_SIZE 600

//static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;

struct Arguments {
    char *inputFileName;
    char *outputFileName;
    char *frameType;
    enum AVPictureType frameTypeEnum;
    int frameLossRate;
    char *logFileName;
};

struct PacketStatistics {
    int numOfPackets;
    int numOfVideoPackets;
    int numOfAudioPackets;
    int numOfUnknownPackets;
    int numOfIFrames;
    int numOfPFrames;
    int numOfBFrames;
    int numOfOtherFrames;
    int numOfDroppedIFrames;
    int numOfDroppedPFrames;
    int numOfDroppedBFrames;
    int numOfDroppedOtherFrames;
    int numOfDroppedPackets;
};

void print_statistics(struct PacketStatistics ps, FILE *f) {
    printf("\nStatistics\n");
    if (f) fprintf(f, "\nStatistics\n");
    printf("-----------\n");
    if (f) fprintf(f, "-----------\n");
    printf("Num Of Frames = %d (Video = %d + Audio = %d)\n", ps.numOfPackets, ps.numOfVideoPackets, ps.numOfAudioPackets);
    if (f) fprintf(f, "Num Of Frames = %d (Video = %d + Audio = %d)\n", ps.numOfPackets, ps.numOfVideoPackets, ps.numOfAudioPackets);
    printf("Video Frame Types: \n");
    if (f) fprintf(f, "Video Frame Types: \n");
    printf(" => I-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedIFrames, ps.numOfIFrames, 100 * ((double)ps.numOfDroppedIFrames)/((double)ps.numOfIFrames));
    if (f) fprintf(f, " => I-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedIFrames, ps.numOfIFrames, 100 * ((double)ps.numOfDroppedIFrames)/((double)ps.numOfIFrames));
    printf(" => P-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedPFrames, ps.numOfPFrames, 100 * ((double)ps.numOfDroppedPFrames)/((double)ps.numOfPFrames));
    if (f) fprintf(f, " => P-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedPFrames, ps.numOfPFrames, 100 * ((double)ps.numOfDroppedPFrames)/((double)ps.numOfPFrames));
    printf(" => B-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedBFrames, ps.numOfBFrames, 100 * ((double)ps.numOfDroppedBFrames)/((double)ps.numOfBFrames));
    if (f) fprintf(f, " => B-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedBFrames, ps.numOfBFrames, 100 * ((double)ps.numOfDroppedBFrames)/((double)ps.numOfBFrames));
    printf(" => O-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedOtherFrames, ps.numOfOtherFrames, 100 * ((double)ps.numOfDroppedOtherFrames)/((double)ps.numOfOtherFrames));
    if (f) fprintf(f, " => O-Frames: %d/%d = %3.2f %%\n", ps.numOfDroppedOtherFrames, ps.numOfOtherFrames, 100 * ((double)ps.numOfDroppedOtherFrames)/((double)ps.numOfOtherFrames));
    printf("-------------------------------------\n");
    if (f) fprintf(f, "-------------------------------------\n");
    printf(" => Total   : %d/%d = %f %%\n", ps.numOfDroppedPackets, ps.numOfVideoPackets, 100 * ((double)ps.numOfDroppedPackets)/((double)ps.numOfVideoPackets));
    if (f) fprintf(f, " => Total   : %d/%d = %f %%\n", ps.numOfDroppedPackets, ps.numOfVideoPackets, 100 * ((double)ps.numOfDroppedPackets)/((double)ps.numOfVideoPackets));
}

int parse_arguments(int argc, char *argv[], struct Arguments *a) {
    if (argc <= 5) {
        printf("USAGE: ./dropFrames inputFile outputFile frameType dropRate [logFile]\n");
        return -1;
    }
    a->inputFileName = argv[1];
    printf("input: %s\n", a->inputFileName);
    a->outputFileName = argv[2];
    printf("output: %s\n", a->outputFileName);
    
    a->frameType = argv[3];
    if ( strcmp(a->frameType, "I") == 0 )
        a->frameTypeEnum = AV_PICTURE_TYPE_I;
    else if ( strcmp(a->frameType, "P") == 0 )
        a->frameTypeEnum = AV_PICTURE_TYPE_P;
    else if ( strcmp(a->frameType, "B") == 0 )
        a->frameTypeEnum = AV_PICTURE_TYPE_B;
    else
        a->frameTypeEnum = AV_PICTURE_TYPE_NONE;
    printf("Frame Type: %d => %s\n", a->frameTypeEnum, a->frameType);

    a->frameLossRate = (int) strtol(argv[4], NULL, 10);
    printf("FLR: %d\%\n", a->frameLossRate);
    if (argc >= 6) {
        a->logFileName = argv[5];
        printf("log: %s\n", a->logFileName);
    }
    return 0;
}

int find_packet(AVPacket pktBuffer[PACKET_BUFFER_SIZE], int frmTypeBuffer[PACKET_BUFFER_SIZE], int pktBufferSize, AVFrame* frame) {
    int matchingIndex = -1;
    int i;
    for (i = 0; i < pktBufferSize; i++) {
        if (pktBuffer[i].pts == frame->pkt_pts) {
            matchingIndex = i;
            frmTypeBuffer[i] = frame->pict_type;
            break;
        }
    }
    return matchingIndex;
}

int remove_packet(AVPacket pktBuffer[PACKET_BUFFER_SIZE], int frmTypeBuffer[PACKET_BUFFER_SIZE], int pktBufferSize, int pktIndex) {
    if (pktIndex >= pktBufferSize)
        return -1;
    
    int i;
    for (i = pktIndex+1; i < pktBufferSize; i++) {
        frmTypeBuffer[i-1] = frmTypeBuffer[i];
        pktBuffer[i-1] = pktBuffer[i];
    }
    pktBufferSize--;
    return pktBufferSize;
}

int should_drop(int currFrmType, int frmTypeToDrop, int frmLossRate, FILE *f, struct PacketStatistics *ps) {
    int drp = 0;
    srand(time(NULL));
    int randomNumber = (rand() % 100);
    if (currFrmType == AV_PICTURE_TYPE_I) {
        ps->numOfIFrames++;
        if ( (randomNumber < frmLossRate) && ( (frmTypeToDrop == AV_PICTURE_TYPE_I) || (frmTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
            ps->numOfDroppedIFrames++;
            ps->numOfDroppedPackets++;
            drp = 1;
        }
        if (drp){
            printf("\n-");
            if (f) fprintf(f, "\n-");
        } else {
            printf("\nI");
            if (f) fprintf(f, "\nI");
        }
    } else if (currFrmType == AV_PICTURE_TYPE_P) {
        ps->numOfPFrames++;
        if ( (randomNumber < frmLossRate) && ( (frmTypeToDrop == AV_PICTURE_TYPE_P) || (frmTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
            ps->numOfDroppedPFrames++;
            ps->numOfDroppedPackets++;
            drp = 1;
        }
        if (drp) {
            printf("-");
            if (f) fprintf(f, "-");
        } else {
            printf("P");
            if (f) fprintf(f, "P");
        }
    } else if (currFrmType == AV_PICTURE_TYPE_B) {
        ps->numOfBFrames++;
        if ( (randomNumber < frmLossRate) && ( (frmTypeToDrop == AV_PICTURE_TYPE_B) || (frmTypeToDrop == AV_PICTURE_TYPE_NONE) ) ) {
            ps->numOfDroppedBFrames++;
            ps->numOfDroppedPackets++;
            drp = 1;
        }
        if (drp) {
            printf("-");
            if (f) fprintf(f, "-");
        } else {
            printf("B");
            if (f) fprintf(f, "B");
        }
    } else {
        ps->numOfOtherFrames++;
        if ( (randomNumber < frmLossRate) && (frmTypeToDrop == AV_PICTURE_TYPE_NONE) ) {
            ps->numOfDroppedOtherFrames++;
            ps->numOfDroppedPackets++;
            drp = 1;
        }
        if (drp) {
            printf("-");
            if (f) fprintf(f, "-");
        }
        else {
            printf("O");
            if (f) fprintf(f, "O");
        }
    }
    return drp;
}

static AVFormatContext *open_input_file(struct Arguments a, int *vS, int *aS) {
    int i;
    // Open video file
    AVFormatContext *s = NULL;
    AVDictionary *d = NULL;
    s = avformat_alloc_context();
    s->probesize = 5 * 1000000;            // bytes
    s->max_analyze_duration = 5 * 1000000; // microseconds
    //pFormatCtx->fps_probe_size = 3000;

    if (avformat_open_input(&s, a.inputFileName, NULL, &d) != 0) {
        av_log(s, AV_LOG_ERROR, "Cannot Open Input: %s\n", a.inputFileName);
        return NULL; // Couldn't open file
    }

    av_log(s, AV_LOG_DEBUG, "%d Programs, %d Streams\n", s->nb_programs, s->nb_streams);
    for (i = 0; i < s->nb_streams; i++)
        av_log(s, AV_LOG_DEBUG, "StreamID[%d] = %d\n", i, s->streams[i]->id);

    // Retrieve stream information
    if (avformat_find_stream_info(s, NULL) < 0)
        return NULL; // Couldn't find stream information

    // Dump information about file onto standard error
    av_dump_format(s, 0, a.inputFileName, 0);
    // usleep(2 * 1000000);

    // Find video and audio streams
    *vS = -1;
    *aS = -1;
    for (i = 0; i < s->nb_streams; i++)
    {
        if (s->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            *vS = i;
        } else if  (s->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            *aS = i;
        }
    }
    if (*vS == -1) {
        av_log(s, AV_LOG_ERROR, "No Video Stream\n");
        return NULL;
    }
    if (*aS == -1) 
        av_log(s, AV_LOG_WARNING, "No Audio Stream\n");

    return s;
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
    if (!ofmt_ctx) {
        av_log(ofmt_ctx, AV_LOG_ERROR, "Could not create output context\n");
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
    // Initalizing Contexts to NULL prevents segfaults!
    AVFormatContext *pFormatCtx = NULL;
    AVCodecContext *codecCtx = NULL;
    AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVPacket packet;

    int i, videoStream, audioStream;
    int frameFinished;
    enum AVMediaType mediaType;
    int drop; // boolean

    struct Arguments arguments;
    struct PacketStatistics pktStats;
    
    av_log_set_level(AV_LOG_QUIET);

    if (parse_arguments(argc, argv, &arguments) < 0)
        return -1;

    // Register all formats and codecs
    av_register_all();
    avformat_network_init();

    pFormatCtx = open_input_file(arguments, &videoStream, &audioStream);
    if (!pFormatCtx)
        return -1;
    
    // Open Output File
    open_output_file(arguments.outputFileName, pFormatCtx);
    
    // Open Log File
    FILE *f;
    if (argc == 6) {
        f = fopen(arguments.logFileName, "w");
        if (f == NULL)
        {
            printf("Error opening file!\n");
            exit(1);
        }
    }

    i = 0;


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
    
    int pktIndex;

    AVPacket packetBuffer[PACKET_BUFFER_SIZE]; // 10 Seconds @ 60 fps
    int frameTypeBuffer[PACKET_BUFFER_SIZE];
    for (i = 0; i < PACKET_BUFFER_SIZE; i++)
        frameTypeBuffer[i] = -1; // Uninitialized
    int packetBufferSize = 0;

    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        mediaType = pFormatCtx->streams[packet.stream_index]->codec->codec_type;

        if (mediaType == AVMEDIA_TYPE_VIDEO) {

            // Buffering
            av_packet_ref(&packetBuffer[packetBufferSize], &packet);
            av_copy_packet(&packetBuffer[packetBufferSize], &packet);
            av_copy_packet_side_data(&packetBuffer[packetBufferSize], &packet);
            packetBuffer[packetBufferSize] = packet;
            packetBufferSize++;
            
            // ---- VIDEO DECODING ---- 
            avcodec_decode_video2(codecCtx, frame, &frameFinished, &packet);
            pktIndex = find_packet(packetBuffer, frameTypeBuffer, packetBufferSize, frame);
            /*if (pktIndex >= 0) {
                printf("Packet Found @%d: pts (%d vs %d)\n", pktIndex, frame->pkt_pts, packetBuffer[pktIndex].pts);
                //packet = packetBuffer[pktIndex];
            }*/

            // Print Buffer
            /*
            printf("====================================================\n");
            if (packetBufferSize < PACKET_BUFFER_SIZE) {
                for(i = 0; i < packetBufferSize; i++)
                    printf("Buffer[%d]: Size = %d, PTS = %d, Type = %d\n", i, packetBuffer[i].size, packetBuffer[i].pts, frameTypeBuffer[i]);
            }
            printf("====================================================\n");
            */

            //av_copy_packet(&packet, &packetBuffer[pktIndex]);
            //av_copy_packet_side_data(&packet, &packetBuffer[pktIndex]);
            //printf("PTS = (%d)%" PRId64 " | (%d)%" PRId64 "\n", frame->pict_type, frame->pts, packet.flags & AV_PKT_FLAG_KEY, packet.pts);
            if (!frameFinished) {
                printf("Incomplete Frame\n");
                //continue;
            }
            pktStats.numOfVideoPackets++;


            // Writing Frames From Packet Buffer to Output File
            frameTypeBuffer[0];
            while ( (frameTypeBuffer[0] != -1) && (packetBufferSize > 0) ) {
                drop = 0;
                //printf("currFrameType = %d\n", frameTypeBuffer[0]);
                drop = should_drop(frameTypeBuffer[0], arguments.frameTypeEnum, arguments.frameLossRate, f, &pktStats);

                if (!drop) {
                    //printf("\nWriting packet[%d]: size = %d, sI = %d, type = %d\n", 0, packetBuffer[0].size, packetBuffer[0].stream_index, mediaType);
                    int ffmpegReturn = av_interleaved_write_frame(ofmt_ctx, &packetBuffer[0]);
                    if (ffmpegReturn < 0) {
                        char buff[500];
                        av_strerror(ffmpegReturn, buff, 500);
                        av_log(NULL, AV_LOG_ERROR, "Error writing frame to %s with error code %d (%s)\n", arguments.outputFileName, ffmpegReturn, buff);
                        //usleep(2 * 1000000);
                        return -1;
                    }
                } else {
                    ;//printf("DROPPED FRAME: Type = %d\n", frameTypeBuffer[0]);
                }

                av_free_packet(&packetBuffer[0]);

                if (remove_packet(packetBuffer, frameTypeBuffer, packetBufferSize, 0) >= 0)
                    packetBufferSize--;
            }




        } else if (mediaType == AVMEDIA_TYPE_AUDIO) {
            pktStats.numOfAudioPackets++;
            int ffmpegReturn = av_interleaved_write_frame(ofmt_ctx, &packet);
        } else {
            pktStats.numOfUnknownPackets++;
            int ffmpegReturn = av_interleaved_write_frame(ofmt_ctx, &packet);
        }

        pktStats.numOfPackets++;
            
        // Free the packet that was allocated by av_read_frame
        // av_free_packet(&packet);
    }

    av_write_trailer(ofmt_ctx);

    print_statistics(pktStats, f);

    // Free the YUV frame
    av_frame_free(&frame);

    // Close the codecs
    avcodec_close(codecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);
    avformat_close_input(&ofmt_ctx);

    // Close log file
    fclose(f);

    return 0;
}