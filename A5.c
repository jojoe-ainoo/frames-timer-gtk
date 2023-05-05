/**
 * @file A5.c
 * @author Emmanuel Ainoo & Elijah Ayomide Oduba
 * @brief 
 * 
 * Multithreading with ffmpeg and GTK
 * 
 * @version 0.1
 * @date 22-11-04
 * 
 * @copyright Copyright (c) 2022
 * @cite //https://github.com/leandromoreira/ffmpeg-libav-tutorial#transcoding
 */


// FFMPEG Libraries
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

// Standard C Libraries
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// GTK Libraries
#include <cairo.h>
#include <gtk/gtk.h>

// //Thread Libraries
// #include <pthread.h>
// #include <unistd.h>

//Initialization
#define BUFFER_SIZE 1026 // buffer for producer & consumer threads
#define WIN_HEIGHT 250 // height of window
#define WIN_WIDTH  320 // width of window
#define NUM_FRAMES 100 // specific number of frames to extract
#define SPEED_OF_VIDEO 100 // in milliseconds

#define MaxItems 2 // maximum items a producer can produce or a consumer can consume
#define BufferSize 3 // Size of the buffer

// Global Variables ffmpeg
static AVFrame *rgb_frame_array [NUM_FRAMES]; // store RGB Frames
static enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P, dst_pix_fmt = AV_PIX_FMT_RGB24; // define av formats for RGB and YUV4
static int animate_position = 0; // determine position change for timer

// GTK variables for GTK
GtkWidget *window, *drawing_area;


// //Global Variables for Thread
// static sem_t empty; // empty slots in buffer
// static sem_t full; // filled slots in buffer
// static int in = 0; // index at which producer will put the next data
// static int out = 0; // index from which the consumer will consume next data
// static AVFrame buffer[BufferSize]; // buffer to hold frames
// static pthread_mutex_t mutex; // Used to provide mutual exclusion for critical section -- for locking



/**
 * @brief 
 * Function to log messages
 * @param fmt 
 * @param ... 
 */
static void logging(const char *fmt, ...);

/**
 * @brief 
 * Function to decode stream packets into frames
 * @param pPacket 
 * @param pCodecContext 
 * @param pFrame 
 * @return int 
 */
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);

/**
 * @brief 
 * Function to save grayscale frame for image
 * @param buf 
 * @param wrap 
 * @param xsize 
 * @param ysize 
 * @param fnumber 
 */
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, int fnumber);

/**
 * @brief 
 * Function to allocate a new frame of data
 * @param width 
 * @param height 
 * @return AVFrame* 
 */
static AVFrame* allocateFrame(int width, int height);


/**
 * @brief 
 * Function to save rgb frame
 * @param frame 
 * @param fnumber 
 */
static void save_rgb_frame(AVFrame *frame, int fnumber);

/**
 * @brief 
 * Function to refresh gtk window
 * @param window 
 * @return gboolean 
 */
static gboolean refresh_screen(GtkWidget *window);

/**
 * @brief 
 * Function to update animate_position for frames
 */
static void update_position();

/**
 * @brief 
 * Function to draw rgb raw data on gtk window
 * @param widget 
 * @param cr 
 * @param user_data 
 */
static void on_draw_event(GtkWidget *widget, cairo_t *cr, unsigned char *user_data);


// static int *producer(AVFrame *frame, void *pno);
// static int *consumer(AVFrame *frame, void *cno);

int main(int argc, char **argv){

    // Check to make sure filename is passed to the command line
    if (argc < 2){ 
        logging("ERROR You need to specify a media file.\n");
        return -1; //exit
    }

    logging("Initializing all the containers, codecs and protocols.");

    // AVFormatContext holds the header information from the format (Container) - Allocating memory for this component
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        logging("ERROR could not allocate memory for Format Context");
        return -1; //exit
    }

    // Open the file and read its header. The codecs are not opened.
    logging("Opening the input file (%s) and loading format (container) header", argv[1]);
    if (avformat_open_input(&pFormatContext, argv[1], NULL, NULL) != 0) {
        logging("ERROR av could not open the file");
        return -1; //exit
    }

    // Log some info about file after reading header
    logging("Format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
    
    // Read Packets from the Format to get stream information, this function populates pFormatContext->streams
    logging("finding stream info from format");
    if (avformat_find_stream_info(pFormatContext,  NULL) < 0){
        logging("ERROR could not get the stream info");
        return -1; //exit
    }

    AVCodec *pCodec = NULL; // the component that knows how to enCOde and DECode the stream
    AVCodecParameters *pCodecParameters =  NULL;   // this component describes the properties of a codec used by the stream i
    int video_stream_index = -1;

    // Loop though all the streams and print its main information
    for (int i = 0; i < pFormatContext->nb_streams; i++) {
        AVCodecParameters *pLocalCodecParameters =  NULL;
        pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
        logging("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
        logging("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
        logging("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
        logging("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

        logging("Finding the proper decoder (CODEC)");

        AVCodec *pLocalCodec = NULL;
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id); // finds the registered decoder for a codec ID

        if (pLocalCodec==NULL) {
            logging("ERROR unsupported codec!"); // if the codec is not found, just skip it
            continue;
        }

        // When the stream is a video we store its index, codec parameters and codec
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (video_stream_index == -1) {
                video_stream_index = i;
                pCodec = pLocalCodec;
                pCodecParameters = pLocalCodecParameters;
            }
        logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
        } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {   
            logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
        }
        // Print its name, id and bitrate
        logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, pLocalCodecParameters->bit_rate);
    } 

    // Check file to check if contains video stream 
    if (video_stream_index == -1) {
        logging("ERROR file %s does not contain a video stream!", argv[1]);
        return -1;
    }

     // Allocate memory for AvCodecContext
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        logging("ERROR failed to allocated memory for AVCodecContext");
        return -1;
    }

    // Fill the codec context based on the values from the supplied codec parameters
    if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0){
        logging("ERROR failed to copy codec params to codec context");
        return -1;
    }

    // Initialize the AVCodecContext to use the given AVCodec.
    if (avcodec_open2(pCodecContext, pCodec, NULL) < 0){
        logging("ERROR Failed to open codec through avcodec_open2");
        return -1;
    }

     // Allocate memory for frame
    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        logging("ERROR failed to allocate memory for AVFrame");
        return -1;
    }

     // Allocate memory packet
    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket) {
        logging("ERROR failed to allocate memory for AVPacket");
        return -1;
    }

    int response = 0;
    int num_of_packets_to_process = NUM_FRAMES;

    // Fill the Packet with data from the Stream
    while (av_read_frame(pFormatContext, pPacket) >= 0) {

        if (pPacket->stream_index == video_stream_index) { // if it's the video stream
            logging("AVPacket->pts %" PRId64, pPacket->pts);
            response = decode_packet(pPacket, pCodecContext, pFrame); // decode packet form stream 

            //TODO producer(pPacket, pCodecContext, pFrame, producer) --> calls decode packet and wait
            //TODO consumer(consumer) --> calls timer and animate to update
           
           
            if (response < 0)
                break;

            if (--num_of_packets_to_process <= 0) break; // stop it when 8 packets are loaded
        }
        av_packet_unref(pPacket); // unreference packet to default values
    }

    logging("Releasing all ffmpeg the Resources");
    avformat_close_input(&pFormatContext); // close stream input
    av_packet_free(&pPacket); // free packet resources
    av_frame_free(&pFrame); // free frame resources
    avcodec_free_context(&pCodecContext); // free context


    logging("Begin GTK Application");

    // initialize GTK widgets
    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), drawing_area);
    logging("Initializing GTK WIdgets");

    // Use signal connect to handle events for drawing area
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // GTK set window details
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), WIN_WIDTH, WIN_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(window), "GTK Video Player with Timer");
    gtk_widget_show_all(window);

    // use timer to fresh gtk window
    logging("Using Timer to Delay and Refresh screen");
    (void)g_timeout_add(SPEED_OF_VIDEO, (GSourceFunc)refresh_screen, window); 
    gtk_main();

    return 0;
}

/**
 * @brief 
 * Function Definition of Logging out Messages
 * @param fmt 
 * @param ... 
 */
static void logging(const char *fmt, ...){
    va_list args;
    fprintf( stderr, "{LOG}:-- " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

/**
 * @brief 
 * Function to decode packets from stream 
 * @param pPacket 
 * @param pCodecContext 
 * @param pFrame 
 * @return int 
 */
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame) {
    int response = avcodec_send_packet(pCodecContext, pPacket);   // Supply raw packet data as input to a decoder

    if (response < 0) {
        logging("Error while sending a packet to the decoder: %s", av_err2str(response));
        return response;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(pCodecContext, pFrame);    // Return decoded output data (into a frame) from a decoder
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } 
        else if (response < 0) {
            logging("Error while receiving a frame from the decoder: %s", av_err2str(response)); // log error message from reponse
            return response;
        }

        if (response >= 0) {
            logging(
                "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
                pCodecContext->frame_number,
                av_get_picture_type_char(pFrame->pict_type),
                pFrame->pkt_size,
                pFrame->format,
                pFrame->pts,
                pFrame->key_frame,
                pFrame->coded_picture_number
            );

        // save a grayscale frame into a .pgm file
        // save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, pCodecContext->frame_number);

        // save a rgb frame into ppm file
        save_rgb_frame(pFrame, pCodecContext->frame_number);
        }
    }
    return 0;
}

/**
 * @brief 
 * Function to save the frame as a grayscale
 * @param buf 
 * @param wrap 
 * @param xsize 
 * @param ysize 
 * @param filename 
 */
static void save_gray_frame(unsigned char *buf,  int wrap, int xsize, int ysize, int fnumber) {
    
    char frame_filename[1024]; // define filename for frame files
    snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", fnumber); // append frame number to filename

    FILE *f; // create file object
    int i; // index each line of file

    f = fopen(frame_filename,"w"); // writing the minimal required header for a pgm file format
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255); //portable graymap format 

    // writing grayscale image data line by line to file
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

/**
 * @brief 
 * Function to convert frame to rgb using swscale and write to file
 * @param pFrame 
 * @param fnumber 
 */
static void save_rgb_frame(AVFrame *pFrame, int fnumber) {
   
    char frame_filename[1024];// define filename for frame files
    snprintf(frame_filename, sizeof(frame_filename), "%s-%d.ppm", "frame", fnumber); //append frame number to filename
  
    // create scaling to convert to rgb
    AVFrame* frame_rgb = allocateFrame(pFrame->width, pFrame->height);

    struct SwsContext* converted_data = sws_getContext(pFrame->width, pFrame->height, pFrame->format, frame_rgb->width,frame_rgb->height, dst_pix_fmt, SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, NULL, NULL, NULL);
    sws_scale(converted_data,(uint8_t const * const *)pFrame->data, pFrame->linesize, 0, pFrame->height, frame_rgb->data, frame_rgb->linesize);
    sws_freeContext(converted_data);

    // insert rgb frame into global rgb frame for cairo to create surface
    int index = fnumber - 1; // use frame number - 1 for array index
    rgb_frame_array[index] = frame_rgb; // insert rgb frame into rgb frame array
    logging("Filling up RGB array of raw data at position: %d ",index);

}

/**
 * @brief 
 * Function to allocate memory for frame (needed for rgb conversion)
 * @param width 
 * @param height 
 * @return AVFrame* 
 */
static AVFrame* allocateFrame(int width, int height){

    AVFrame* newFrame = av_frame_alloc(); // allocate memory for new destination frame for rgb

    // Allocate memeory for AVFrame
    if (newFrame == NULL){
        logging("ERROR failed to allocate memory for AVFrame");
        exit(1);
    }
    
    // allocate memory for AV Image
    if (av_image_alloc(newFrame->data, newFrame->linesize, width, height, dst_pix_fmt, 1) < 0) {
        logging("ERROR failed to allocate memory for AV Image");

    }
    // update new frame parameters to match destination frame for rgb
    newFrame->width = width;
    newFrame->height = height;
    newFrame->format = dst_pix_fmt; // format for rgb 
    return newFrame;
}

/**
 * @brief 
 * Function to draw rgb raw data on gtk window
 * @param widget 
 * @param cr 
 * @param user_data 
 */
static void on_draw_event(GtkWidget *widget, cairo_t *cr, unsigned char *user_data) {
    //create new cairo surface with raw rgb data
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, WIN_WIDTH, WIN_HEIGHT);
    unsigned char *current_row = cairo_image_surface_get_data(surface);
    int x, y, stride = cairo_image_surface_get_stride(surface);

    // get rgb data from rgb_frame_array global
    unsigned char *buf = rgb_frame_array[animate_position]->data[0]; // use animate_position to switch between frames
    logging("Get RGB Frame at: %d\n", animate_position);
    cairo_surface_flush(surface);

    //write line by line onto cairo surface with rgb raw data
    for (y = 0; y < WIN_HEIGHT; ++y) {
        uint32_t *row = (void *)current_row;
        const int line_size = rgb_frame_array[animate_position]->linesize[0]; // use animate_position to switch between frame data
        for (x = 0; x < line_size / 3; ++x) {
            uint32_t r = *(buf + y * line_size + x * 3 + 0); // get red value of rgb data
            uint32_t g = *(buf + y * line_size + x * 3 + 1); // get green value of rgb data
            uint32_t b = *(buf + y * line_size + x * 3 + 2); // get blue value of rgb data
            row[x] = (r << 16) | (g << 8) | b;
        }
        current_row += stride;
    }
    cairo_surface_mark_dirty(surface);
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
    return;
}

/**
 * @brief 
 * Function to Update animate_position for frames
 */
static void update_position() {
    //check frame position to avoid segmentation fault
    if(animate_position==(NUM_FRAMES-2)){//frame is stored i index-2
        logging("animate_position: %d\n", animate_position);
        animate_position = 0; // restart video when it gets to end rgb frame array 
    }else {
        animate_position++; // increase frame position
        logging("animate_position: %d\n", animate_position);
    }
}

/**
 * @brief 
 * Function to Refresh gtk window
 * @param window 
 * @return gboolean 
 */
static gboolean refresh_screen(GtkWidget *window) {
    // call position to redraw next frame
    update_position();
    // refresh gtk window
    logging("Refresh GTK Window");
    gtk_widget_queue_draw_area(window, 0, 0, WIN_WIDTH, WIN_HEIGHT);
    return TRUE;
}



//TODO pass in packet details for decoding
// static void producer( AVFrame *frame, void *pno){
    //int response;
//     // int item;
//     for(int i = 0; i < MaxItems; i++) {
//         // item = rand(); // Produce a random item
//         sem_wait(&empty); // wait on empty slots

//         pthread_mutex_lock(&mutex); // lock producing process
//         // buffer[in] = item; // add item to buffer
// TODO make call to decode 
            //response = decode_packet(pPacket, pCodecContext, pFrame); 
//          buffer[in] = frame; // add frame to buffer

//         logging("Producer %d: decode frame %d at %d\n", *((int *)pno),buffer[in],in); 
//         in = (in+1)%BufferSize;
//         pthread_mutex_unlock(&mutex); // unlock thread after process
        
//         sem_post(&full); //signal consumer buffer has some frames to grab

//     }
      //  return response;
// }

// static void *consumer(void *cno) {   
//     for(int i = 0; i < MaxItems; i++) {
//         sem_wait(&full); // wait if no full slots

//         pthread_mutex_lock(&mutex); // lock for mutual exclusion
//         // int item = buffer[out]; // consume data to be done by timer

//         (void)g_timeout_add(SPEED_OF_VIDEO, (GSourceFunc)refresh_screen, window); 

//         logging("Consumer %d: Grab Frame %d from %d\n",*((int *)cno),item, out); // grab data from frame
//         out = (out+1)%BufferSize;
//         pthread_mutex_unlock(&mutex); // unlock mutex

//         sem_post(&empty); // signal producer consumer is done
//     }
// }



//CONSUMER- thread2 -- wait until buffer has something before putting in
// consumer():
// lock()
// timerEvent()
// grabOneFrame()
// while(buffer_size <= 0)
//   wait()
// display()
// signal()
//unlock#include <pthread.h>


//PRODUCER - thread1 -- wait until buffer has space before putting in
// int buffer_size = 0; //0 == empty, 5 == full
// producer():
// lock()
// while(buffer_size >= 5):
    // wait()
// printf(decodeFrame())
// addBuffer
// signal()
// unlock()