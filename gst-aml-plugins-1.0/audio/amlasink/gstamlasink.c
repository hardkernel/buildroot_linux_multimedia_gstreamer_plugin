/* GStreamer
 * Copyright (C) <2003> Laurent Vivier <Laurent.Vivier@bull.net>
 * Copyright (C) <2004> Arwed v. Merkatz <v.merkatz@gmx.net>
 *
 * Based on esdsink.c:
 * Copyright (C) <2001> Richard Boulton <richard-gst@tartarus.org>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio-format.h>
#include <string.h>
#include "gstamlasink.h"
#include "amlutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_aml_asink_debug);
#define GST_CAT_DEFAULT gst_aml_asink_debug
#define VERSION "1.1"

enum
{
  ARG_0,
  ARG_MUTE
};

#define DEFAULT_MUTE  FALSE
#define DEFAULT_HOST  NULL

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
            "format = (string) " GST_AUDIO_NE(S32) ", "
            "layout = (string) interleaved, "
            "rate = " GST_AUDIO_RATE_RANGE ", "
            "channels = " GST_AUDIO_CHANNELS_RANGE)
    );

static void gst_aml_asink_finalize (GObject * object);
static gboolean gst_aml_asink_start (GstBaseSink * asink);
static gboolean gst_aml_asink_stop(GstBaseSink * asink);
static GstFlowReturn gst_aml_asink_render(GstBaseSink * asink, GstBuffer *buffer);
static void gst_aml_asink_set_property(GObject * object, guint prop_id,
        const GValue * value, GParamSpec * pspec);
static void gst_aml_asink_get_property(GObject * object, guint prop_id,
        GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_amlasink_change_state(GstElement * element,
        GstStateChange transition);
static gboolean gst_aml_asink_query(GstElement * element, GstQuery * query);
static gboolean gst_aml_asink_event(GstBaseSink * asink, GstEvent *event);

#define parent_class gst_aml_asink_parent_class
G_DEFINE_TYPE (GstAmlAsink, gst_aml_asink, GST_TYPE_BASE_SINK);


static void
gst_aml_asink_class_init (GstAmlAsinkClass * klass)
{
    GObjectClass *gobject_class;
    GstElementClass *gstelement_class;
    GstBaseSinkClass *gstbasesink_class;

    gobject_class = (GObjectClass *) klass;
    gstelement_class = (GstElementClass *) klass;
    gstbasesink_class = (GstBaseSinkClass *) klass;

    gstelement_class->change_state =
            GST_DEBUG_FUNCPTR(gst_amlasink_change_state);
    gstelement_class->query = GST_DEBUG_FUNCPTR(gst_aml_asink_query);
    gobject_class->set_property = gst_aml_asink_set_property;
    gobject_class->get_property = gst_aml_asink_get_property;
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_aml_asink_finalize);

    g_object_class_install_property(gobject_class, ARG_MUTE,
            g_param_spec_boolean("mute", "mute", "Whether to mute playback",
            DEFAULT_MUTE,
                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT
                            | G_PARAM_STATIC_STRINGS));

    gst_element_class_add_static_pad_template(gstelement_class, &sinktemplate);
    gstbasesink_class->start = GST_DEBUG_FUNCPTR(gst_aml_asink_start);
    gstbasesink_class->stop = GST_DEBUG_FUNCPTR(gst_aml_asink_stop);
    gstbasesink_class->event = GST_DEBUG_FUNCPTR(gst_aml_asink_event);
    gstbasesink_class->render = GST_DEBUG_FUNCPTR(gst_aml_asink_render);

    gst_element_class_set_static_metadata(gstelement_class,
            "Amlogic Fake Audio Sink",
            "Sink/Audio",
            "Amlogic audiosink",
            "mm@amlogic.com>");

}

#define MAXRATE 2
static double aptsrate=1.0;
static void
gst_aoption_ratepts (double * rate)
{
    char *srate = NULL;
    srate=getenv("media_gst_rate");
    if (!srate)
    {
        return;
    }
    *rate = (double )(atof(srate));
    if (*rate != 1.0 && *rate > 0 && *rate <= MAXRATE)
        aptsrate =*rate;
    return ;
}
static void
gst_aml_asink_init (GstAmlAsink * amlasink)
{
    GstBaseSink *bsink = GST_BASE_SINK(amlasink);
    gst_base_sink_set_sync(bsink, FALSE);
    gst_base_sink_set_async_enabled(bsink, FALSE);
    amlasink->segment.rate = 1.0;
    aptsrate = 1.0;
    gst_aoption_ratepts(&aptsrate);
}

static void
gst_aml_asink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
    GstAmlAsink *amlasink;

    amlasink = GST_AMLASINK(object);

    switch (prop_id) {
    case ARG_MUTE:
        amlasink->mute = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void
gst_aml_asink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
    GstAmlAsink *amlasink;

    amlasink = GST_AMLASINK(object);

    switch (prop_id) {
    case ARG_MUTE:
        g_value_set_boolean(value, amlasink->mute);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}
static GstStateChangeReturn
gst_amlasink_change_state (GstElement * element, GstStateChange transition)
{
    GstAmlAsink *amlasink = GST_AMLASINK(element);
    GstStateChangeReturn result;

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_NULL_TO_READY");
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_READY_TO_PAUSED");
        break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
        break;
    default:
        break;
    }

    result = GST_ELEMENT_CLASS (parent_class)->change_state(element,
            transition);

    switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
        break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_PAUSED_TO_READY");
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_INFO_OBJECT(amlasink, "GST_STATE_CHANGE_READY_TO_NULL");
        break;
    default:
        break;
    }
    return result;
}

static void
gst_aml_asink_finalize (GObject * object)
{
    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static GstFlowReturn
gst_aml_asink_render (GstBaseSink * asink, GstBuffer *buffer)
{
    return GST_FLOW_OK;
}


static gboolean
gst_aml_asink_start (GstBaseSink * asink)
{
    GstAmlAsink *amlasink = GST_AMLASINK(asink);
    return TRUE;
}

static gboolean
gst_aml_asink_stop (GstBaseSink * asink)
{
    GstAmlAsink *amlasink = GST_AMLASINK(asink);
    return TRUE;
}


static gboolean
gst_aml_asink_query (GstElement * element, GstQuery * query)
{
    gboolean res = FALSE;
    GstAmlAsink *amlasink = GST_AMLASINK(element);
    switch (GST_QUERY_TYPE(query)) {
    case GST_QUERY_POSITION: {
        GstClockTime cur;
        GstFormat format;
        unsigned long pts = get_sysfs_int("/sys/class/tsync/pts_pcrscr");
        if (pts && pts != 1) {
            if (amlasink->segment.rate < 0.0) {
                pts = ~pts;
            }
            gst_query_parse_position(query, &format, NULL);
            cur = (GstClockTime) pts * 100000LL / 9LL;
            if (aptsrate != 1.0 && aptsrate > 0 && aptsrate <= MAXRATE) {
                cur = cur *aptsrate;
            }
            gst_query_set_position(query, format, cur);
            res = TRUE;
        } else {
            res = FALSE;
        }

        break;
    }
    case GST_EVENT_EOS:
        res = FALSE;
        break;
    default:
        res = GST_ELEMENT_CLASS(parent_class)->query(element, query);
        break;
    }

    return res;
}

static gboolean
gst_aml_asink_event(GstBaseSink * asink, GstEvent *event)
{
    gboolean ret = FALSE;
    GstAmlAsink *amlasink = GST_AMLASINK(asink);
    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_SEGMENT:
        gst_event_copy_segment(event, &amlasink->segment);
        GST_INFO_OBJECT(amlasink, "rate =  %f\n", amlasink->segment.rate);
        ret = GST_BASE_SINK_CLASS(parent_class)->event(asink, event);
        break;
    default:
        ret = GST_BASE_SINK_CLASS(parent_class)->event(asink, event);
        break;
    }

    return ret;
}

static gboolean
amlasink_init (GstPlugin * plugin)
{
    GST_DEBUG_CATEGORY_INIT(gst_aml_asink_debug, "amlasink", 0,
            "Amlogic Fake Audio Sink");
    return gst_element_register(plugin, "amlasink", GST_RANK_PRIMARY,
            GST_TYPE_AMLASINK);
}

#ifndef PACKAGE
#define PACKAGE "gst-plugins-amlogic"
#endif


GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    amlasink,
    "Amlogic Fake Audio Sink",
    amlasink_init,
    VERSION,
    "LGPL",
    "Amlogic",
    "http://amlogic.com/"
);
