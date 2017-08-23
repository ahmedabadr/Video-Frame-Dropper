// dropFrames.c
// Build
// gcc dropFrames.c -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames
// gcc -w dropFrames.c -Llib -lavformat -lavcodec -lavutil -o dropFrames -Wl,-rpath=lib && echo $?
// gcc dropFrames.c -I/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/ffmpeg -L/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib -lavformat -lavcodec -lswscale -lz -lavutil -o dropFrames -Wl,-rpath=/home/shahzad/Desktop/Repo_Clones/Video-Frame-Dropper/sqm-ffmpeg/lib && echo $?
// Usage:
// ./dropFrames inputFile outputFile frameType dropRate [logFile]\n
// Examples:
// ./dropFrames videos/Messi_GOP10.ts videos/Messi_GOP10_TypeI_FLR1.ts I 1 videos/Messi_GOP10_TypeI_FLR
// ./dropFrames input.ts output.ts P 100 output.log
// Useful for Testing:
// ./ffprobe -loglevel quiet -select_streams v:0 -show_frames output.ts | grep "SOMETHING"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define PACKET_BUFFER_SIZE    600
#define MAX_FIELD_LEN         128
#define MAX_VECTOR_LEN        20

//static AVFormatContext *ifmt_ctx;
static AVFormatContext *ofmt_ctx;

struct Arguments {
    // files
    char inputFileName[MAX_FIELD_LEN];
    char outputFileName[MAX_FIELD_LEN];
    char logFileName[MAX_FIELD_LEN];

    // fdt
    char frameType[MAX_VECTOR_LEN];
    enum AVPictureType frameTypeEnum[MAX_VECTOR_LEN];
    
    // fdr
    int frameLossRate[MAX_VECTOR_LEN];

    // fdd 
    int frameDropDuration[MAX_VECTOR_LEN];

    // Vector Lengths
    int fdtSize;
    int fdrSize;
    int fddSize;
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
    if ( ( argc < 3 ) || ( argc > 13 ) ) {
        printf( "USAGE: -i [INPUT_FILE] -o [OUTPUT_FILE] -fdt [FRAME_DROP_TYPE] -fdr [FRAME_DROP_RATE] -fdd [FRAME_DROP_DURATION] -l [LOG_FILE]\n");
        printf( "WHERE:\n");
        printf( "      [-i]     : path to input media file from. \n");
        printf( "      [-o]     : path to output the generated media file. \n");
        printf( "      [-fdt]   : frame drop type. ( A, I, P, B ). Default is A if not specified. \n");
        printf( "      [-fdr]   : frame drop rate. ( MIN=0 , MAX=100 ). Default is 0 if not specified. \n");
        printf( "      [-fdd]   : frame drop duration, specifies how many frames to do the dropping on. ( MIN=0 ,MAX=TotalFramesInFile )\n");
        printf( "      [-l]     : path to the input media file.\n\n");

        printf( "      Optional: -o   [Default: prepends 'Dropped_File_' to the input file name]\n" );
        printf( "                -fdt [Default: A]\n" );
        printf( "                -fdr [Defualt: 0]\n" );                 
        printf( "                -fdd [Default: 0]\n" ); 
        printf( "                -l   [Default: no log output]\n" ); 
        printf( "EXAMLPLE COMMAND: [./dropFrames -i in.ts -o out.ts -fdt A -fdr 10 -l debug.log] \n" );

        return -1;
    }
    int arg_index = 1; 
    for( arg_index = 1; arg_index < argc; ++(arg_index) ) {
    	
    	// Find -i and store it's argument inside the Arguments structure.
    	if ( strcmp( argv[arg_index], "-i" ) == 0 ) { 
           	if ( ( ( arg_index + 1 ) < argc ) && ( strcmp( argv[arg_index + 1], "" ) != 0 ) ) { 
                strcpy( a->inputFileName, argv[arg_index + 1] );
                if ( strcmp( a->outputFileName, "" ) == 0 ) { // If output file name not set yet.
                    strcpy( a->outputFileName, "Dropped_File_");
                    strcat( a->outputFileName , a->inputFileName );
                }
                printf( "INPUT_FILE: %s \n", a->inputFileName );
            }
    	    else {
    	        printf("Error: [-i] Needs a Valid Input File Name.\n");
    	    	return( -1 );
    	    }
        }

    	// Find -o and store it's argument inside the Arguments structure.
     	else if ( strcmp( argv[arg_index], "-o" ) == 0 ) {
     
           	if ( ( ( arg_index + 1 ) < argc ) && ( strcmp( argv[arg_index + 1], "" ) != 0 ) ) { 
                strcpy( a->outputFileName, argv[arg_index + 1] );
      	        printf( "OUTPUT_FILE: %s \n", a->outputFileName );
                }
    	    else {
    	        printf( "Error: [-o] Needs a Valid Output File Name. \n" );
    	    	return( -1 );
    	    }
        }       

    	// Find -fdt and store it's argument inside the Arguments structure.
     	else if ( strcmp( argv[arg_index], "-fdt" ) == 0 ) {
     
           	if ( ( arg_index + 1 ) < argc ) { // Check if there is an argument for this option.

                int fdt_count = 0;
                char frameType_arg[MAX_FIELD_LEN];
                strcpy( frameType_arg, argv[arg_index + 1] );
                char * fdt_token = strtok( frameType_arg, "," );
                while ( fdt_token != NULL ) { // Parse the frame drop types and store them if they are valid.

                    // Set the frame dropping types, which an only be one of: A, I, P, B.
                    a->frameType[fdt_count] = *( fdt_token ) ; // get the first character.

                    if ( a->frameType[fdt_count] == 'A' )  {
                   	    a->frameTypeEnum[fdt_count] = AV_PICTURE_TYPE_NONE;
                     } else if ( a->frameType[fdt_count] == 'I' )  {
                   	    a->frameTypeEnum[fdt_count] = AV_PICTURE_TYPE_I;
                     } else if ( a->frameType[fdt_count] == 'P' )  {
                   	    a->frameTypeEnum[fdt_count] = AV_PICTURE_TYPE_P;
                     } else if ( a->frameType[fdt_count] == 'B' )  {
                   	    a->frameTypeEnum[fdt_count] = AV_PICTURE_TYPE_B;
                     } else { // Not a Valid Frame Type.
                        printf( "Error: [-fdt] Needs ALL Valid Frame Dropping Types (A, I, P, B) \n" );
                        return( -1 );
                     }      

                    printf( "[%d-th]FRAME_DROP_TYPE: %c \n", fdt_count, a->frameType[fdt_count] );
                    ++( fdt_count );
                    fdt_token = strtok( NULL, "," );
                } // End of While-Loop that parses the frame drop types.
                
                a->fdtSize = fdt_count;
                printf( "NUMBER OF FRAME_DROP_TYPES: %d \n", a->fdtSize );

            } // End of case to check if there is an argument.

    	    else {
    	        printf( "Error: [-fdt] Needs an argument.\n" );
    	    	return( -1 );
    	    }

        } // End of "-fdt" case.

        // Find -fdr and store it's argument inside the Arguments structure.
        else if ( strcmp( argv[arg_index], "-fdr" ) == 0 ) {
     
            if ( ( arg_index + 1 ) < argc ) { // Check if there is an argument for this option.
    
                int fdr_count = 0;
                char frameLossRate_arg[MAX_FIELD_LEN];
                strcpy( frameLossRate_arg, argv[arg_index + 1] );
                char * fdr_token = strtok( frameLossRate_arg, "," );
                while ( fdr_token != NULL )  { // Parse the frame drop rates and store them if they are valid.
                    
                    // Set the frame dropping rates, which is within the range [0, 100] (inslusive) 
                    a->frameLossRate[fdr_count] = (int) strtol( fdr_token, NULL, 10 );
              
                    // Handle Non-Valid Frame Drop Rates.
                    if ( ( a->frameLossRate[fdr_count] ) < 0 )  {
                        a->frameLossRate[fdr_count] = 0;
                        printf( "Warning: [-fdr] Needs ALL Valid Frame Dropping Rates (0-100), Recieved a Negative. \n" );
                    } else if ( ( a->frameLossRate[fdr_count] ) > 100 )  {
                        a->frameLossRate[fdr_count] = 100;
                        printf( "Warning: [-fdr] Needs ALL Valid Frame Dropping Rates (0-100), Recieved an over 100.  \n" );
                    }

                    printf( "[%d-th]FRAME_DROP_RATE: %d \n", fdr_count, a->frameLossRate[fdr_count]  );
                    ++( fdr_count );
                    fdr_token = strtok( NULL, "," );
                } // End of While-Loop that parses the frame loss rates.
                
                a->fdrSize = fdr_count;
                printf( "NUMBER OF FRAME_DROP_RATES: %d \n", a->fdrSize );

            } // End of case to check if there is an argument.
           
            else { 
                printf( "Error: [-fdr] Needs an argument.\n" ); 
    	    	return( -1 );
            }

        } // End of "-fdr" case.

         // Find -fdd and store it's argument inside the Arguments structure.
        else if ( strcmp( argv[arg_index], "-fdd" ) == 0 ) {

            if ( ( arg_index + 1 ) < argc ) { // Check if there is an argument for this option.
    
                int fdd_count = 0;
                char frameDropDuration_arg[MAX_FIELD_LEN];
                strcpy( frameDropDuration_arg, argv[arg_index + 1] );
                char * fdd_token = strtok( frameDropDuration_arg, "," );
                while ( fdd_token != NULL )  { // Parse the frame drop rates and store them if they are valid.
                    
                    // Set the durations to drop frames upto (duration count is in terms frames).
                    a->frameDropDuration[fdd_count] = (int) strtol( fdd_token, NULL, 10 );
                    
                    // Handle Non-Valid Frame Durations.
                    if ( ( a->frameDropDuration[fdd_count] ) < 0 )  { 
                        a->frameDropDuration[fdd_count] = 0;
                        printf( "Warning: [-fdd] Needs ALL Valid Frame Dropping Durations (0-NumOfFramesInFile) \n" );
                    }     

                    printf( "[%d-th]FRAME_DROP_DURATION: %d \n", fdd_count, a->frameDropDuration[fdd_count]  );

                    ++( fdd_count );
                    fdd_token = strtok( NULL, "," );
                } // End of While-Loop that parses the frame drop durations.
                
                a->fddSize = fdd_count;
                printf( "NUMBER OF FRAME_DROP_DURATION: %d \n", a->fddSize );

            } // End of case to check if there is an argument.

            else {
                printf( "Error: [-fdd] Needs an argument.\n" );
                return( -1 );
            }

        } // End of "-fdd" case.


        // Find -l and store it's argument inside the Arguments structure.
        else if ( strcmp( argv[arg_index], "-l" ) == 0 ) {
            if ( ( arg_index + 1 ) < argc ) { // Check if there is an argument for this option.
                strcpy( a->logFileName, argv[arg_index + 1] );
                printf( "LOG_FILE: %s \n", a->logFileName );
            } // End of case to check if there is an argument.

            else {
                printf( "Error: [-l] Needs an argument.\n" );
            return( -1 );
            }
        } // End of "-l" case.

    } // End of For-Loop that parsees all the arguments.
    
    // Check if -fdt -fdr -fdd all have same number of arguments provided.
    if ( ( a->fdtSize == a->fdrSize ) &&
         ( a->fdtSize == a->fddSize ) &&
         ( a->fdrSize == a->fddSize ) ) {
        return ( 0 );
    }
    else {
        printf( "ERROR: All arguments to -fdt -fdr -fdd must have same number of elements.\n" ); 
        return( -1 );
    }
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

// Modifies fdt_ptr and fdr_ptr to become the respective frame types to drop and the rate to drop at in this current interval(based on fdd)
int get_Type_Rate( int FrameIndex, struct Arguments * a, int * fdt_ptr, int * fdr_ptr ) {
   
    // Parse through the frameDropDuration to find which interval we are in currently, with respect to the current frame index, 
    int index_fdd = 0 ;   
    for ( index_fdd = 0; index_fdd < ( a->fddSize ) ; ++( index_fdd ) ) {
        if ( FrameIndex <= ( a->frameDropDuration[index_fdd] ) ) {
             *( fdt_ptr ) = a->frameTypeEnum[index_fdd];
             *( fdr_ptr ) = a->frameLossRate[index_fdd];
             break;
        }
        else {
            // If last element in frameDropDuration then use the last options for fdt and fdr till the end. 
            if ( ( a->frameDropDuration[index_fdd] == 0 ) ||
                 ( ( index_fdd + 1 ) == ( a->fddSize ) ) ) {
                *( fdt_ptr ) = a->frameTypeEnum[index_fdd];
                *( fdr_ptr ) = a->frameLossRate[index_fdd];
            } 
        }
    }  

    if ( ( *( fdt_ptr ) < 0 ) || ( *( fdr_ptr ) < 0 ) || ( FrameIndex < 0 ) ) {
       return( -1 );  
    }

    return( index_fdd );
}


int should_drop(int currFrmType, int frmTypeToDrop, int frmLossRate, FILE *f, struct PacketStatistics *ps) {
    int drp = 0;
    
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

    // Initialize PacketStatistics Defaults.
    struct PacketStatistics pktStats;
    pktStats.numOfPackets = 0 ;
    pktStats.numOfVideoPackets = 0 ;
    pktStats.numOfAudioPackets = 0 ;
    pktStats.numOfUnknownPackets = 0 ;
    pktStats.numOfIFrames = 0 ;
    pktStats.numOfPFrames = 0 ;
    pktStats.numOfBFrames = 0 ;
    pktStats.numOfOtherFrames = 0 ;
    pktStats.numOfDroppedIFrames = 0 ;
    pktStats.numOfDroppedPFrames = 0 ;
    pktStats.numOfDroppedBFrames = 0 ;
    pktStats.numOfDroppedOtherFrames = 0 ;
    pktStats.numOfDroppedPackets = 0 ;
    
    av_log_set_level(AV_LOG_QUIET);
    srand(time(NULL));

    // Initialize Arguments Defaults.
    struct Arguments arguments;
    strcpy( arguments.inputFileName, "" );
    strcpy( arguments.outputFileName, "" );
    strcpy ( arguments.logFileName, "" );
    arguments.frameType[0] = 'A' ;
    arguments.frameTypeEnum[0] = AV_PICTURE_TYPE_NONE;
    arguments.frameLossRate[0] = 0;
    arguments.frameDropDuration[0] = 0;
    arguments.fdtSize = 1;
    arguments.fdrSize = 1;
    arguments.fddSize = 1;

    if ( parse_arguments( argc, argv, &arguments ) < 0 ) {
        return -1;
    }
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
    if ( strcmp( arguments.logFileName, "" ) != 0 ) {
        f = fopen(arguments.logFileName, "w");
        if (f == NULL)
        {
            printf("Error opening log file!\n");
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

    while (av_read_frame(pFormatCtx, &packet) >= 0) {
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
               
                int curr_fdt = -2;
                int curr_fdr = -2;
                // Get the type to drop and the rate to drop at based on this interval(finds current interval from frameDropDuration). 
                if ( get_Type_Rate( pktStats.numOfVideoPackets, &arguments, &curr_fdt, &curr_fdr ) < 0 ) {
                    printf( "ERROR While Getting Type and Rate for %d Frame Index\n", pktStats.numOfVideoPackets ) ;
                }

                drop = should_drop(frameTypeBuffer[0], curr_fdt, curr_fdr, f, &pktStats);

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

    // Close log file if user specified to dump a log
    if ( strcmp( arguments.logFileName, "" ) != 0 ) {
        fclose(f);
    }

    return 0;
}
