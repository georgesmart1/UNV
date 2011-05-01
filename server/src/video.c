//
//      (C) 30/Apr/2011 - George Smart, M1GEO <george.smart@ucl.ac.uk>
//		The UNV Project, Electronic Enginering, University College London
//

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h> 
#include <libswscale/swscale.h>
#include "util.h"
#include "rtspServer.h"

AVFormatContext		*pWebcamFormatContext;
AVCodecContext		*pWebcamCodecContext;
AVCodec				*pWebcamCodec;
AVFrame				*pFrameDec;		// YUYV422
AVPacket			pWebcamPacket;
int					iVideoStream=-1;

void setup_ffmpeg() {
	av_register_all();
    avdevice_register_all();
}

AVFrame * get_webcam_frame()
{
	int				iFrameFinished=0;
	int 			temp;

	pFrameDec = avcodec_alloc_frame();
	if ( (pFrameDec == NULL) ) {
		printf("couldn't allocate either pFrame of pFrameDec\n");
		exit(EXIT_FAILURE);
	}
	
	do {
		temp = av_read_frame(pWebcamFormatContext, &pWebcamPacket);
		if (temp < 0) {printf("av_read_frame(webcam) failed. Exiting..."); exit(EXIT_FAILURE);} // error or EOF
		if(pWebcamPacket.stream_index==iVideoStream) {
			temp = avcodec_decode_video2(pWebcamCodecContext, pFrameDec, &iFrameFinished, &pWebcamPacket);
			if (temp < 0) {printf("avcodec_decode_video2(webcam) failed. Exiting..."); exit(EXIT_FAILURE);} // error
		}
	av_free_packet(&pWebcamPacket);
	} while (!iFrameFinished);	
	
	return (pFrameDec);
}

void write_video_frame(AVFormatContext *oc, AVStream *st)
{
    int out_size;
    AVCodecContext *c;
    static struct SwsContext *img_convert_ctx;

    c = st->codec;

        /* no more frame to compress. The codec has a latency of a few
           frames if using B frames, so we get the last frames by
           passing the same picture again */
        if (c->pix_fmt != pWebcamCodecContext->pix_fmt) {  
            /* as we only generate a YUV420P picture, we must convert it
               to the codec pixel format if needed */
            if (img_convert_ctx == NULL) {
                img_convert_ctx = sws_getContext(c->width, c->height,
                                                 pWebcamCodecContext->pix_fmt,  
                                                 c->width, c->height,
                                                 c->pix_fmt,
                                                 sws_flags, NULL, NULL, NULL);
                if (img_convert_ctx == NULL) {
                    fprintf(stderr, "Cannot initialize the conversion context\n");
                    exit(1);
                }
            }
            tmp_picture=get_webcam_frame();
            sws_scale(img_convert_ctx, (const uint8_t* const*) tmp_picture->data, tmp_picture->linesize, 0, c->height, picture->data, picture->linesize);
        } else {
            picture=get_webcam_frame();
        }
    
    /* encode the image */
    out_size = avcodec_encode_video(c, video_outbuf, video_outbuf_size, picture);
    /* if zero size, it means the image was buffered */
    if (out_size > 0) {
        AVPacket pkt;
        av_init_packet(&pkt);
        if (c->coded_frame->pts != (int)AV_NOPTS_VALUE)
            pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        if(c->coded_frame->key_frame)
            pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index= st->index;
        pkt.data= video_outbuf;
        pkt.size= out_size;
		iPktSize= out_size;
        /* write the compressed frame in the media file */
		if (av_interleaved_write_frame(oc, &pkt) != 0) {
			fprintf(stderr, "Error while writing video frame\n");
			exit(1);
		}
	}
    frame_count++;
}

void close_video(AVFormatContext *oc, AVStream *st)
{
    avcodec_close(st->codec);
    av_freep(picture); // This is the *RIGHT* way to free these see: http://wiki.aasimon.org/doku.php?id=ffmpeg:ffmpeg AHB
    if (tmp_picture) {
        av_free(tmp_picture); 
    }
    av_free(video_outbuf);
}

void open_webcam()
{
	int temp = -1;
	AVInputFormat		*pWebcamInputFormat;
	
/*	UNUSED AT THE MOMENT
	AVFormatParameters	WebcamFormatParams;
	WebcamFormatParams.channel = 0;
	WebcamFormatParams.standard = "pal";
	WebcamFormatParams.width = 640;
	WebcamFormatParams.height = 480;
	WebcamFormatParams.time_base.den = 15;
	WebcamFormatParams.time_base.num = 1;
*/	
	
	pWebcamInputFormat = av_find_input_format("video4linux2");

	temp = av_open_input_file(&pWebcamFormatContext, cliOpts.url, pWebcamInputFormat, 0, NULL);
	if(temp !=0) {
		printf("Couldn't open webcam\n");
		exit(EXIT_FAILURE);
	}
	
	temp = av_find_stream_info(pWebcamFormatContext);
	if(temp<0) {
		printf("couldn't find stream infomation\n");
		exit(EXIT_FAILURE);
	}

	iVideoStream=-1;
	for(temp=0; temp<(int)pWebcamFormatContext->nb_streams; temp++) {
		if(pWebcamFormatContext->streams[temp]->codec->codec_type==CODEC_TYPE_VIDEO) {
			iVideoStream=temp;
			break;
		}
	}
	if(iVideoStream == -1) {
		printf("couldn't select video stream\n");
		exit(EXIT_FAILURE);
	}
	
	pWebcamCodecContext=pWebcamFormatContext->streams[iVideoStream]->codec; 
	pWebcamCodec=avcodec_find_decoder(pWebcamCodecContext->codec_id);
	if(pWebcamCodec==NULL) {
		printf("couldn't find required codec");
	}
	
	temp = avcodec_open(pWebcamCodecContext, pWebcamCodec);
	if(temp<0) {
		printf("couldn't open decoder codec\n");
	}
	
	printf("WebCam Init Complete : %s format\n", PIX_FMT_LOOKUP(pWebcamCodecContext->pix_fmt));
}

void close_webcam()
{
	av_free(pFrameDec);
}

void open_video(AVFormatContext *oc, AVStream *st)
{
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    /* open the codec */
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        exit(1);
    }

    video_outbuf = NULL;
    if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
        /* allocate output buffer */
        /* XXX: API change will be done */
        /* buffers passed into lav* can be allocated any way you prefer,
           as long as they're aligned enough for the architecture, and
           they're freed appropriately (such as using av_free for buffers
           allocated with av_malloc) */
        video_outbuf_size = 200000;
        video_outbuf = (uint8_t*)av_malloc(video_outbuf_size);
    }

    /* allocate the encoded raw picture */
    picture = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!picture) {
        fprintf(stderr, "Could not allocate picture\n");
        exit(1);
    }

    /* if the output format is not YUV420P, then a temporary YUV420P
       picture is needed too. It is then converted to the required
       output format */
    tmp_picture = NULL;
    if (c->pix_fmt != PIX_FMT_YUV420P) {
        tmp_picture = alloc_picture(PIX_FMT_YUV420P, c->width, c->height);
        if (!tmp_picture) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
}

/* add a video output stream */
AVStream *add_video_stream(AVFormatContext *oc, enum CodecID codec_id)
{
    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = 640;
    c->height = 480;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = STREAM_FRAME_RATE;
    c->time_base.num = 1;
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = STREAM_PIX_FMT;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == CODEC_ID_MPEG1VIDEO){
        /* Needed to avoid using macroblocks in which some coeffs overflow.
           This does not happen with normal video, it just happens here as
           the motion of the chroma plane does not match the luma plane. */
        c->mb_decision=2;
    }
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

