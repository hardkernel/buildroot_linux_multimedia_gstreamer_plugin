/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2015 U-AMLOGICTao.Guo <<user@hostname.org>>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-amlvdec
 *
 * FIXME:Describe amlvdec here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! amlvdec ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <gst/gst.h>
#include <stdio.h>
#include "gstamlvdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_aml_vdec_debug);
#define GST_CAT_DEFAULT gst_aml_vdec_debug
#define VERSION	"1.1"

#define COMMON_VIDEO_CAPS \
  "width = (int) [ 16, 4096 ], " \
  "height = (int) [ 16, 4096 ] "

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "video/mpeg, "
        "mpegversion = (int) { 1, 2, 4 }, "
        "systemstream = (boolean) false, "
        COMMON_VIDEO_CAPS "; "
        "video/x-h264, "
        "stream-format={ byte-stream }, "
        "alignment={ nal }, "
        COMMON_VIDEO_CAPS "; "
        "video/x-h265, "
        "stream-format={ byte-stream }, "
        "alignment={ nal };"
        "video/x-vp9, "
        COMMON_VIDEO_CAPS "; "
        "video/x-cavs;"
//	        "video/x-huffyuv, "
//	        COMMON_VIDEO_CAPS "; "
//	        "video/x-dv, "
//	        COMMON_VIDEO_CAPS "; "
        "video/x-flash-video, "
        COMMON_VIDEO_CAPS "; "
        "video/x-h263, "
        COMMON_VIDEO_CAPS "; "
        "image/jpeg, "
        COMMON_VIDEO_CAPS "; "
//	        "video/x-theora; "
//	        "video/x-dirac, "
//	        COMMON_VIDEO_CAPS "; "
//	        "video/x-pn-realvideo, "
//	        "rmversion = (int) [1, 4], "
//	        COMMON_VIDEO_CAPS "; "
//	        "video/x-vp8, "
//	        COMMON_VIDEO_CAPS "; "
//	        "video/x-vp9, "
//	        COMMON_VIDEO_CAPS "; "
//	        "video/x-raw, "
//	        "format = (string) { YUY2, I420, YV12, UYVY, AYUV, GRAY8, BGR, RGB }, "
//	        COMMON_VIDEO_CAPS "; "
        "video/x-pn-realvideo; "
        "video/x-wmv, "
        "wmvversion = (int) { 1, 3 }, "
        COMMON_VIDEO_CAPS)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420"))
    );

static void					gst_aml_vdec_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void					gst_aml_vdec_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean					gst_aml_vdec_open(GstVideoDecoder * dec);
static gboolean					gst_aml_vdec_close(GstVideoDecoder * dec);
static gboolean					gst_aml_vdec_start(GstVideoDecoder * dec);
static gboolean					gst_aml_vdec_stop(GstVideoDecoder * dec);
static gboolean					gst_aml_vdec_set_format(GstVideoDecoder *dec, GstVideoCodecState *state);
static GstFlowReturn			gst_aml_vdec_handle_frame(GstVideoDecoder *dec, GstVideoCodecFrame *frame);
static void					gst_aml_vdec_flush(GstVideoDecoder * dec);
static gboolean					gst_aml_vdec_sink_event  (GstVideoDecoder * amlvdec, GstEvent * event);
static gboolean					gst_set_vstream_info(GstAmlVdec *amlvdec, GstCaps * caps);
static GstFlowReturn			gst_aml_vdec_decode (GstAmlVdec *amlvdec, GstBuffer * buf, GstVideoCodecFrame *frame);
static GstStateChangeReturn		gst_aml_vdec_change_state (GstElement * element, GstStateChange transition);

#define gst_aml_vdec_parent_class parent_class
G_DEFINE_TYPE (GstAmlVdec, gst_aml_vdec, GST_TYPE_VIDEO_DECODER);

/* GObject vmethod implementations */

/* initialize the amlvdec's class */
static void
gst_aml_vdec_class_init (GstAmlVdecClass * klass)
{
	GObjectClass *gobject_class = (GObjectClass *) klass;
	GstElementClass *element_class = (GstElementClass *) klass;
	GstVideoDecoderClass *base_class = (GstVideoDecoderClass *) klass;
	gobject_class->set_property = gst_aml_vdec_set_property;
	gobject_class->get_property = gst_aml_vdec_get_property;
	element_class->change_state = GST_DEBUG_FUNCPTR (gst_aml_vdec_change_state);
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_factory));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_factory));

	gst_element_class_set_details_simple(element_class,
			"Amlogic Video Decoder",
			"Codec/Decoder/Video",
			"Amlogic Video Decoder Plugin",
			"mm@amlogic.com");

	base_class->open = GST_DEBUG_FUNCPTR(gst_aml_vdec_open);
	base_class->close = GST_DEBUG_FUNCPTR(gst_aml_vdec_close);
	base_class->start = GST_DEBUG_FUNCPTR(gst_aml_vdec_start);
	base_class->stop = GST_DEBUG_FUNCPTR(gst_aml_vdec_stop);
	base_class->set_format = GST_DEBUG_FUNCPTR(gst_aml_vdec_set_format);
	base_class->handle_frame = GST_DEBUG_FUNCPTR(gst_aml_vdec_handle_frame);
	base_class->flush = GST_DEBUG_FUNCPTR(gst_aml_vdec_flush);
	base_class->sink_event =  GST_DEBUG_FUNCPTR(gst_aml_vdec_sink_event);

}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_aml_vdec_init (GstAmlVdec * amlvdec)
{
	GstVideoDecoder *dec = GST_VIDEO_DECODER (amlvdec);
}

static void
gst_aml_vdec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(object);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
gst_aml_vdec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(object);

	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean
gst_aml_vdec_open(GstVideoDecoder * dec)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);

	amlvdec->pcodec = g_malloc(sizeof(codec_para_t));
	memset(amlvdec->pcodec, 0, sizeof(codec_para_t));

	set_tsync_enable(0);
	set_tsync_mode(TSYNC_MODE_PCRSCR);

	return TRUE;
}

static void
unref_frame (gpointer data)
{
	GstVideoCodecFrame *frame = (GstVideoCodecFrame *) data;
	gst_video_codec_frame_unref(frame);
}
static void
gst_amlvdec_polling_eos (GstAmlVdec *amlvdec)
{
	static int last_pts = -1;
	static int stop_count = 5;
	static int delay = 0;
	unsigned long pts;

	usleep(1000 * 100);

	pts = codec_get_vpts(amlvdec->pcodec);
	if (amlvdec->last_checkin_pts != -1L && pts != -1L && pts != 1) {
		if (last_pts != pts) {
			last_pts = pts;
			stop_count = delay + 5;
		} else if (!amlvdec->is_paused && amlvdec->is_eos) {
			stop_count--;
		}
		GST_INFO_OBJECT(amlvdec, "TESTV %ld %ld %d %d %d", pts,
				amlvdec->last_checkin_pts, amlvdec->is_eos, amlvdec->is_paused,
				stop_count);
		if ((pts > amlvdec->last_checkin_pts && amlvdec->is_eos)
				|| (amlvdec->is_eos && !amlvdec->is_paused && stop_count < 0)) {
			GST_VIDEO_DECODER_CLASS(parent_class)->sink_event(amlvdec,
					gst_event_new_eos());
			gst_task_pause(amlvdec->eos_task);
		}
	} else if (pts == 1) {
		delay++;
	}
}

static void
start_eos_task (GstAmlVdec *amlvdec)
{
	if (!amlvdec->eos_task) {
		amlvdec->eos_task = gst_task_new(
				(GstTaskFunction) gst_amlvdec_polling_eos, amlvdec, NULL);
		gst_task_set_lock(amlvdec->eos_task, &amlvdec->eos_lock);
	}
	gst_task_start(amlvdec->eos_task);
}

static void
stop_eos_task (GstAmlVdec *amlvdec)
{
	if (!amlvdec->eos_task)
		return;
	gst_task_stop(amlvdec->eos_task);
	gst_task_join(amlvdec->eos_task);
	gst_object_unref(amlvdec->eos_task);
	amlvdec->eos_task = NULL;
}
#define MAXRATE 2
static double vrate=1.0;
static void
gst_voption_rate (double * rate,codec_para_t *pcodec)
{
	char *srate = NULL;
	srate=getenv("media_gst_rate");
	if (!srate)
	{
		return;
	}
	*rate = (double )(atof(srate));

	if (*rate != 1.0 && *rate > 0 && *rate <= MAXRATE)
	{
	    vrate =*rate;
		pcodec->am_sysinfo.rate /= *rate;
		printf("playback rate video rate -> %d vrate %f\n", pcodec->am_sysinfo.rate,vrate);
	}
	return ;
}

static gboolean
gst_aml_vdec_close(GstVideoDecoder * dec)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);
	gint ret = 0;
	stop_eos_task(amlvdec);
	if (amlvdec->codec_init_ok) {
		amlvdec->codec_init_ok = 0;
		if (amlvdec->is_paused == TRUE) {
			ret = codec_resume(amlvdec->pcodec);
			if (ret != 0) {
				GST_ERROR("resume failed!ret=%d", ret);
			} else {
				amlvdec->is_paused = FALSE;
			}
		}
		set_black_policy(1);
		codec_close(amlvdec->pcodec);

		amlvdec->is_headerfeed = FALSE;
		if (amlvdec->input_state) {
			gst_video_codec_state_unref(amlvdec->input_state);
			amlvdec->input_state = NULL;
		}

	}
	if (amlvdec->info) {
		amlvdec->info->finalize(amlvdec->info);
		amlvdec->info = NULL;
	}

	if (amlvdec->pcodec) {
		g_free(amlvdec->pcodec);
		amlvdec->pcodec = NULL;
	}

	if (amlvdec->list) {
		g_list_free_full(amlvdec->list, unref_frame);
		amlvdec->list = NULL;
	}

	return TRUE;
}
static gboolean
gst_aml_vdec_start(GstVideoDecoder * dec)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);

	amlvdec->pcodec->has_video = 1;
	amlvdec->pcodec->am_sysinfo.rate = 0;
	amlvdec->pcodec->am_sysinfo.height = 0;
	amlvdec->pcodec->am_sysinfo.width = 0;
	amlvdec->pcodec->has_audio = 0;
	amlvdec->pcodec->noblock = 0;
	amlvdec->pcodec->stream_type = STREAM_TYPE_ES_VIDEO;
	amlvdec->is_headerfeed = FALSE;
	amlvdec->is_paused = FALSE;
	amlvdec->is_eos = FALSE;
	amlvdec->codec_init_ok = 0;
	amlvdec->trickRate = 1.0;
	amlvdec->segment.rate = 1.0;
	amlvdec->list = NULL;
	vrate=1.0;
	amsysfs_set_sysfs_str("/sys/class/vfm/map", "rm default");
	amsysfs_set_sysfs_str("/sys/class/vfm/map", "add default decoder ppmgr deinterlace amvideo");
	return TRUE;
}

static gboolean
gst_aml_vdec_stop(GstVideoDecoder * dec)
{
	gboolean ret = TRUE;
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);
	GST_DEBUG_OBJECT(amlvdec, "stop amlvdec");
	if (amlvdec->is_paused == TRUE && amlvdec->codec_init_ok) {
#if 0
		ret = codec_resume(amlvdec->pcodec);
		if (ret != 0) {
			GST_ERROR_OBJECT(amlvdec, "resume failed!ret=%d", ret);
		} else {
			amlvdec->is_paused = FALSE;
		}
#endif
	}

	return ret;
}
static gboolean gst_aml_vdec_set_format(GstVideoDecoder *dec, GstVideoCodecState *state)
{
	gboolean ret = FALSE;
	GstStructure *structure;
	const char *name;
	GstVideoInfo *info;
	gint par_num, par_den;
	GstVideoFormat fmt;
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);

	g_return_val_if_fail(state != NULL, FALSE);
	if (amlvdec->input_state)
		gst_video_codec_state_unref(amlvdec->input_state);
	amlvdec->input_state = gst_video_codec_state_ref(state);

	structure = gst_caps_get_structure(state->caps, 0);
	name = gst_structure_get_name(structure);
	GST_INFO_OBJECT(amlvdec, "format = %s", name);
	if (name) {
		ret = gst_set_vstream_info(amlvdec, state->caps);
		if (!amlvdec->output_state && amlvdec->pcodec->am_sysinfo.width
				&& amlvdec->pcodec->am_sysinfo.height) {
			info = &amlvdec->input_state->info;
			fmt = GST_VIDEO_FORMAT_I420;//GST_VIDEO_FORMAT_xRGB;
			GST_VIDEO_INFO_WIDTH (info) = amlvdec->pcodec->am_sysinfo.width;
			GST_VIDEO_INFO_HEIGHT (info) = amlvdec->pcodec->am_sysinfo.height;
			par_num = GST_VIDEO_INFO_PAR_N(info);
			par_den = GST_VIDEO_INFO_PAR_D(info);
			amlvdec->output_state = gst_video_decoder_set_output_state(GST_VIDEO_DECODER(amlvdec),
					fmt, info->width,
					info->height,
					amlvdec->input_state);
			gst_video_decoder_negotiate (GST_VIDEO_DECODER (amlvdec));
		}

	}
	return ret;
}

static GstFlowReturn
gst_aml_vdec_handle_frame(GstVideoDecoder *dec, GstVideoCodecFrame *frame)
{
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);
	GstBuffer *outbuffer;
	GstFlowReturn ret;
	GstVideoCodecState *state;
	GSList *l;
	GstVideoCodecFrame *p = frame;


	if (G_UNLIKELY(!frame)) {
		return GST_FLOW_OK;
	}

	GST_DEBUG_OBJECT(amlvdec, "handle frame %p, %llu, %d", frame, frame->pts, g_slist_length(amlvdec->list));

	amlvdec->list = g_slist_append(amlvdec->list, (gpointer) frame);

	if (!amlvdec->codec_init_ok) {
		GST_INFO_OBJECT(amlvdec, "decode frame later");
		return GST_FLOW_OK;
	}

	l = amlvdec->list;
	while (l) {
		GSList *node = l;
		l = g_slist_next(l);
		if (node->data) {
			p = (GstVideoCodecFrame *) (node->data);
			ret = gst_video_decoder_allocate_output_frame(dec, p);
			if (G_UNLIKELY(ret != GST_FLOW_OK)) {
				GST_ERROR_OBJECT(amlvdec, "failed to allocate output frame");
				gst_video_codec_frame_unref(p);
			} else {
				gst_aml_vdec_decode(amlvdec, p->input_buffer, p);
				GST_BUFFER_FLAG_SET(p->output_buffer, (1 << 16));   //set flag to avoid use yuvplayer
				gst_video_decoder_finish_frame(dec, p);
			}
		}

		amlvdec->list = g_slist_remove_link(amlvdec->list, node);
		g_slist_free_1(node);
	}

	return GST_FLOW_OK;
}

static gboolean
gst_aml_vdec_sink_event  (GstVideoDecoder * dec, GstEvent * event)
{
	gboolean ret = TRUE;
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);
	GST_DEBUG_OBJECT(amlvdec, "Got %s event on sink pad",
			GST_EVENT_TYPE_NAME(event));
	switch (GST_EVENT_TYPE(event)) {

	case GST_EVENT_SEGMENT: {
		gst_event_copy_segment(event, &amlvdec->segment);
		GST_INFO_OBJECT(amlvdec, "rate = %f", amlvdec->segment.rate);
		ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_event(amlvdec,
				event);
		break;
	}
#if 0
	case GST_EVENT_FLUSH_START:

		if (amlvdec->codec_init_ok) {
			set_black_policy(0);
		}
		ret = TRUE;
		break;

	case GST_EVENT_FLUSH_STOP: {
		stop_eos_task(amlvdec);
		if (amlvdec->codec_init_ok) {
			gint res = -1;
			res = codec_reset(amlvdec->pcodec);
			if (res < 0) {
				GST_ERROR("reset vcodec failed, res= %x", res);
				return FALSE;
			}
			amlvdec->is_headerfeed = FALSE;
		}
		GST_WARNING("vformat:%d", amlvdec->pcodec->video_type);
		break;
	}
#endif
	case GST_EVENT_EOS:
		GST_WARNING("get GST_EVENT_EOS,check for video end");
		if (amlvdec->codec_init_ok) {
			gst_task_start(amlvdec->eos_task);
			amlvdec->is_eos = TRUE;
			ret = FALSE;
		} else {
			ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_event(amlvdec,
					event);
		}

		break;

	default:
		ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_event(amlvdec,
				event);
		break;
	}

	return ret;
}

static GstStateChangeReturn
gst_aml_vdec_change_state (GstElement * element, GstStateChange transition)
{
	GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;
	gint ret = 0;
	GstAmlVdec *amlvdec = GST_AMLVDEC(element);
	g_return_val_if_fail(GST_IS_AMLVDEC(element), GST_STATE_CHANGE_FAILURE);
	switch (transition) {
#if 0
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_NULL_TO_READY");
		break;

	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_READY_TO_PAUSED");
		break;
#endif
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		if (amlvdec->is_paused == TRUE && amlvdec->codec_init_ok) {
			ret = codec_resume(amlvdec->pcodec);
			if (ret != 0) {
				GST_ERROR_OBJECT(amlvdec, "resume failed!ret=%d", ret);
			} else {
				amlvdec->is_paused = FALSE;
			}
		}
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
		break;

	default:
		break;
	}
	result = GST_ELEMENT_CLASS (parent_class)->change_state(element,
			transition);

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		if (!amlvdec->is_eos && amlvdec->codec_init_ok) {
			ret = codec_pause(amlvdec->pcodec);
			if (ret != 0) {
				GST_ERROR_OBJECT(amlvdec, "pause failed!ret=%d", ret);
			} else {
				amlvdec->is_paused = TRUE;
			}
		}
		break;
#if 0
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_PAUSED_TO_READY");
		break;

	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_INFO_OBJECT(amlvdec, "GST_STATE_CHANGE_READY_TO_NULL");
		break;
#endif
	default:
		break;
	}
	GST_DEBUG_OBJECT(amlvdec, "ret=%d", result);
	return result;
}


static void
gst_aml_vdec_flush(GstVideoDecoder * dec)
{
	int ret;
	GSList *l;
	GstVideoCodecFrame *frame;
	GstAmlVdec *amlvdec = GST_AMLVDEC(dec);
	GST_WARNING_OBJECT(amlvdec, "flush");

	if (amlvdec->codec_init_ok) {
		unsigned long pts;
		pts = codec_get_vpts(amlvdec->pcodec);
		if (pts != -1L && pts != 0 && !amlvdec->is_paused
				&& amlvdec->segment.rate > 0.0) {
			if (amlvdec->list) {
				g_list_free_full(amlvdec->list, unref_frame);
				amlvdec->list = NULL;
			}
			set_black_policy(0);
			gst_task_pause(amlvdec->eos_task);
			ret = codec_reset(amlvdec->pcodec);
			if (ret < 0) {
				GST_ERROR("reset acodec failed, ret=%x", ret);
			} else {
				amlvdec->is_headerfeed = FALSE;
			}
			amlvdec->is_eos = FALSE;
			amlvdec->last_checkin_pts = -1L;
			gst_task_start(amlvdec->eos_task);
		}
	}
	
	if (get_black_policy() != 1) {
        	set_black_policy(1);
        }
}

static gboolean
gst_set_vstream_info(GstAmlVdec *amlvdec, GstCaps * caps)
{
	GstStructure *structure;
	const char *name;
	gint32 ret = CODEC_ERROR_NONE;
	AmlStreamInfo *videoinfo = NULL;
	int coordinate[4] = {0, 0, 0, 0};
	float rate =1.0;
	structure = gst_caps_get_structure(caps, 0);
	name = gst_structure_get_name(structure);

	videoinfo = amlVstreamInfoInterface(name);
	if (NULL == name) {
		return FALSE;
	}
	amlvdec->info = videoinfo;
	if (0 != videoinfo->init(videoinfo, amlvdec->pcodec, structure)) {
		return FALSE;
	}
	if (0 == amlvdec->pcodec->am_sysinfo.width || 0 == amlvdec->pcodec->am_sysinfo.height || 0 == amlvdec->pcodec->am_sysinfo.rate) {
		return TRUE;
	}
	if (amlvdec->pcodec && amlvdec->pcodec->stream_type == STREAM_TYPE_ES_VIDEO) {
		if (!amlvdec->codec_init_ok) {
			int tsync_mode;
			//amlvdec->pcodec->vbuf_size = 0xf20000;
			gst_voption_rate(&rate,amlvdec->pcodec);
			ret = codec_init(amlvdec->pcodec);
			if (ret != CODEC_ERROR_NONE) {
				GST_ERROR("codec init failed, ret=-0x%x", -ret);
				return FALSE;
			}

			tsync_mode = get_tsync_mode();
			if (tsync_mode == TSYNC_MODE_AUDIO) {
				set_tsync_enable(1);
			} else {
				set_tsync_mode(TSYNC_MODE_VIDEO);
			}

			codec_set_pcrscr(amlvdec->pcodec, 0);
			amlvdec->codec_init_ok = 1;
			if (amlvdec->trickRate > 0) {
				if (amlvdec->pcodec && amlvdec->pcodec->cntl_handle) {
//T					codec_set_video_playrate(amlvdec->pcodec, (int) (amlvdec->trickRate * (1 << 16)));
				}
			}
			start_eos_task(amlvdec);
			GST_DEBUG_OBJECT(amlvdec, "pcodec: video codec_init ok");
		}

	}
	return TRUE;
}

static GstFlowReturn
gst_aml_vdec_decode (GstAmlVdec *amlvdec, GstBuffer * buf, GstVideoCodecFrame *frame)
{
	GstFlowReturn ret = GST_FLOW_OK;
	guint8 *data;
	guint size;
	gint written;
	GstClockTime timestamp = GST_CLOCK_TIME_NONE, pts;

	struct buf_status vbuf;
	GstMapInfo map;

	if (amlvdec->pcodec && amlvdec->codec_init_ok) {
		while (codec_get_vbuf_state(amlvdec->pcodec, &vbuf) == 0) {
			if (vbuf.data_len * 10 < vbuf.size * 7) {
				break;
			}
			if (amlvdec->is_paused) {
				break;
			}
			usleep(20000);
		}
		/*
		if (GST_BUFFER_PTS_IS_VALID(buf))
			timestamp = GST_BUFFER_PTS(buf);
		else if (GST_BUFFER_DTS_IS_VALID(buf))
			timestamp = GST_BUFFER_DTS(buf);
			*/
		timestamp = frame->pts;
		pts = timestamp * 9LL / 100000LL + 1L;
		if (vrate != 1 && vrate > 0 && vrate <= MAXRATE)
		{
			 pts = pts/vrate;
			 GST_INFO_OBJECT(amlvdec, "setpts for playback rate\n");
		}
		if (timestamp != GST_CLOCK_TIME_NONE) {
			if (amlvdec->segment.rate < 0.0) {
				pts = ~pts;
			}
			GST_INFO_OBJECT(amlvdec, " video pts = %x", (unsigned long) pts);
			if (codec_checkin_pts(amlvdec->pcodec, (unsigned long) pts) != 0) {
				GST_ERROR_OBJECT(amlvdec, "pts checkin flied maybe lose sync");
			} else {
				amlvdec->last_checkin_pts = pts;
			}
		}

		if (!amlvdec->is_headerfeed) {
			if (amlvdec->info->writeheader) {
				amlvdec->info->writeheader(amlvdec->info, amlvdec->pcodec);
			}
			amlvdec->is_headerfeed = TRUE;
		}
		if (amlvdec->info->add_startcode) {
			amlvdec->info->add_startcode(amlvdec->info, amlvdec->pcodec, buf);
		}
		gst_buffer_map(buf, &map, GST_MAP_READ);
		data = map.data;
		size = map.size;
#if 0
		FILE *fp2= fopen("/mnt/codec.data","a+");
		if (fp2) {
			int flen=fwrite(data,1,size,fp2);
			fclose(fp2);
		} else {
			g_print("could not open file:codec.data");
		}
#endif
		while (size > 0) {
			written = codec_write(amlvdec->pcodec, data, size);
			if (written >= 0) {
				size -= written;
				data += written;
			} else if (errno == EAGAIN || errno == EINTR) {
				GST_WARNING_OBJECT(amlvdec, "codec_write busy");
				if (amlvdec->is_paused) {
					break;
				}
				usleep(20000);
			} else {
				GST_ERROR_OBJECT(amlvdec, "codec_write failed");
				ret = GST_FLOW_ERROR;
				break;
			}
		}
		gst_buffer_unmap(buf, &map);
	}
	return ret;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
amlvdec_init (GstPlugin * amlvdec)
{
	/* debug category for fltering log messages
	 *
	 * exchange the string 'Template amlvdec' with your description
	 */
	GST_DEBUG_CATEGORY_INIT(gst_aml_vdec_debug, "amlvdec", 0, "Amlogic Video Decoder");

	return gst_element_register(amlvdec, "amlvdec", GST_RANK_PRIMARY+1, GST_TYPE_AMLVDEC);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "gst-plugins-amlogic"
#endif

/* gstreamer looks for this structure to register amlvdecs
 *
 * exchange the string 'Template amlvdec' with your amlvdec description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlvdec,
    "Amlogic Video Decoder",
    amlvdec_init,
    VERSION,
    "LGPL",
    "Amlogic",
    "http://amlogic.com/"
)
