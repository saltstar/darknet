
#include "network.h"
#include "detection_layer.h"
#include "region_layer.h"
#include "cost_layer.h"
#include "./tools/utils.h"
#include "parser.h"
#include "box.h"
#include "image.h"
#include "demo.h"
#include <sys/time.h>

#define DEMO 1

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef OPENCV
#include "opencv2/highgui/highgui_c.h"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/videoio/videoio_c.h"

static CvVideoWriter *video_writer;
static char **demo_names;
static image **demo_alphabet;
static int demo_classes;
static network *net;
static image buff [3];
static image buff_letter[3];
static int buff_index = 0;
static void * cap;
static float fps = 0;
static float demo_thresh = 0;
static float demo_hier = .5;
static int running = 0;
static int demo_frame = 3;
static int demo_index = 0;
static float **predictions;
static float *avg;
static int demo_done = 0;
static int demo_total = 0;
double demo_time;

//the cycle when to pick a frame
int frame_cycle = 30/(1000.f/25);

detection *get_network_boxes(network *net, int w, int h, float thresh, float hier, int *map, int relative, int *num);

void sig_int(int signo) {
    printf("\n get intrrupt command. close videowriter.\n");
    if(video_writer) {
        cvReleaseVideoWriter(&video_writer);
    }
	_exit(0);
}

int size_network(network *net) {
    int i;
    int count = 0;
    for(i=0; i<net->n; ++i) {
        layer l = net->layers[i];
        if(l.type==YOLO || l.type==REGION || l.type==DETECTION) {
            count += l.outputs;
        }
    }
    return count;
}

void remember_network(network *net) {
    int i;
    int count = 0;
    for(i=0; i<net->n; ++i) {
        layer l = net->layers[i];
        if(l.type==YOLO || l.type==REGION || l.type==DETECTION) {
            memcpy(predictions[demo_index] + count, net->layers[i].output, sizeof(float) * l.outputs);
            count += l.outputs;
        }
    }
}

detection *avg_predictions(network *net, int *nboxes) {
    int i, j;
    int count = 0;
    fill_cpu(demo_total, 0, avg, 1);
    for(j=0; j<demo_frame; ++j) {
        axpy_cpu(demo_total, 1./demo_frame, predictions[j], 1, avg, 1);
    }
    for(i=0; i<net->n; ++i) {
        layer l = net->layers[i];
        if(l.type==YOLO || l.type==REGION || l.type==DETECTION) {
            memcpy(l.output, avg + count, sizeof(float) * l.outputs);
            count += l.outputs;
        }
    }
    detection *dets = get_network_boxes(net, buff[0].w, buff[0].h, demo_thresh, demo_hier, 0, 1, nboxes);
    return dets;
}

void *detect_in_thread(void *ptr) {
    printf("start detect thread ...\n");
    running = 1;
    float nms = .4;

    layer l = net->layers[net->n-1];
    float *X = buff_letter[(buff_index+2)%3].data;
	printf("detect buff[%d]\n",(buff_index+2)%3);
    network_predict(net, X);

    /*
       if(l.type == DETECTION){
       get_detection_boxes(l, 1, 1, demo_thresh, probs, boxes, 0);
       } else */
    remember_network(net);
    detection *dets = 0;
    int nboxes = 0;
    dets = avg_predictions(net, &nboxes);


    /*
       int i,j;
       box zero = {0};
       int classes = l.classes;
       for(i = 0; i < demo_detections; ++i){
       avg[i].objectness = 0;
       avg[i].bbox = zero;
       memset(avg[i].prob, 0, classes*sizeof(float));
       for(j = 0; j < demo_frame; ++j){
       axpy_cpu(classes, 1./demo_frame, dets[j][i].prob, 1, avg[i].prob, 1);
       avg[i].objectness += dets[j][i].objectness * 1./demo_frame;
       avg[i].bbox.x += dets[j][i].bbox.x * 1./demo_frame;
       avg[i].bbox.y += dets[j][i].bbox.y * 1./demo_frame;
       avg[i].bbox.w += dets[j][i].bbox.w * 1./demo_frame;
       avg[i].bbox.h += dets[j][i].bbox.h * 1./demo_frame;
       }
    //copy_cpu(classes, dets[0][i].prob, 1, avg[i].prob, 1);
    //avg[i].objectness = dets[0][i].objectness;
    }
     */

    if (nms > 0) do_nms_obj(dets, nboxes, l.classes, nms);
    //printf("\033[2J");
    //printf("\033[1;1H");
    printf("FPS:%.1f\n",fps);
    printf("Objects:\n");
    image display = buff[(buff_index+2) % 3];
    draw_detections(display, dets, nboxes, demo_thresh, demo_names, demo_alphabet, demo_classes);
    free_detections(dets, nboxes);

    demo_index = (demo_index + 1)%demo_frame;
    running = 0;
	printf("end detect thread ...\n");
    return 0;
}

void *fetch_in_thread(void *ptr) {
    printf("start fetch thread ...\n");
    free_image(buff[buff_index]);
    printf("fetch buff[%d]\n",buff_index);

    image img_frame;
	int frame_index = 0;
	while(true) {
        img_frame = get_image_from_stream(cap);
        if(img_frame.data == 0) {
             demo_done = 1;
             return 0;
        }
        frame_index++;
        if(frame_index > frame_cycle) {
            break;
        }
    }
    //buff[buff_index] = get_image_from_stream(cap);
    buff[buff_index] = img_frame;
    letterbox_image_into(buff[buff_index], net->w, net->h, buff_letter[buff_index]);
	printf("end fetch thread ...\n");
    return 0;
}

void *display_in_thread(void *ptr) {
    printf("start display thread ...\n");
    int c = show_image(buff[(buff_index + 1)%3], "Demo", 1);
    printf("display buff[%d]\n",(buff_index+1)%3);
    if (c != -1) c = c%256;
    if (c == 27) {
        demo_done = 1;
        return 0;
    } else if (c == 82) {
        demo_thresh += .02;
    } else if (c == 84) {
        demo_thresh -= .02;
        if(demo_thresh <= .02) demo_thresh = .02;
    } else if (c == 83) {
        demo_hier += .02;
    } else if (c == 81) {
        demo_hier -= .02;
        if(demo_hier <= .0) demo_hier = .0;
    }
	printf("end display thread ...\n");
    return 0;
}

void *display_loop(void *ptr) {
    while(1){
        display_in_thread(0);
    }
}

void *detect_loop(void *ptr) {
    while(1) {
        detect_in_thread(0);
    }
}

void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names,
          int classes, int delay, char *prefix, int avg_frames, float hier, int w, int h, int frames, int fullscreen) {
    //demo_frame = avg_frames;
    image **alphabet = load_alphabet();
    demo_names = names;
    demo_alphabet = alphabet;
    demo_classes = classes;
    demo_thresh = thresh;
    demo_hier = hier;
    printf("Demo\n");
    net = load_network(cfgfile, weightfile, 0);
    set_batch_network(net, 1);
    pthread_t detect_thread;
    pthread_t fetch_thread;

    srand(2222222);

    int i;
    demo_total = size_network(net);
    predictions = calloc(demo_frame, sizeof(float*));
    for (i=0; i<demo_frame; ++i) {
        predictions[i] = calloc(demo_total, sizeof(float));
    }
    avg = calloc(demo_total, sizeof(float));

    if(filename) {
        printf("video file: %s\n", filename);
        cap = open_video_stream(filename, 0, 0, 0, 0);
    } else {
        cap = open_video_stream(0, cam_index, w, h, frames);
    }

    if(!cap) error("Couldn't connect to webcam.\n");

    buff[0] = get_image_from_stream(cap);
    buff[1] = copy_image(buff[0]);
    buff[2] = copy_image(buff[0]);
    buff_letter[0] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[1] = letterbox_image(buff[0], net->w, net->h);
    buff_letter[2] = letterbox_image(buff[0], net->w, net->h);

    //accept signal intrrupt
    signal(SIGINT, sig_int);

    int count = 0;
    if(!prefix) {
        make_window("Demo", 1352, 1013, fullscreen);
    } else {
        CvSize size;
        size.width = buff[0].w;
        size.height = buff[0].h;
        video_writer = cvCreateVideoWriter(strcat(prefix,".avi"), CV_FOURCC('D', 'I', 'V', 'X'), 25, size, 1);
    }

    while(!demo_done) {
        buff_index = (buff_index + 1) %3;
        if(pthread_create(&fetch_thread, 0, fetch_in_thread, 0)) error("Thread creation failed");
        pthread_join(fetch_thread, 0);
        demo_time = what_time_is_it_now();
        if(pthread_create(&detect_thread, 0, detect_in_thread, 0)) error("Thread creation failed");
        if(!prefix) {
            pthread_join(detect_thread, 0);
            fps = 1./(what_time_is_it_now() - demo_time);
            display_in_thread(0);
        } else {
            pthread_join(detect_thread, 0);
            printf("save image buff[%d] to video \n", (buff_index+1)%3);
            cvWriteFrame(video_writer, image_to_ipl(buff[(buff_index+1)%3]));
            //cvReleaseImage(&buff[(buff_index+1)%3]);
        }
        ++count;
    }

    if(video_writer) {
        cvReleaseVideoWriter(&video_writer);
        printf("video_writer closed\n");
    }
}

#else
void demo(char *cfgfile, char *weightfile, float thresh, int cam_index, const char *filename, char **names, 
          int classes, int delay, char *prefix, int avg, float hier, int w, int h, int frames, int fullscreen) {
    fprintf(stderr, "Demo needs OpenCV for webcam images.\n");
}
#endif
