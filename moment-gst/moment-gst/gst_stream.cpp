/*  Moment-Gst - GStreamer support module for Moment Video Server
    Copyright (C) 2011-2013 Dmitry Shatrov

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/


#include <moment-gst/gst_stream.h>


using namespace M;
using namespace Moment;

namespace MomentGst {

static LogGroup libMary_logGroup_chains   ("mod_gst.chains",   LogLevel::I);
static LogGroup libMary_logGroup_pipeline ("mod_gst.pipeline", LogLevel::I);
static LogGroup libMary_logGroup_stream   ("mod_gst.stream",   LogLevel::I);
static LogGroup libMary_logGroup_bus      ("mod_gst.bus",      LogLevel::I);
static LogGroup libMary_logGroup_frames   ("mod_gst.frames",   LogLevel::E); // E is the default
static LogGroup libMary_logGroup_novideo  ("mod_gst.novideo",  LogLevel::I);
static LogGroup libMary_logGroup_plug     ("mod_gst.autoplug", LogLevel::I);

void
GstStream::reportStatusEvents ()
{
    deferred_reg.scheduleTask (&deferred_task, false /* permanent */);
}

bool
GstStream::deferredTask (void * const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);
    self->doReportStatusEvents ();
    return false /* do not reschedule */;
}

void
GstStream::workqueueThreadFunc (void * const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    updateTime ();

    logD (pipeline, _self_func_);

    self->mutex.lock ();

    self->tlocal = libMary_getThreadLocal();

    for (;;) {
        if (self->stream_closed) {
            self->mutex.unlock ();
            return;
        }

        while (self->workqueue_list.isEmpty()) {
            self->workqueue_cond.wait (self->mutex);
            updateTime ();

            if (self->stream_closed) {
                self->mutex.unlock ();
                return;
            }
        }

        Ref<WorkqueueItem> const workqueue_item = self->workqueue_list.getFirst();
        self->workqueue_list.remove (self->workqueue_list.getFirstElement());

        self->mutex.unlock ();

        switch (workqueue_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                self->doCreatePipeline ();
                break;
            case WorkqueueItem::ItemType_ReleasePipeline:
                self->doReleasePipeline ();
                break;
            default:
                unreachable ();
        }

        self->mutex.lock ();
    }
}

void
GstStream::createPipelineForChainSpec ()
{
    logD (chains, _func, playback_item->stream_spec);

    assert (playback_item->spec_kind == PlaybackItem::SpecKind::Chain);

    if (!playback_item->stream_spec->mem().len())
	return;

    GstElement *chain_el = NULL;
    GstElement *in_stats_el = NULL;
    GstElement *video_el = NULL;
    GstElement *audio_el = NULL;
    GstElement *mix_video_el = NULL;
    GstElement *mix_audio_el = NULL;

  {
    GError *error = NULL;
    chain_el = gst_parse_launch (playback_item->stream_spec->cstr(), &error);
    if (!chain_el) {
	if (error) {
	    logE_ (_func, "gst_parse_launch() failed: ", error->code,
		   " ", error->message);
	} else {
	    logE_ (_func, "gst_parse_launch() failed");
	}

	mutex.lock ();
	goto _failure;
    }

    mutex.lock ();
    playbin = chain_el;
    gst_object_ref (playbin);

    if (stream_closed)
	goto _failure;

    {
	in_stats_el = gst_bin_get_by_name (GST_BIN (chain_el), "in_stats");
	if (in_stats_el) {
	    GstPad * const pad = gst_element_get_static_pad (in_stats_el, "sink");
	    if (!pad) {
		logE_ (_func, "element called \"in_stats\" doesn't have a \"sink\" "
		       "pad. Chain spec: ", playback_item->stream_spec);
		goto _failure;
	    }

	    got_in_stats = true;

	    gst_pad_add_buffer_probe (pad, G_CALLBACK (GstStream::inStatsDataCb), this);

	    gst_object_unref (pad);

	    gst_object_unref (in_stats_el);
	    in_stats_el = NULL;
	}
    }

    {
	audio_el = gst_bin_get_by_name (GST_BIN (chain_el), "audio");
	if (audio_el) {
	    GstPad * const pad = gst_element_get_static_pad (audio_el, "sink");
	    if (!pad) {
		logE_ (_func, "element called \"audio\" doesn't have a \"sink\" "
		       "pad. Chain spec: ", playback_item->stream_spec);
		goto _failure;
	    }

	    got_audio = true;

	    // TODO Use "handoff" signal
	    audio_probe_id = gst_pad_add_buffer_probe (
		    pad, G_CALLBACK (GstStream::audioDataCb), this);

	    gst_object_unref (pad);

	    gst_object_unref (audio_el);
	    audio_el = NULL;
	} else {
	    logW_ (_func, "chain \"", channel_opts->channel_name, "\" does not contain "
		   "an element named \"audio\". There'll be no audio "
		   "for the stream. Chain spec: ", playback_item->stream_spec);
	}
    }

    {
	video_el = gst_bin_get_by_name (GST_BIN (chain_el), "video");
	if (video_el) {
	    GstPad * const pad = gst_element_get_static_pad (video_el, "sink");
	    if (!pad) {
		logE_ (_func, "element called \"video\" doesn't have a \"sink\" "
		       "pad. Chain spec: ", playback_item->stream_spec);
		goto _failure;
	    }

	    got_video = true;

	    // TODO Use "handoff" signal
	    video_probe_id = gst_pad_add_buffer_probe (
		    pad, G_CALLBACK (GstStream::videoDataCb), this);

	    gst_object_unref (pad);

	    gst_object_unref (video_el);
	    video_el = NULL;
	} else {
	    logW_ (_func, "chain \"", channel_opts->channel_name, "\" does not contain "
		   "an element named \"video\". There'll be no video "
		   "for the stream. Chain spec: ", playback_item->stream_spec);
	}
    }

    mix_audio_el = gst_bin_get_by_name (GST_BIN (chain_el), "mix_audio");
    if (mix_audio_el) {
	mix_audio_src = GST_APP_SRC (mix_audio_el);
	g_object_ref (mix_audio_src);
    }

    mix_video_el = gst_bin_get_by_name (GST_BIN (chain_el), "mix_video");
    if (mix_video_el) {
	mix_video_src = GST_APP_SRC (mix_video_el);
	g_object_ref (mix_video_src);
    }

    logD (chains, _func, "chain \"", channel_opts->channel_name, "\" created");

    if (!mt_unlocks (mutex) setPipelinePlaying ()) {
	mutex.lock ();
	goto _failure;
    }
  }

    goto _return;

mt_mutex (mutex) _failure:
    mt_unlocks (mutex) pipelineCreationFailed ();

_return:
    if (chain_el)
	gst_object_unref (chain_el);
    if (in_stats_el)
	gst_object_unref (in_stats_el);
    if (video_el)
	gst_object_unref (video_el);
    if (audio_el)
	gst_object_unref (audio_el);
    if (mix_video_el)
	gst_object_unref (mix_video_el);
    if (mix_audio_el)
	gst_object_unref (mix_audio_el);
}

void
GstStream::createPipelineForUri ()
{
    logD (pipeline, _this_func_);

    assert (playback_item->spec_kind == PlaybackItem::SpecKind::Uri);

    if (!playback_item->stream_spec->mem().len())
	return;

    GstElement *playbin           = NULL,
	       *audio_encoder_bin = NULL,
	       *video_encoder_bin = NULL,
	       *audio_encoder     = NULL,
	       *video_encoder     = NULL,
	       *fakeaudiosink     = NULL,
	       *fakevideosink     = NULL,
	       *videoscale        = NULL,
	       *audio_capsfilter  = NULL,
	       *video_capsfilter  = NULL;

  {
    playbin = gst_element_factory_make ("playbin2", NULL);
    if (!playbin) {
	logE_ (_func, "gst_element_factory_make() failed (playbin2)");
	mutex.lock ();
	goto _failure;
    }

    mutex.lock ();

    if (stream_closed)
	goto _failure;

    this->playbin = playbin;
    gst_object_ref (this->playbin);

    fakeaudiosink = gst_element_factory_make ("fakesink", NULL);
    if (!fakeaudiosink) {
	logE_ (_func, "gst_element_factory_make() failed (fakeaudiosink)");
	goto _failure;
    }
    g_object_set (G_OBJECT (fakeaudiosink),
		  "sync", TRUE,
		  "signal-handoffs", TRUE, NULL);

    fakevideosink = gst_element_factory_make ("fakesink", NULL);
    if (!fakevideosink) {
	logE_ (_func, "gst_element_factory_make() failed (fakevideosink)");
	goto _failure;
    }
    g_object_set (G_OBJECT (fakevideosink),
		  "sync", TRUE,
		  "signal-handoffs", TRUE, NULL);

#if 0
// Deprecated in favor of "handoff" signal.
    {
	GstPad * const pad = gst_element_get_static_pad (fakeaudiosink, "sink");
	audio_probe_id = gst_pad_add_buffer_probe (
		pad, G_CALLBACK (audioDataCb), this);
	gst_object_unref (pad);
    }

    {
	GstPad * const pad = gst_element_get_static_pad (fakevideosink, "sink");

	video_probe_id = gst_pad_add_buffer_probe (
		pad, G_CALLBACK (videoDataCb), this);

#if 0
	GstCaps * const caps = gst_caps_new_simple ("video/x-raw-yuv", "width", G_TYPE_INT, 64, "height", G_TYPE_INT, 48, NULL);
	gst_pad_set_caps (pad, caps);
	gst_caps_unref (caps);
#endif

	gst_object_unref (pad);
    }
#endif

    g_signal_connect (fakeaudiosink, "handoff", G_CALLBACK (GstStream::handoffAudioDataCb), this);
    g_signal_connect (fakevideosink, "handoff", G_CALLBACK (GstStream::handoffVideoDataCb), this);

    {
      // Audio transcoder.

	audio_encoder_bin = gst_bin_new (NULL);
	if (!audio_encoder_bin) {
	    logE_ (_func, "gst_bin_new() failed (audio_encoder_bin)");
	    goto _failure;
	}

	audio_capsfilter = gst_element_factory_make ("capsfilter", NULL);
	if (!audio_capsfilter) {
	    logE_ (_func, "gst_element_factory_make() failed (audio capsfilter)");
	    goto _failure;
	}
	g_object_set (GST_OBJECT (audio_capsfilter), "caps",
		      gst_caps_new_simple ("audio/x-raw-int",
					   "rate", G_TYPE_INT, 16000,
					   "channels", G_TYPE_INT, 1,
					   NULL), NULL);

//	audio_encoder = gst_element_factory_make ("ffenc_adpcm_swf", NULL);
	audio_encoder = gst_element_factory_make ("speexenc", NULL);
	if (!audio_encoder) {
	    logE_ (_func, "gst_element_factory_make() failed (speexenc)");
	    goto _failure;
	}
//	g_object_set (audio_encoder, "quality", 10, NULL);

	gst_bin_add_many (GST_BIN (audio_encoder_bin), audio_capsfilter, audio_encoder, fakeaudiosink, NULL);
	gst_element_link_many (audio_capsfilter, audio_encoder, fakeaudiosink, NULL);

	{
	    GstPad * const pad = gst_element_get_static_pad (audio_capsfilter, "sink");
	    gst_element_add_pad (audio_encoder_bin, gst_ghost_pad_new ("sink", pad));
	    gst_object_unref (pad);
	}

	audio_capsfilter = NULL;
	audio_encoder = NULL;
	fakeaudiosink = NULL;
    }

    {
      // Transcoder to Sorenson h.263.

	video_encoder_bin = gst_bin_new (NULL);
	if (!video_encoder_bin) {
	    logE_ (_func, "gst_bin_new() failed (video_encoder_bin)");
	    goto _failure;
	}

	videoscale = gst_element_factory_make ("videoscale", NULL);
	if (!videoscale) {
	    logE_ (_func, "gst_element_factory_make() failed (videoscale)");
	    goto _failure;
	}
	g_object_set (G_OBJECT (videoscale), "add-borders", TRUE, NULL);

	video_capsfilter = gst_element_factory_make ("capsfilter", NULL);
	if (!video_capsfilter) {
	    logE_ (_func, "gst_element_factory_make() failed (video capsfilter)");
	    goto _failure;
	}

	if (playback_item->default_width && playback_item->default_height) {
	    g_object_set (G_OBJECT (video_capsfilter), "caps",
			  gst_caps_new_simple ("video/x-raw-yuv",
					       "width",  G_TYPE_INT, (int) playback_item->default_width,
					       "height", G_TYPE_INT, (int) playback_item->default_height,
					       "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					       NULL), NULL);
	} else
	if (playback_item->default_width) {
	    g_object_set (G_OBJECT (video_capsfilter), "caps",
			  gst_caps_new_simple ("video/x-raw-yuv",
					       "width",  G_TYPE_INT, (int) playback_item->default_width,
					       "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					       NULL), NULL);
	} else
	if (playback_item->default_height) {
	    g_object_set (G_OBJECT (video_capsfilter), "caps",
			  gst_caps_new_simple ("video/x-raw-yuv",
					       "height", G_TYPE_INT, (int) playback_item->default_height,
					       "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					       NULL), NULL);
	}

	video_encoder = gst_element_factory_make ("ffenc_flv", NULL);
	if (!video_encoder) {
	    logE_ (_func, "gst_element_factory_make() failed (ffenc_flv)");
	    goto _failure;
	}
	// TODO Config parameter for bitrate.
//	g_object_set (G_OBJECT (video_encoder), "bitrate", 100000, NULL);
	g_object_set (G_OBJECT (video_encoder), "bitrate", (gulong) playback_item->default_bitrate, NULL);

#if 0
	{
//	    GstPad * const pad = gst_element_get_static_pad (video_encoder, "sink");
	    GstPad * const pad = gst_element_get_static_pad (videoscale, "src");
	    GstCaps * const caps = gst_caps_new_simple ("video/x-raw-yuv", "width", G_TYPE_INT, 64, "height", G_TYPE_INT, 48, NULL);
	    gst_pad_set_caps (pad, caps);
	    gst_caps_unref (caps);
	    gst_object_unref (pad);
	}
#endif

#if 0
	{
	    GstPad * const pad = gst_element_get_static_pad (video_encoder, "sink");
//	    GstCaps * const caps = gst_caps_new_simple ("video/x-raw-yuv", "width", G_TYPE_INT, 32, NULL);
	    GstCaps * const caps = gst_caps_new_simple ("video/x-raw-yuv", "width", G_TYPE_INT, 64, "height", G_TYPE_INT, 48, NULL);
	    gst_pad_set_caps (pad, caps);
	    gst_caps_unref (caps);
	    gst_object_unref (pad);
	}
#endif

	gst_bin_add_many (GST_BIN (video_encoder_bin), videoscale, video_capsfilter, video_encoder, fakevideosink, NULL);
	gst_element_link_many (videoscale, video_capsfilter, video_encoder, fakevideosink, NULL);

	{
	    GstPad * const pad = gst_element_get_static_pad (videoscale, "sink");
	    gst_element_add_pad (video_encoder_bin, gst_ghost_pad_new ("sink", pad));
	    gst_object_unref (pad);
	}

	// 'videoscale', 'video_encoder' and 'fakevideosink' belong to
	// 'video_encoder_bin' now.
	videoscale    = NULL;
	video_capsfilter = NULL;
	video_encoder = NULL;
	fakevideosink = NULL;
    }

    g_object_set (G_OBJECT (playbin), "audio-sink", audio_encoder_bin, NULL);
    audio_encoder_bin = NULL;

    g_object_set (G_OBJECT (playbin), "video-sink", video_encoder_bin, NULL);
    video_encoder_bin = NULL;

    g_object_set (G_OBJECT (playbin), "uri", playback_item->stream_spec->cstr(), NULL);

    // TODO got_video, got_audio -?

    if (!mt_unlocks (mutex) setPipelinePlaying ()) {
	mutex.lock ();
	goto _failure;
    }
  }

    goto _return;

mt_mutex (mutex) _failure:
    mt_unlocks (mutex) pipelineCreationFailed ();

_return:
    if (playbin)
	gst_object_unref (GST_OBJECT (playbin));
    if (audio_encoder_bin)
	gst_object_unref (GST_OBJECT (audio_encoder_bin));
    if (video_encoder_bin)
	gst_object_unref (GST_OBJECT (video_encoder_bin));
    if (audio_encoder)
	gst_object_unref (GST_OBJECT (audio_encoder));
    if (video_encoder)
	gst_object_unref (GST_OBJECT (video_encoder));
    if (fakeaudiosink)
	gst_object_unref (GST_OBJECT (fakeaudiosink));
    if (fakevideosink)
	gst_object_unref (GST_OBJECT (fakevideosink));
    if (videoscale)
	gst_object_unref (GST_OBJECT (videoscale));
    if (audio_capsfilter)
	gst_object_unref (GST_OBJECT (audio_capsfilter));
    if (video_capsfilter)
	gst_object_unref (GST_OBJECT (video_capsfilter));
}

void
GstStream::createSmartPipelineForUri ()
{
//    logD_ (_this_func_);

    assert (playback_item->spec_kind == PlaybackItem::SpecKind::Uri);

    if (playback_item->stream_spec->mem().len() == 0) {
        logE_ (_this_func, "URI not specified for channel \"", channel_opts->channel_name, "\"");
        return;
    }

    GstElement *pipeline  = NULL,
               *decodebin = NULL;

    mutex.lock ();

    got_audio_pad = false;
    got_video_pad = false;

    if (stream_closed) {
        logE_ (_this_func, "stream closed, channel \"", channel_opts->channel_name, "\"");
        goto _failure;
    }

    logD_ (_func, "uri: ", playback_item->stream_spec);

  {
    pipeline = gst_pipeline_new ("pipeline");
    // TODO add bus watch

    decodebin = gst_element_factory_make ("uridecodebin", NULL);
    if (!decodebin) {
        logE_ (_func, "gst_element_factory_make() failed (decodebin2)");
        goto _failure;
    }

    g_signal_connect (decodebin, "autoplug-continue", G_CALLBACK (decodebinAutoplugContinue), this);
    g_signal_connect (decodebin, "pad-added", G_CALLBACK (decodebinPadAdded), this);

    this->playbin = pipeline;
//    logD_ (_this_func, "this->playbin: 0x", fmt_hex, (UintPtr) this->playbin);
    gst_object_ref (this->playbin);

    gst_bin_add_many (GST_BIN (pipeline), decodebin, NULL);
    g_object_set (G_OBJECT (decodebin), "uri", playback_item->stream_spec->cstr(), NULL);
    g_object_set (G_OBJECT (decodebin), "use-buffering", FALSE, NULL);
//    g_object_set (G_OBJECT (decodebin), "download", FALSE, NULL);
//    g_object_set (G_OBJECT (decodebin), "buffer-size", (Int64) 0, NULL);
//    g_object_set (G_OBJECT (decodebin), "buffer-duration", (Int64) 0, NULL);
    decodebin = NULL;

    if (!mt_unlocks (mutex) setPipelinePlaying ()) {
        mutex.lock ();
	goto _failure;
    }
  }

    goto _return;

mt_mutex (mutex) _failure:
    mt_unlocks (mutex) pipelineCreationFailed ();

_return:
    if (pipeline)
        gst_object_unref (GST_OBJECT (pipeline));
    if (decodebin)
        gst_object_unref (GST_OBJECT (decodebin));
}

static bool isAnyCaps (GstCaps     * const caps,
                       ConstMemory   const ref_mem)
{
    GstStructure * const st = gst_caps_get_structure (caps, 0);
    gchar const * st_name = gst_structure_get_name (st);

    ConstMemory const st_name_mem (st_name, strlen (st_name));

    if (st_name_mem.len() >= ref_mem.len()
        && equal (ref_mem, ConstMemory (st_name_mem.mem(), ref_mem.len())))
    {
        return true;
    }

    return false;
}

static bool isAnyAudioCaps (GstCaps * const caps)
{
    return isAnyCaps (caps, "audio");
}

static bool isAnyVideoCaps (GstCaps * const caps)
{
    return isAnyCaps (caps, "video");
}

static bool isRawAudioCaps (GstCaps * const caps)
{
    for (guint i = 0, i_end = gst_caps_get_size (caps); i < i_end; ++i) {
        GstStructure * const ref_structure = gst_caps_get_structure (caps, i);
        gchar const * const name = gst_structure_get_name (ref_structure);
        ConstMemory const name_mem = ConstMemory (name, strlen (name));
        if (equal (name_mem, "audio/x-raw-int") ||
            equal (name_mem, "audio/x-raw-float"))
        {
            return true;
        }
    }

    return false;
}

static bool isRawVideoCaps (GstCaps * const caps)
{
    for (guint i = 0, i_end = gst_caps_get_size (caps); i < i_end; ++i) {
        GstStructure * const ref_structure = gst_caps_get_structure (caps, i);
        gchar const * const name = gst_structure_get_name (ref_structure);
        ConstMemory const name_mem = ConstMemory (name, strlen (name));
        if (equal (name_mem, "video/x-raw-yuv") ||
            equal (name_mem, "video/x-raw-rgb"))
        {
            return true;
        }
    }

    return false;
}

static bool isMpegAudioCaps (GstCaps * const caps)
{
    for (guint i = 0, i_end = gst_caps_get_size (caps); i < i_end; ++i) {
        GstStructure * const ref_structure = gst_caps_get_structure (caps, i);
        gchar const * const name = gst_structure_get_name (ref_structure);
        if (equal (ConstMemory (name, strlen (name)), "audio/mpeg")) {
            gboolean val = false;
            if (gst_structure_get_boolean (ref_structure, "framed", &val) && val)
                return true;
        }
    }

    return false;
}

static bool isH264VideoCaps (GstCaps * const caps)
{
    for (guint i = 0, i_end = gst_caps_get_size (caps); i < i_end; ++i) {
        GstStructure * const ref_structure = gst_caps_get_structure (caps, i);
        gchar const * const name = gst_structure_get_name (ref_structure);
        if (equal (ConstMemory (name, strlen (name)), "video/x-h264")) {
            gboolean val = false;
            if (gst_structure_get_boolean (ref_structure, "parsed", &val) && val)
                return true;
        }
    }

    return false;
}

gboolean
GstStream::decodebinAutoplugContinue (GstElement * const /* bin */,
                                      GstPad     * const /* pad */,
                                      GstCaps    * const caps,
                                      gpointer     const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    logD (plug, _func_);

    if (logLevelOn (plug, LogLevel::Debug)) {
        gchar * const str = gst_caps_to_string (caps);
        logD_ (_func, "caps: ", str);
        g_free (str);
    }

    if (!self->playback_item->force_transcode && !self->playback_item->force_transcode_audio) {
        if (isMpegAudioCaps (caps)) {
            logD (plug, _func, "autoplugged mpeg audio");
            return FALSE;
        }
    }

    if (!self->playback_item->force_transcode && !self->playback_item->force_transcode_video) {
        if (isH264VideoCaps (caps)) {
            logD (plug, _func, "autoplugged h.264 video");
            return FALSE;
        }
    }

    return TRUE;
}

void
GstStream::decodebinPadAdded (GstElement * const /* element */,
                              GstPad     * const new_pad,
                              gpointer     const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    logD (plug, _func_);

    GstCaps * const caps = gst_pad_get_caps (new_pad);
    if (!caps) {
        logD (plug, _func, "NULL caps");
        return;
    }

    {
        gchar * const str = gst_caps_to_string (caps);
        logD (plug, _func, "caps: ", str);
        g_free (str);
    }

    if (self->playback_item->no_audio
        && isAnyAudioCaps (caps))
    {
        self->setAudioPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    if (self->playback_item->no_video
        && isAnyVideoCaps (caps))
    {
        self->setVideoPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    if (isRawAudioCaps (caps)) {
        self->setRawAudioPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    if (isRawVideoCaps (caps)) {
        self->setRawVideoPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    if (isMpegAudioCaps (caps)) {
        // TODO Process AAC and MP3
        self->setAudioPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    if (isH264VideoCaps (caps)) {
        self->setVideoPad (new_pad);
        gst_caps_unref (caps);
        return;
    }

    logW_ (_func, "Unknown stream data format");
    gst_caps_unref (caps);
}

mt_unlocks (mutex) void
GstStream::doSetPad (GstPad            * const pad,
                     ConstMemory         const sink_el_name,
                     MediaDataCallback   const media_data_cb,
                     ConstMemory         const chain)
{
    assert (playbin);

    GstElement *encoder_bin = NULL;

    {
        GError *err = NULL;
        // TODO configurable encoder
        String const chain_str (chain);
        logD (plug, _func, "calling gst_parse_bin_from_description()");
        encoder_bin = gst_parse_bin_from_description (chain_str.cstr(),
                                                      TRUE /* ghost_unlinked_pads */,
                                                      &err);
        logD (plug, _func, "gst_parse_bin_from_description() returned");
        if (!encoder_bin) {
            logE_ (_func, "gst_parse_bin_from_description() failed: ", err->message);
            goto _failure;
        }

        if (err)
            logE_ (_func, "gst_parse_bin_from_description() recoverable error: ", err->message);
    }

    if (media_data_cb) {
        String const sink_el_name_str (sink_el_name);
        GstElement * const sink_el = gst_bin_get_by_name (GST_BIN (encoder_bin), sink_el_name_str.cstr());
        if (!sink_el) {
            logE_ (_func, "no \"", sink_el_name, "\" element in the encoding bin");
            goto _failure;
        }

        GstPad * const sink_pad = gst_element_get_static_pad (sink_el, "sink");
        if (!sink_pad) {
            logE_ (_func, "element called \"", sink_el_name, "\" doesn't have a \"sink\" pad");
            gst_object_unref (sink_el);
            goto _failure;
        }

        // TODO Use "handoff" signal
        gst_pad_add_buffer_probe (sink_pad, G_CALLBACK (media_data_cb), this);

        gst_object_unref (sink_pad);
        gst_object_unref (sink_el);
    }

    {
        GstPad * const sink_pad = gst_element_get_static_pad (encoder_bin, "sink");
        if (!sink_pad) {
            logE_ (_func, "encoder_bin doesn't have a \"sink\" pad");
            goto _failure;
        }

        GstElement * const tmp_playbin = this->playbin;
        gst_object_ref (tmp_playbin);
        // TODO unref

        mutex.unlock ();

        gst_bin_add (GST_BIN (tmp_playbin), encoder_bin);

#if 0
        logD_ (_func, "setting state to PLAYING, locked: ", GST_OBJECT_FLAG_IS_SET (encoder_bin, GST_ELEMENT_LOCKED_STATE));
        if (gst_element_set_state (encoder_bin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            logE_ (_func, "gst_element_set_state() failed (PLAYING)");
            encoder_bin = NULL;
            goto _failure_unlocked;
        }
#endif

//        encoder_bin = NULL;

        if (gst_pad_link (pad, sink_pad) != GST_PAD_LINK_OK) {
            logE_ (_func, "gst_pad_link() failed");
            gst_object_unref (sink_pad);
            goto _failure_unlocked;
        }
        gst_object_unref (sink_pad);

        logD (plug, _func, "setting state to PLAYING, locked: ", GST_OBJECT_FLAG_IS_SET (encoder_bin, GST_ELEMENT_LOCKED_STATE));
        {
            GstStateChangeReturn res = gst_element_set_state (encoder_bin, GST_STATE_PLAYING);
            switch (res) {
                case GST_STATE_CHANGE_FAILURE:
                    logD (plug, _func, "GST_STATE_CHANGE_FAILURE");
                    break;
                case GST_STATE_CHANGE_SUCCESS:
                    logD (plug, _func, "GST_STATE_CHANGE_SUCCESS");
                    break;
                case GST_STATE_CHANGE_ASYNC:
                    logD (plug, _func, "GST_STATE_CHANGE_ASYNC");
                    break;
                case GST_STATE_CHANGE_NO_PREROLL:
                    logD (plug, _func, "GST_STATE_CHANGE_NO_PREROLL");
                    break;
                default:
                    logD (plug, _func, "Unknown GstStateChangeReturn");
            }
            if (res == GST_STATE_CHANGE_FAILURE) {
                logE_ (_func, "gst_element_set_state() failed (PLAYING)");
                encoder_bin = NULL;
                goto _failure_unlocked;
            }
        }

#if 0
        logD_ (_func, "setting state to PLAYING #2, locked: ", GST_OBJECT_FLAG_IS_SET (playbin, GST_ELEMENT_LOCKED_STATE));
        if (gst_element_set_state (playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            logE_ (_func, "gst_element_set_state() failed (PLAYING)");
            encoder_bin = NULL;
            goto _failure_unlocked;
        }
#endif
    }

    goto _failure_unlocked;

_failure:
    mutex.unlock ();

_failure_unlocked:
    ;
//    if (encoder_bin)
//        gst_object_unref (encoder_bin);
}

void
GstStream::doSetAudioPad (GstPad      * const pad,
                          ConstMemory   const chain)
{
    logD (plug, _this_func, chain);

    mutex.lock ();
    if (got_audio_pad) {
        mutex.unlock ();

	GstCaps * const caps = gst_pad_get_caps (pad);
        gchar * const str = gst_caps_to_string (caps);
        logD (plug, _this_func, "ignoring extra audio pad, caps: ", str);
        g_free (str);
        gst_caps_unref (caps);

        return;
    }
    got_audio_pad = true;

    got_audio = true;

    mt_unlocks (mutex) doSetPad (pad,
                                 "audio",
                                 playback_item->no_audio ? NULL : GstStream::audioDataCb,
                                 chain);
}

void
GstStream::doSetVideoPad (GstPad      * const pad,
                          ConstMemory   const chain)
{
    logD (plug, _this_func, chain);

    mutex.lock ();
    if (got_video_pad) {
        mutex.unlock ();

	GstCaps * const caps = gst_pad_get_caps (pad);
        gchar * const str = gst_caps_to_string (caps);
        logD (plug, _this_func, "ignoring extra video pad, caps: ", str);
        g_free (str);
        gst_caps_unref (caps);

        return;
    }
    got_video_pad = true;

    got_video = true;

    mt_unlocks (mutex) doSetPad (pad,
                                 "video",
                                 playback_item->no_video ? NULL : GstStream::videoDataCb,
                                 chain);
}

void
GstStream::setRawAudioPad (GstPad * const pad)
{
    // TODO Configurable bitrate for faac
    StRef<String> const chain =
            st_makeString ("audioconvert ! audioresample ! audio/x-raw-int,channels=2 ! "
                           "faac",
                           (playback_item->aac_perfect_timestamp ? " perfect-timestamp=true" : ""),
                           " ! audio/mpeg,mpegversion=4,channels=2,base-profile=lc ! "
                           "fakesink name=audio",
                           (playback_item->sync_to_clock ? " sync=true" : ""));
    logD_ (_func, "chain: ", chain);
    doSetAudioPad (pad, chain->mem());
}

void
GstStream::setRawVideoPad (GstPad * const pad)
{
    // TODO Configurable bitrate for x264enc
    StRef<String> const chain =
            st_makeString ("ffmpegcolorspace ! "
                           "x264enc bitrate=500 speed-preset=veryfast profile=baseline "
                                   "key-int-max=30 threads=1 sliced-threads=true ! "
                           "video/x-h264,alignment=au,stream-format=avc,profile=constrained-baseline ! "
                           "fakesink name=video",
                           playback_item->sync_to_clock ? " sync=true" : "");
    logD_ (_func, "chain: ", chain);
    doSetVideoPad (pad, chain->mem());
}

void
GstStream::setAudioPad (GstPad * const pad)
{
    StRef<String> const chain =
            st_makeString ("fakesink name=audio",
                           playback_item->sync_to_clock ? " sync=true" : "");
    doSetAudioPad (pad, chain->mem());
}

void
GstStream::setVideoPad (GstPad * const pad)
{
    StRef<String> const chain =
            st_makeString ("h264parse ! video/x-h264,stream-format=avc,alignment=au ! "
                           "fakesink name=video",
                           playback_item->sync_to_clock ? " sync=true" : "");
    doSetVideoPad (pad, chain->mem());
}

mt_unlocks (mutex) Result
GstStream::setPipelinePlaying ()
{
    logD (pipeline, _this_func_);

    GstElement * const chain_el = playbin;
    assert (chain_el);
    gst_object_ref (chain_el);

    if (channel_opts->no_video_timeout > 0) {
	no_video_timer = timers->addTimer (CbDesc<Timers::TimerCallback> (noVideoTimerTick,
                                                                          this /* cb_data */,
                                                                          this /* coderef_container */),
					   channel_opts->no_video_timeout,
					   true  /* periodical */,
                                           false /* auto_delete */);
    }

    changing_state_to_playing = true;
    mutex.unlock ();

    {
	GstBus * const bus = gst_element_get_bus (chain_el);
	assert (bus);
	gst_bus_set_sync_handler (bus, GstStream::busSyncHandler, this);
	gst_object_unref (bus);
    }

    logD (pipeline, _func, "Setting pipeline state to PAUSED");
    if (gst_element_set_state (chain_el, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
	logE_ (_func, "gst_element_set_state() failed (PAUSED)");
	goto _failure;
    }

    mutex.lock ();
    changing_state_to_playing = false;
    if (stream_closed) {
        mutex.unlock ();

	logD (pipeline, _func, "Setting pipeline state to NULL");
	if (gst_element_set_state (chain_el, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	    logE_ (_func, "gst_element_set_state() failed (NULL)");
    } else {
      mutex.unlock ();
    }

    gst_object_unref (chain_el);
    return Result::Success;

_failure:
    logD (pipeline, _func, "Setting pipeline state to NULL");
    if (gst_element_set_state (chain_el, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	logE_ (_func, "gst_element_set_state() failed (NULL #2)");

    gst_object_unref (chain_el);
    return Result::Failure;
}

mt_unlocks (mutex) void
GstStream::pipelineCreationFailed ()
{
    logD (pipeline, _this_func_);

    eos_pending = true;
    mutex.unlock ();

    releasePipeline ();
}

void
GstStream::doReleasePipeline ()
{
    logD (pipeline, _this_func_);

    mutex.lock ();

    if (no_video_timer) {
	timers->deleteTimer (no_video_timer);
	no_video_timer = NULL;
    }

    GstElement * const tmp_playbin = playbin;
    playbin = NULL;

    GstElement * const tmp_mix_audio_src = GST_ELEMENT (mix_audio_src);
    mix_audio_src = NULL;

    GstElement * const tmp_mix_video_src = GST_ELEMENT (mix_video_src);
    mix_video_src = NULL;

    bool to_null_state = false;
    if (!changing_state_to_playing)
	to_null_state = true;

    stream_closed = true;
    mutex.unlock ();

    reportStatusEvents ();

    if (tmp_playbin) {
	if (to_null_state) {
	    logD (pipeline, _func, "Setting pipeline state to NULL");
	    if (gst_element_set_state (tmp_playbin, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
		logE_ (_func, "gst_element_set_state() failed (NULL)");
	}

	gst_object_unref (tmp_playbin);
    }

    if (tmp_mix_audio_src)
	gst_object_unref (tmp_mix_audio_src);

    if (tmp_mix_video_src)
	gst_object_unref (tmp_mix_video_src);
}

void
GstStream::releasePipeline ()
{
    logD (pipeline, _this_func_);

//#error Two bugs here:
//#error   1. If the pipeline is being released (got an item in the workqueue),
//#error      then we should wait for the thread to join, but we don't.
//#error   2. Simultaneous joining of the same thread is undefined behavior.

    mutex.lock ();

    while (!workqueue_list.isEmpty()) {
        Ref<WorkqueueItem> &last_item = workqueue_list.getLast();
        switch (last_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                workqueue_list.remove (workqueue_list.getLastElement());
                break;
            case WorkqueueItem::ItemType_ReleasePipeline:
                mutex.unlock ();
                return;
            default:
                unreachable ();
        }
    }

    Ref<WorkqueueItem> const new_item = grab (new (std::nothrow) WorkqueueItem);
    new_item->item_type = WorkqueueItem::ItemType_ReleasePipeline;

    workqueue_list.prepend (new_item);
    workqueue_cond.signal ();

    bool const join = (tlocal != libMary_getThreadLocal());

    Ref<Thread> const tmp_workqueue_thread = workqueue_thread;
    workqueue_thread = NULL;
    mutex.unlock ();

    if (join) {
        if (tmp_workqueue_thread)
            tmp_workqueue_thread->join ();
    }
}

mt_mutex (mutex) void
GstStream::reportMetaData ()
{
    logD (stream, _func_);

    if (metadata_reported) {
	return;
    }
    metadata_reported = true;

    if (!playback_item->send_metadata)
	return;

    VideoStream::VideoMessage msg;
    if (!RtmpServer::encodeMetaData (&metadata, page_pool, &msg)) {
	logE_ (_func, "encodeMetaData() failed");
	return;
    }

    logD (stream, _func, "Firing video message");
    // TODO unlock/lock? (locked event == WRONG)
    video_stream->fireVideoMessage (&msg);

    page_pool->msgUnref (msg.page_list.first);
}

#if 0
// Moved to libmoment_gst
static void
dumpBufferFlags (GstBuffer * const buffer)
{
    Uint32 flags = (Uint32) GST_BUFFER_FLAGS (buffer);
    if (flags != GST_BUFFER_FLAGS (buffer))
	log__ (_func, "flags do not fit into Uint32");

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_READONLY)) {
	log__ (_func, "GST_BUFFER_FLAG_READONLY");
	flags ^= GST_BUFFER_FLAG_READONLY;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA4)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA4");
	flags ^= GST_BUFFER_FLAG_MEDIA4;
    }
#endif

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_PREROLL)) {
	log__ (_func, "GST_BUFFER_FLAG_PREROLL");
	flags ^= GST_BUFFER_FLAG_PREROLL;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DISCONT)) {
	log__ (_func, "GST_BUFFER_FLAG_DISCONT");
	flags ^= GST_BUFFER_FLAG_DISCONT;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS)) {
	log__ (_func, "GST_BUFFER_FLAG_IN_CAPS");
	flags ^= GST_BUFFER_FLAG_IN_CAPS;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_GAP)) {
	log__ (_func, "GST_BUFFER_FLAG_GAP");
	flags ^= GST_BUFFER_FLAG_GAP;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT)) {
	log__ (_func, "GST_BUFFER_FLAG_DELTA_UNIT");
	flags ^= GST_BUFFER_FLAG_DELTA_UNIT;
    }

#if 0
    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA1)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA1");
	flags ^= GST_BUFFER_FLAG_MEDIA1;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA2)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA2");
	flags ^= GST_BUFFER_FLAG_MEDIA2;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_MEDIA3)) {
	log__ (_func, "GST_BUFFER_FLAG_MEDIA3");
	flags ^= GST_BUFFER_FLAG_MEDIA3;
    }
#endif

    if (flags)
	log__ (_func, "Extra flags: 0x", fmt_hex, flags);
}
#endif

static Ref<String>
gstStateToString (GstState const state)
{
    ConstMemory mem;

    switch (state) {
	case GST_STATE_VOID_PENDING:
	    mem = "VOID_PENDING";
	    break;
	case GST_STATE_NULL:
	    mem = "NULL";
	    break;
	case GST_STATE_READY:
	    mem = "READY";
	    break;
	case GST_STATE_PAUSED:
	    mem = "PAUSED";
	    break;
	case GST_STATE_PLAYING:
	    mem = "PLAYING";
	    break;
	default:
	    mem = "Unknown";
    }

    return grab (new (std::nothrow) String (mem));
}

gboolean
GstStream::inStatsDataCb (GstPad    * const /* pad */,
			  GstBuffer * const buffer,
			  gpointer    const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    self->mutex.lock ();
    self->rx_bytes += GST_BUFFER_SIZE (buffer);
    self->mutex.unlock ();

    return TRUE;
}

void
GstStream::doAudioData (GstBuffer * const buffer)
{
#if 0
    {
	GstCaps * const caps = gst_buffer_get_caps (buffer);
	{
	    gchar * const str = gst_caps_to_string (caps);
	    logD (stream, _func, "caps: ", str);
	    g_free (str);
	}
        gst_caps_unref (caps);
        logD_ (_func, "stream 0x", fmt_hex, (UintPtr) this, ", "
              "timestamp ", fmt_def, GST_BUFFER_TIMESTAMP (buffer), ", "
              "flags 0x", fmt_hex, GST_BUFFER_FLAGS (buffer), ", "
              "size: ", fmt_def, GST_BUFFER_SIZE (buffer));
    }
#endif

    // TODO Update current time efficiently.
    updateTime ();

    logD (frames, _func, "stream 0x", fmt_hex, (UintPtr) this, ", "
	  "timestamp 0x", fmt_hex, GST_BUFFER_TIMESTAMP (buffer), ", "
	  "flags 0x", fmt_hex, GST_BUFFER_FLAGS (buffer), ", "
	  "size: ", fmt_def, GST_BUFFER_SIZE (buffer));
    if (logLevelOn (frames, LogLevel::Debug)) {
	dumpGstBufferFlags (buffer);

	if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS) ||
	    GST_BUFFER_TIMESTAMP (buffer) == (GstClockTime) -1)
	{
            logLock ();
	    hexdump (logs, ConstMemory (GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer)));
            logUnlock ();
	}
    }

    mutex.lock ();

    rx_audio_bytes += GST_BUFFER_SIZE (buffer);

    last_frame_time = getTime ();
    logD (frames, _func, "last_frame_time: 0x", fmt_hex, last_frame_time);

    if (prv_audio_timestamp > GST_BUFFER_TIMESTAMP (buffer)) {
	logW_ (_func, "backwards timestamp: prv 0x", fmt_hex, prv_audio_timestamp,
	       ", cur 0x", fmt_hex, GST_BUFFER_TIMESTAMP (buffer)); 
    }
    prv_audio_timestamp = GST_BUFFER_TIMESTAMP (buffer);

    VideoStream::AudioFrameType codec_data_type = VideoStream::AudioFrameType::Unknown;
    GstBuffer *codec_data_buffers [2];
    // Should be unrefed on return.
    GstBuffer *codec_data_buffer = NULL;
    Count num_codec_data_buffers = 0;
    if (first_audio_frame) {
	GstCaps * const caps = gst_buffer_get_caps (buffer);
	{
	    gchar * const str = gst_caps_to_string (caps);
	    logD (stream, _func, "caps: ", str);
	    g_free (str);
	}

	GstStructure * const structure = gst_caps_get_structure (caps, 0);
	gchar const * structure_name = gst_structure_get_name (structure);
	logD (stream, _func, "structure name: ", gst_structure_get_name (structure));

	ConstMemory const structure_name_mem (structure_name, strlen (structure_name));

	gint channels;
	gint rate;
	if (!gst_structure_get_int (structure, "channels", &channels))
	    channels = 1;
	if (!gst_structure_get_int (structure, "rate", &rate))
	    rate = 44100;

        logD_ (_func, "rate: ", rate, ", channels: ", channels);

	if (equal (structure_name_mem, "audio/mpeg")) {
	    gint mpegversion;
	    gint layer;

	    if (!gst_structure_get_int (structure, "mpegversion", &mpegversion))
		mpegversion = 1;
	    if (!gst_structure_get_int (structure, "layer", &layer))
		layer = 3;

	    if (mpegversion == 1 && layer == 3) {
	      // MP3
		audio_codec_id = VideoStream::AudioCodecId::MP3;
	    } else {
	      // AAC
		audio_codec_id = VideoStream::AudioCodecId::AAC;

                if (char const * const stream_format = gst_structure_get_string (structure, "stream-format")) {
                    logD_ (_func, "stream-format: ", stream_format);
                    if (equal (ConstMemory (stream_format, strlen (stream_format)), "adts")) {
                        // Note: For reference, see http://wiki.multimedia.cx/index.php?title=ADTS
                        is_adts_aac_stream = true;
                    }
                }

		do {
		  // Processing AacSequenceHeader.

		    GValue const * const val = gst_structure_get_value (structure, "codec_data");
		    if (!val) {
			logW_ (_func, "Codec data not found");
			break;
		    }

		    if (!GST_VALUE_HOLDS_BUFFER (val)) {
			logW_ (_func, "codec_data doesn't hold a buffer");
			break;
		    }

		    codec_data_type = VideoStream::AudioFrameType::AacSequenceHeader;
		    codec_data_buffers [0] = gst_value_get_buffer (val);
		    num_codec_data_buffers = 1;
		} while (0);
	    }
	} else
	if (equal (structure_name_mem, "audio/x-speex")) {
	  // Speex
	    audio_codec_id = VideoStream::AudioCodecId::Speex;

	    do {
	      // Processing Speex stream headers.

		GValue const * const val = gst_structure_get_value (structure, "streamheader");
		if (!val) {
		    logW_ (_func, "Speex streamheader not found");
		    break;
		}

		if (!GST_VALUE_HOLDS_ARRAY (val)) {
		    logW_ (_func, "streamheader doesn't hold an array");
		    break;
		}

		codec_data_type = VideoStream::AudioFrameType::SpeexHeader;

		guint const arr_size = gst_value_array_get_size (val);
		for (guint i = 0; i < arr_size; ++i) {
		    GValue const * const elem_val = gst_value_array_get_value (val, i);
		    if (!elem_val) {
			logW_ (_func, "Speex streamheader: NULL array element #", i);
		    } else
		    if (!GST_VALUE_HOLDS_BUFFER (elem_val)) {
			logW_ (_func, "Speex streamheader element #", i, " doesn't hold a buffer");
		    } else {
			codec_data_buffers [i] = gst_value_get_buffer (elem_val);
			++num_codec_data_buffers;
		    }
		}

		logD (stream, _func, "Speex streamheader: num_codec_data_buffers: ", num_codec_data_buffers);
	    } while (0);
	} else
	if (equal (structure_name_mem, "audio/x-nellymoser")) {
	  // Nellymoser
	    audio_codec_id = VideoStream::AudioCodecId::Nellymoser;
	} else
	if (equal (structure_name_mem, "audio/x-adpcm")) {
	  // ADPCM
	    audio_codec_id = VideoStream::AudioCodecId::ADPCM;
	} else
	if (equal (structure_name_mem, "audio/x-raw-int")) {
	  // Linear PCM, little endian
	    audio_codec_id = VideoStream::AudioCodecId::LinearPcmLittleEndian;
	} else
	if (equal (structure_name_mem, "audio/x-alaw")) {
	  // G.711 A-law logarithmic PCM
	    audio_codec_id = VideoStream::AudioCodecId::G711ALaw;
	} else
	if (equal (structure_name_mem, "audio/x-mulaw")) {
	  // G.711 mu-law logarithmic PCM
	    audio_codec_id = VideoStream::AudioCodecId::G711MuLaw;
	}

        audio_rate = rate;
        audio_channels = channels;

	metadata.audio_sample_rate = (Uint32) rate;
	metadata.got_flags |= RtmpServer::MetaData::AudioSampleRate;

	metadata.audio_sample_size = 16;
	metadata.got_flags |= RtmpServer::MetaData::AudioSampleSize;

	metadata.num_channels = (Uint32) channels;
	metadata.got_flags |= RtmpServer::MetaData::NumChannels;

	gst_caps_unref (caps);
    }

    if (first_audio_frame) {
	first_audio_frame = false;

	if (playback_item->send_metadata) {
	    if (!got_video || !first_video_frame) {
	      // There's no video or we've got the first video frame already.
		reportMetaData ();
		metadata_reported_cond.signal ();
	    } else {
	      // Waiting for the first video frame.
		while (got_video && first_video_frame)
		    // TODO FIXME This is a perfect way to hang up a thread.
		    metadata_reported_cond.wait (mutex);
	    }
	}
    }

    if (is_adts_aac_stream) {
        if (GST_BUFFER_SIZE (buffer) >= 7) {
            // Bit packing example:
            // AAAA BBBB  CCCC DDDD
            // 4321 4321  4321 4321
            //
            // 1234
            // 0001 0010  0011 0100
            // [] = { 0x12, 0x34 }

            // AAC codec data:
            //
            // AAAA ABBB  BDDD DEFG
            // 5432 1432  1432 1111
            //
            // A 5 AudioObjectType
            // B 4 samplingFrequencyIndex
            // if (samplingFrequencyIndex == 15) - forbidden
            //     C 24 samplingFrequency
            // D 4 channelConfiguration (0-7, last bit reserved)
            // E 1 frameLengthFlag
            // F 1 dependsOnCoreCoder = 0
            // G 1 extensionFlag = 0

            Byte * const adts_header = GST_BUFFER_DATA (buffer);
            Byte codec_data [2] = { 0, 0 };

            // 2 bits for object type in ADTS => possible values: 1 2 3 4
            Byte const obj_type = ((adts_header [2] >> 6) & 0x3) + 1;
            codec_data [0] |= obj_type << 3;

            Byte const rate_idx = (adts_header [2] >> 2) & 0xf;
            codec_data [0] |= rate_idx >> 1;
            codec_data [1] |= (rate_idx & 0x1) << 7;

            Byte const channel_config = ((adts_header [2] & 0x1) << 2) | ((adts_header [3] >> 6) & 0x3);
            codec_data [1] |= channel_config << 3;

            // Just use 1024 samples per frame.
            // See http://spectralhole.blogspot.ru/2010/09/aac-bistream-flaws-part-2-aac-960-zero.html
            codec_data [1] |= 0;

            if (!got_adts_aac_codec_data
                || adts_aac_codec_data [0] != codec_data [0]
                || adts_aac_codec_data [1] != codec_data [1])
            {
                got_adts_aac_codec_data = true;
                adts_aac_codec_data [0] = codec_data [0];
                adts_aac_codec_data [1] = codec_data [1];

                codec_data_buffer = gst_buffer_new_and_alloc (2);
                assert (codec_data_buffer);
                GST_BUFFER_TIMESTAMP (codec_data_buffer) = GST_BUFFER_TIMESTAMP (buffer);
                GST_BUFFER_SIZE (codec_data_buffer) = 2;
                GST_BUFFER_DURATION (codec_data_buffer) = 0;
                memcpy (GST_BUFFER_DATA (codec_data_buffer), codec_data, 2);

                codec_data_type = VideoStream::AudioFrameType::AacSequenceHeader;
                codec_data_buffers [0] = codec_data_buffer;
                num_codec_data_buffers = 1;

                logD_ (_func, "New AAC codec data: 0x", fmt_hex, (Uint32) codec_data [0], " 0x", (Uint32) codec_data [1]);
            }
        } else {
            logE_ (_func,
                   "AAC ADTS packet is too short. "
                   "AAC codec data was not extracted. Audio stream will not be playable.");
        }
    }

    bool skip_frame = false;
    {
	if (audio_skip_counter > 0) {
	    --audio_skip_counter;
	    logD (frames, _func, "Skipping audio frame, audio_skip_counter: ", audio_skip_counter, " left");
	    skip_frame = true;
	}

//	if (initial_play_pending && !play_pending) {
//	if (initial_seek_pending && initial_seek > 0) {
        if (!initial_seek_complete) {
	    // We have not started playing yet. This is most likely a preroll frame.
	    logD (frames, _func, "Skipping an early preroll frame");
	    skip_frame = true;
	}
    }

    VideoStream::AudioCodecId const tmp_audio_codec_id = audio_codec_id;
    unsigned const tmp_audio_rate = audio_rate;
    unsigned const tmp_audio_channels = audio_channels;
//    logD_ (_func, "rate: ", audio_rate, ", channels: ", audio_channels);
    mutex.unlock ();

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS) ||
	GST_BUFFER_TIMESTAMP (buffer) == (GstClockTime) -1)
    {
	skip_frame = true;
    }

    if (tmp_audio_codec_id == VideoStream::AudioCodecId::Unknown) {
	logD (frames, _func, "unknown codec id, dropping audio frame");
	goto _return;
    }

    if (num_codec_data_buffers > 0) {
      // Reporting codec data if needed.

	for (Size i = 0; i < num_codec_data_buffers; ++i) {
	    Size msg_len = 0;

//	    Uint64 const cd_timestamp_nanosec = (Uint64) (GST_BUFFER_TIMESTAMP (codec_data_buffers [i]));
	    Uint64 const cd_timestamp_nanosec = 0;

	    if (logLevelOn (frames, LogLevel::D)) {
                logLock ();
                logD_unlocked_ (_func, "CODEC DATA");
		hexdump (logs, ConstMemory (GST_BUFFER_DATA (codec_data_buffers [i]), GST_BUFFER_SIZE (codec_data_buffers [i])));
                logUnlock ();
            }

            PagePool::PageListHead page_list;

            if (playback_item->enable_prechunking) {
                Size msg_audio_hdr_len = 1;
                if (codec_data_type == VideoStream::AudioFrameType::AacSequenceHeader)
                    msg_audio_hdr_len = 2;

                RtmpConnection::PrechunkContext prechunk_ctx (msg_audio_hdr_len /* initial_offset */);
                RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                                     ConstMemory (GST_BUFFER_DATA (codec_data_buffers [i]),
                                                                  GST_BUFFER_SIZE (codec_data_buffers [i])),
                                                     page_pool,
                                                     &page_list,
                                                     RtmpConnection::DefaultAudioChunkStreamId,
                                                     cd_timestamp_nanosec,
                                                     false /* first_chunk */);
            } else {
                page_pool->getFillPages (&page_list,
                                         ConstMemory (GST_BUFFER_DATA (codec_data_buffers [i]),
                                                      GST_BUFFER_SIZE (codec_data_buffers [i])));
            }
	    msg_len += GST_BUFFER_SIZE (codec_data_buffers [i]);

	    VideoStream::AudioMessage msg;
	    msg.timestamp_nanosec = cd_timestamp_nanosec;
	    msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
	    msg.frame_type = codec_data_type;
	    msg.codec_id = tmp_audio_codec_id;
            msg.rate = tmp_audio_rate;
            msg.channels = tmp_audio_channels;

	    msg.page_pool = page_pool;
	    msg.page_list = page_list;
	    msg.msg_len = msg_len;
	    msg.msg_offset = 0;

	    video_stream->fireAudioMessage (&msg);

	    page_pool->msgUnref (page_list.first);
	}
    }

    if (skip_frame)
        goto _return;

  {
    Size msg_len = 0;

    Uint64 const timestamp_nanosec = (Uint64) (GST_BUFFER_TIMESTAMP (buffer));
//    logD_ (_func, "timestamp: 0x", fmt_hex, timestamp_nanosec, ", size: ", fmt_def, GST_BUFFER_SIZE (buffer));
//    logD_ (_func, "tmp_audio_codec_id: ", tmp_audio_codec_id);

    PagePool::PageListHead page_list;

    Byte *buffer_data = GST_BUFFER_DATA (buffer);
    Size  buffer_size = GST_BUFFER_SIZE (buffer);
    if (is_adts_aac_stream && buffer_size >= 7) {
        bool const protection_absent = buffer_data [1] & 0x1;
        Size const adts_header_len = (protection_absent ? 7 : 9);

        buffer_data += adts_header_len;
        buffer_size -= adts_header_len;
    }

    if (playback_item->enable_prechunking) {
        Size gen_audio_hdr_len = 1;
        if (tmp_audio_codec_id == VideoStream::AudioCodecId::AAC)
            gen_audio_hdr_len = 2;

        RtmpConnection::PrechunkContext prechunk_ctx (gen_audio_hdr_len /* initial_offset */);
        RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                             ConstMemory (buffer_data, buffer_size),
                                             page_pool,
                                             &page_list,
                                             RtmpConnection::DefaultAudioChunkStreamId,
                                             timestamp_nanosec / 1000000,
                                             false /* first_chunk */);
    } else {
        page_pool->getFillPages (&page_list, ConstMemory (buffer_data, buffer_size));
    }
    msg_len += buffer_size;

    VideoStream::AudioMessage msg;
    msg.timestamp_nanosec = timestamp_nanosec;
    msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
    msg.frame_type = VideoStream::AudioFrameType::RawData;
    msg.codec_id = tmp_audio_codec_id;

    msg.page_pool = page_pool;
    msg.page_list = page_list;
    msg.msg_len = msg_len;
    msg.msg_offset = 0;
    msg.rate = tmp_audio_rate;
    msg.channels = tmp_audio_channels;

    video_stream->fireAudioMessage (&msg);

    page_pool->msgUnref (page_list.first);
  }

_return:
    if (codec_data_buffer)
        gst_buffer_unref (codec_data_buffer);
}

gboolean
GstStream::audioDataCb (GstPad    * const /* pad */,
			GstBuffer * const buffer,
			gpointer    const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);
    self->doAudioData (buffer);
    return TRUE;
}

void
GstStream::handoffAudioDataCb (GstElement * const /* element */,
			       GstBuffer  * const buffer,
			       GstPad     * const /* pad */,
			       gpointer     const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);
    self->doAudioData (buffer);
}

void
GstStream::doVideoData (GstBuffer * const buffer)
{
    // TODO Update current time efficiently.
    updateTime ();

    logD (frames, _func, "stream 0x", fmt_hex, (UintPtr) this, ", "
	  "timestamp 0x", fmt_hex, GST_BUFFER_TIMESTAMP (buffer),
          " (", fmt_def, GST_BUFFER_TIMESTAMP (buffer), "), "
          "size ", GST_BUFFER_SIZE (buffer), ", "
	  "flags 0x", fmt_hex, GST_BUFFER_FLAGS (buffer));
    if (logLevelOn (frames, LogLevel::Debug))
	dumpGstBufferFlags (buffer);

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_PREROLL))
	logD (frames, _func, "preroll buffer");

#if 0
    // TEST Allowing both audio and video to flow through the same pin.
    {
	GstCaps * const caps = gst_buffer_get_caps (buffer);
	logD_ (_func, "caps: 0x", fmt_hex, (UintPtr) caps);
	if (!caps)
	    return;

	{
	    gchar * const str = gst_caps_to_string (caps);
	    logD_ (_func, "caps: ", str);
	    g_free (str);
	}

	GstStructure * const st = gst_caps_get_structure (caps, 0);
	gchar const * st_name = gst_structure_get_name (st);
	logD_ (_func, "st_name: ", gst_structure_get_name (st));
	ConstMemory const st_name_mem (st_name, strlen (st_name));

	ConstMemory const audio_str ("audio");
	if (st_name_mem.len() >= audio_str.len()
	    && memcmp (st_name_mem.mem(), audio_str.mem(), audio_str.len()) == 0)
	{
	    doAudioData (buffer);
	    return;
	}
    }
#endif

    mutex.lock ();

    rx_video_bytes += GST_BUFFER_SIZE (buffer);

    last_frame_time = getTime ();
    logD (frames, _func, "last_frame_time: 0x", fmt_hex, last_frame_time);

    if (first_video_frame) {
	first_video_frame = false;

	GstCaps * const caps = gst_buffer_get_caps (buffer);
	{
	    gchar * const str = gst_caps_to_string (caps);
	    logD (frames, _func, "caps: ", str);
	    g_free (str);
	}

	GstStructure * const st = gst_caps_get_structure (caps, 0);
	gchar const * st_name = gst_structure_get_name (st);
	logD (frames, _func, "st_name: ", gst_structure_get_name (st));
	ConstMemory const st_name_mem (st_name, strlen (st_name));

	if (equal (st_name_mem, "video/x-flash-video")) {
	   video_codec_id = VideoStream::VideoCodecId::SorensonH263;
	} else
	if (equal (st_name_mem, "video/x-h264")) {
	   video_codec_id = VideoStream::VideoCodecId::AVC;
           is_h264_stream = true;

#if 0
// Deprecated
	   do {
	     // Processing AvcSequenceHeader

	       GValue const * const val = gst_structure_get_value (st, "codec_data");
	       if (!val) {
		   logW_ (_func, "Codec data not found");
		   break;
	       }

	       if (!GST_VALUE_HOLDS_BUFFER (val)) {
		   logW_ (_func, "codec_data doesn't hold a buffer");
		   break;
	       }

	       avc_codec_data_buffer = gst_value_get_buffer (val);
	       logD (frames, _func, "avc_codec_data_buffer: 0x", fmt_hex, (UintPtr) avc_codec_data_buffer);
	   } while (0);
#endif
	} else
	if (equal (st_name_mem, "video/x-vp6")) {
	   video_codec_id = VideoStream::VideoCodecId::VP6;
	} else
	if (equal (st_name_mem, "video/x-flash-screen")) {
	   video_codec_id = VideoStream::VideoCodecId::ScreenVideo;
	}

        gst_caps_unref (caps);

	if (playback_item->send_metadata) {
	    if (!got_audio || !first_audio_frame) {
	      // There's no video or we've got the first video frame already.
		reportMetaData ();
		metadata_reported_cond.signal ();
	    } else {
	      // Waiting for the first audio frame.
		while (got_audio && first_audio_frame)
		    // TODO FIXME This is a perfect way to hang up a thread.
		    metadata_reported_cond.wait (mutex);
	    }
	}
    }

    bool skip_frame = false;
    {
	if (video_skip_counter > 0) {
	    --video_skip_counter;
	    logD (frames, _func, "Skipping frame, video_skip_counter: ", video_skip_counter);
	    skip_frame = true;
	}

//	if (initial_play_pending && !play_pending) {
//	if (initial_seek_pending && initial_seek > 0) {
        if (!initial_seek_complete) {
	    // We have not started playing yet. This is most likely a preroll frame.
	    logD (frames, _func, "Skipping an early preroll frame");
	    skip_frame = true;
	}
    }

    VideoStream::VideoCodecId const tmp_video_codec_id = video_codec_id;
    mutex.unlock ();

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_IN_CAPS) ||
	GST_BUFFER_TIMESTAMP (buffer) == (GstClockTime) -1)
    {
        logD (frames, _func, "Skipping frame by in_caps/timestamp: ", GST_BUFFER_TIMESTAMP (buffer));
	skip_frame = true;
    }

    if (is_h264_stream) {
      // Reporting AVC codec data if needed.

        bool report_avc_codec_data = false;
        {
            GstCaps * const caps = gst_buffer_get_caps (buffer);
            GstStructure * const st = gst_caps_get_structure (caps, 0);

            do {
                GValue const * const val = gst_structure_get_value (st, "codec_data");
                if (!val || !GST_VALUE_HOLDS_BUFFER (val))
                   break;

                GstBuffer * const new_buffer = gst_value_get_buffer (val);
                if (avc_codec_data_buffer) {
                    if (equal (ConstMemory (GST_BUFFER_DATA (avc_codec_data_buffer),
                                            GST_BUFFER_SIZE (avc_codec_data_buffer)),
                               ConstMemory (GST_BUFFER_DATA (new_buffer),
                                            GST_BUFFER_SIZE (new_buffer))))
                    {
                      // Codec data has not changed.
                        break;
                    }

                    gst_buffer_unref (avc_codec_data_buffer);
                }

                avc_codec_data_buffer = new_buffer;
                gst_buffer_ref (avc_codec_data_buffer);
                report_avc_codec_data = true;
            } while (0);

            gst_caps_unref (caps);
        }

        if (report_avc_codec_data) {
            // TODO vvv This doesn't sound correct.
            //
            // Timestamps for codec data buffers are seemingly random.
//            Uint64 const timestamp_nanosec = (Uint64) (GST_BUFFER_TIMESTAMP (avc_codec_data_buffer));
            Uint64 const timestamp_nanosec = 0;

            Size msg_len = 0;

            if (logLevelOn (frames, LogLevel::D)) {
                logLock ();
                log_unlocked__ (_func, "AVC SEQUENCE HEADER");
                hexdump (logs, ConstMemory (GST_BUFFER_DATA (avc_codec_data_buffer), GST_BUFFER_SIZE (avc_codec_data_buffer)));
                logUnlock ();
            }

            PagePool::PageListHead page_list;

            if (playback_item->enable_prechunking) {
                RtmpConnection::PrechunkContext prechunk_ctx (5 /* initial_offset: FLV AVC header length */);
                RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                                     ConstMemory (GST_BUFFER_DATA (avc_codec_data_buffer),
                                                                  GST_BUFFER_SIZE (avc_codec_data_buffer)),
                                                     page_pool,
                                                     &page_list,
                                                     RtmpConnection::DefaultVideoChunkStreamId,
                                                     timestamp_nanosec / 1000000,
                                                     false /* first_chunk */);
            } else {
                page_pool->getFillPages (&page_list,
                                         ConstMemory (GST_BUFFER_DATA (avc_codec_data_buffer),
                                                      GST_BUFFER_SIZE (avc_codec_data_buffer)));
            }
            msg_len += GST_BUFFER_SIZE (avc_codec_data_buffer);

            VideoStream::VideoMessage msg;
            msg.timestamp_nanosec = timestamp_nanosec;
            msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);
            msg.frame_type = VideoStream::VideoFrameType::AvcSequenceHeader;
            msg.codec_id = tmp_video_codec_id;

            msg.page_pool = page_pool;
            msg.page_list = page_list;
            msg.msg_len = msg_len;
            msg.msg_offset = 0;

            if (logLevelOn (frames, LogLevel::D)) {
                logD_ (_func, "Firing video message (AVC sequence header):");
                logLock ();
                PagePool::dumpPages (logs, &page_list);
                logUnlock ();
            }

            video_stream->fireVideoMessage (&msg);

            page_pool->msgUnref (page_list.first);
        } // if (report_avc_codec_data)
    } // if (is_h264_stream)

    if (skip_frame) {
	logD (frames, _func, "skipping frame");
	return;
    }

    Size msg_len = 0;

    Uint64 const timestamp_nanosec = (Uint64) (GST_BUFFER_TIMESTAMP (buffer));

    VideoStream::VideoMessage msg;
    msg.frame_type = VideoStream::VideoFrameType::InterFrame;
    msg.codec_id = tmp_video_codec_id;

    bool is_keyframe = false;
#if 0
  // This is deprecated, kept here for reference.

    // Keyframe detection by parsing message body for Sorenson H.263
    // See ffmpeg:h263.c for reference.
    if (tmp_video_codec_id == VideoStream::VideoCodecId::SorensonH263) {
	if (GST_BUFFER_SIZE (buffer) >= 5) {
	    Byte const format = ((GST_BUFFER_DATA (buffer) [3] & 0x03) << 1) |
				((GST_BUFFER_DATA (buffer) [4] & 0x80) >> 7);
	    size_t offset = 4;
	    switch (format) {
		case 0:
		    offset += 2;
		    break;
		case 1:
		    offset += 4;
		    break;
		default:
		    break;
	    }

	    if (GST_BUFFER_SIZE (buffer) > offset) {
		if (((GST_BUFFER_DATA (buffer) [offset] & 0x60) >> 4) == 0)
		    is_keyframe = true;
	    }
	}
    } else
#endif
    {
	is_keyframe = !GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }

    if (is_keyframe) {
	msg.frame_type = VideoStream::VideoFrameType::KeyFrame;
    } else {
      // Note: We do not make distinction between inter frames and
      //       disposable inter frames for Sorenson h.263 here.
    }

    PagePool::PageListHead page_list;

    if (playback_item->enable_prechunking) {
        Size gen_video_hdr_len = 1;
        if (tmp_video_codec_id == VideoStream::VideoCodecId::AVC)
            gen_video_hdr_len = 5;

        RtmpConnection::PrechunkContext prechunk_ctx (gen_video_hdr_len /* initial_offset */);
        RtmpConnection::fillPrechunkedPages (&prechunk_ctx,
                                             ConstMemory (GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer)),
                                             page_pool,
                                             &page_list,
                                             RtmpConnection::DefaultVideoChunkStreamId,
                                             timestamp_nanosec / 1000000,
                                             false /* first_chunk */);
    } else {
        page_pool->getFillPages (&page_list, ConstMemory (GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer)));
    }
    msg_len += GST_BUFFER_SIZE (buffer);

    msg.timestamp_nanosec = timestamp_nanosec;
    msg.prechunk_size = (playback_item->enable_prechunking ? RtmpConnection::PrechunkSize : 0);

    msg.page_pool = page_pool;
    msg.page_list = page_list;
    msg.msg_len = msg_len;
    msg.msg_offset = 0;

#if 0
    if (logLevelOn (frames, LogLevel::D)) {
        logD_ (_func, "Firing video message:");
        logLock ();
        PagePool::dumpPages (logs, &page_list);
        logUnlock ();
    }
#endif

    video_stream->fireVideoMessage (&msg);

    page_pool->msgUnref (page_list.first);
}

gboolean
GstStream::videoDataCb (GstPad    * const /* pad */,
			GstBuffer * const buffer,
			gpointer    const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);
    self->doVideoData (buffer);
    return TRUE;
}

void
GstStream::handoffVideoDataCb (GstElement * const /* element */,
			       GstBuffer  * const buffer,
			       GstPad     * const /* pad */,
			       gpointer     const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);
    self->doVideoData (buffer);
}

GstBusSyncReply
GstStream::busSyncHandler (GstBus     * const /* bus */,
			   GstMessage * const msg,
			   gpointer     const _self)
{
    updateTime ();
    logD (bus, _func, gst_message_type_get_name (GST_MESSAGE_TYPE (msg)), ", src: 0x", fmt_hex, (UintPtr) GST_MESSAGE_SRC (msg));

    GstStream * const self = static_cast <GstStream*> (_self);
    logD (bus, _func, "self->playbin: 0x", fmt_hex, (UintPtr) self->playbin);

    self->mutex.lock ();
    if (GST_MESSAGE_SRC (msg) == GST_OBJECT (self->playbin)) {
	logD (bus, _func, "PIPELINE MESSAGE: ", gst_message_type_get_name (GST_MESSAGE_TYPE (msg)));

	switch (GST_MESSAGE_TYPE (msg)) {
	    case GST_MESSAGE_STATE_CHANGED: {
		GstState old_state,
			 new_state,
			 pending_state;
		gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
		logD (stream, _func, "STATE_CHANGED from ", gstStateToString (old_state), " "
		      "to ", gstStateToString (new_state), ", "
		      "pending state: ", gstStateToString (pending_state));
		if (pending_state == GST_STATE_VOID_PENDING) {
		    if (new_state == GST_STATE_PAUSED) {
			logD (bus, _func, "PAUSED");

			if (self->initial_seek_pending) {
//                            logD_ (_func, "setting 'initial_seek_pending' to 'false'");
                            self->initial_seek_pending = false;
			    if (self->initial_seek > 0) {
				self->seek_pending = true;
			    } else {
                                self->initial_seek_complete = true;
				if (self->initial_play_pending) {
				    self->initial_play_pending = false;
				    self->play_pending = true;
				}
			    }
			} else {
                            self->initial_seek_complete = true;
			    if (self->initial_play_pending) {
				self->initial_play_pending = false;
				self->play_pending = true;
			    }
			}

			self->mutex.unlock ();

                        self->reportStatusEvents ();
			goto _return;
		    } else
		    if (new_state == GST_STATE_PLAYING) {
			logD (bus, _func, "PLAYING");
		    }
		}
	    } break;
	    case GST_MESSAGE_EOS: {
		logD (stream, _func, "EOS");

		self->eos_pending = true;
		self->mutex.unlock ();

                self->reportStatusEvents ();
		goto _return;
	    } break;
	    case GST_MESSAGE_ERROR: {
		logD (stream, _func, "ERROR");

		self->error_pending = true;
		self->mutex.unlock ();

                self->reportStatusEvents ();
		goto _return;
	    } break;
	    default:
	      // No-op
		;
	}

	logD (bus, _func, "MSG DONE");
    }
    self->mutex.unlock ();

_return:
    return GST_BUS_PASS;
}

void
GstStream::noVideoTimerTick (void * const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    Time const time = getTime();

    self->mutex.lock ();
    logD (novideo, _self_func, "time: 0x", fmt_hex, time, ", last_frame_time: 0x", self->last_frame_time);

    if (self->stream_closed) {
	self->mutex.unlock ();
	return;
    }

    if (time > self->last_frame_time &&
	time - self->last_frame_time >= 15 /* TODO Config param for the timeout */)
    {
	logD (novideo, _func, "firing \"no video\" event");

	if (self->no_video_timer) {
	    self->timers->deleteTimer (self->no_video_timer);
	    self->no_video_timer = NULL;
	}

	self->no_video_pending = true;
	self->mutex.unlock ();

	self->doReportStatusEvents ();
    } else {
	self->got_video_pending = true;
	self->mutex.unlock ();

	self->doReportStatusEvents ();
    }
}

VideoStream::EventHandler GstStream::mix_stream_handler = {
    mixStreamAudioMessage,
    mixStreamVideoMessage,
    NULL /* rtmpCommandMessage */,
    NULL /* closed */,
    NULL /* numWatchersChanged */
};

void
GstStream::mixStreamAudioMessage (VideoStream::AudioMessage * const mt_nonnull /* audio_msg */,
				  void * const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    logD_ (_func_);

    self->mutex.lock ();
    if (!self->mix_audio_src) {
	self->mutex.unlock ();
	return;
    }

  // TODO

    self->mutex.unlock ();
}

void
GstStream::mixStreamVideoMessage (VideoStream::VideoMessage * const mt_nonnull video_msg,
				  void * const _self)
{
    GstStream * const self = static_cast <GstStream*> (_self);

    logD_ (_func_);

    self->mutex.lock ();
    if (!self->mix_video_src) {
	self->mutex.unlock ();
	return;
    }

    GstBuffer * const buffer = gst_buffer_new_and_alloc (video_msg->msg_len);
    assert (buffer);

    {
	PagePool *normalized_page_pool;
	PagePool::PageListHead normalized_pages;
	Size normalized_offset = 0;
	bool unref_normalized_pages;
	if (video_msg->prechunk_size == 0) {
	    logD_ (_func, "non-prechunked data");

	    normalized_pages = video_msg->page_list;
            normalized_offset = video_msg->msg_offset;
	    unref_normalized_pages = false;
	} else {
	    logD_ (_func, "prechunked data");

	    unref_normalized_pages = true;

	    RtmpConnection::normalizePrechunkedData (video_msg,
						     video_msg->page_pool,
						     &normalized_page_pool,
						     &normalized_pages,
						     &normalized_offset);
	    // TODO This assertion should become unnecessary (support msg_offs
	    //      in PagePool::PageListArray).
	    assert (normalized_offset == 0);
	}

	PagePool::PageListArray pl_array (normalized_pages.first, video_msg->msg_len);
	if (video_msg->msg_len > 0) {
            // TODO 1. FLV header stripping is not necessary anymore, "-1" len bug.
	    pl_array.get (1 /* TEST FLV VIDEO TAG STRIPPING */ /* offset */, Memory (GST_BUFFER_DATA (buffer), video_msg->msg_len - 1));
        }

	if (unref_normalized_pages)
	    normalized_page_pool->msgUnref (normalized_pages.first);
    }

    gst_buffer_set_caps (buffer, self->mix_video_caps);
    GST_BUFFER_TIMESTAMP (buffer) = (Uint64) video_msg->timestamp_nanosec;
    GST_BUFFER_SIZE (buffer) = video_msg->msg_len;
    GST_BUFFER_DURATION (buffer) = 0;

    if (video_msg->frame_type.isInterFrame())
	GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    logD_ (_func, "pushing buffer");
    GstFlowReturn const res = gst_app_src_push_buffer (self->mix_video_src, buffer);
    if (res != GST_FLOW_OK)
	logD_ (_func, "res: ", (unsigned long) res);

    self->mutex.unlock ();
}

void
GstStream::doCreatePipeline ()
{
    logD (pipeline, _this_func_);

    if (playback_item->spec_kind == PlaybackItem::SpecKind::Chain) {
	createPipelineForChainSpec ();
    } else
    if (playback_item->spec_kind == PlaybackItem::SpecKind::Uri) {
//	createPipelineForUri ();
	createSmartPipelineForUri ();
    } else {
        assert (playback_item->spec_kind == PlaybackItem::SpecKind::None);
    }
}

void
GstStream::createPipeline ()
{
    logD (pipeline, _this_func_);

    mutex.lock ();

    while (!workqueue_list.isEmpty()) {
        Ref<WorkqueueItem> &last_item = workqueue_list.getLast();
        switch (last_item->item_type) {
            case WorkqueueItem::ItemType_CreatePipeline:
                mutex.unlock ();
                return;
            case WorkqueueItem::ItemType_ReleasePipeline:
                mutex.unlock ();
                return;
            default:
                unreachable ();
        }
    }

    Ref<WorkqueueItem> const new_item = grab (new (std::nothrow) WorkqueueItem);
    new_item->item_type = WorkqueueItem::ItemType_CreatePipeline;

    workqueue_list.prepend (new_item);
    workqueue_cond.signal ();

    mutex.unlock ();
}

void
GstStream::doReportStatusEvents ()
{
    logD (stream, _func_);

    mutex.lock ();
    if (reporting_status_events) {
	mutex.unlock ();
	return;
    }
    reporting_status_events = true;

    while (eos_pending       ||
	   error_pending     ||
	   no_video_pending  ||
	   got_video_pending ||
	   seek_pending      ||
	   play_pending)
    {
	if (close_notified) {
	    logD (stream, _func, "close_notified");

	    mutex.unlock ();
	    return;
	}

	if (eos_pending) {
	    logD (stream, _func, "eos_pending");
	    eos_pending = false;
	    close_notified = true;

	    if (frontend) {
		logD (stream, _func, "firing EOS");
		mt_unlocks_locks (mutex) frontend.call_mutex (frontend->eos, mutex);
	    }

	    break;
	}

	if (error_pending) {
	    logD (stream, _func, "error_pending");
	    error_pending = false;
	    close_notified = true;

	    if (frontend) {
		logD (stream, _func, "firing ERROR");
		mt_unlocks_locks (mutex) frontend.call_mutex (frontend->error, mutex);
	    }

	    break;
	}

	if (no_video_pending) {
            got_video_pending = false;

	    logD (stream, _func, "no_video_pending");
	    no_video_pending = false;
	    if (stream_closed) {
		mutex.unlock ();
		return;
	    }

	    if (frontend) {
		logD (stream, _func, "firing NO_VIDEO");
		mt_unlocks_locks (mutex) frontend.call_mutex (frontend->noVideo, mutex);
	    }
	}

	if (got_video_pending) {
	    logD (stream, _func, "got_video_pending");
	    got_video_pending = false;
	    if (stream_closed) {
		mutex.unlock ();
		return;
	    }

	    if (frontend)
		mt_unlocks_locks (mutex) frontend.call_mutex (frontend->gotVideo, mutex);
	}

	if (seek_pending) {
	    logD (stream, _func, "seek_pending");
	    assert (!play_pending);
	    seek_pending = false;
	    if (stream_closed) {
		mutex.unlock ();
		return;
	    }

	    if (initial_seek > 0) {
		logD (stream, _func, "initial_seek: ", initial_seek);

		Time const tmp_initial_seek = initial_seek;

		GstElement * const tmp_playbin = playbin;
		gst_object_ref (tmp_playbin);
		mutex.unlock ();

		bool seek_failed = false;
		if (!gst_element_seek_simple (tmp_playbin,
					      GST_FORMAT_TIME,
					      (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
					      (GstClockTime) tmp_initial_seek * 1000000000LL))
		{
		    seek_failed = true;
		    logE_ (_func, "Seek failed");
		}

		gst_object_unref (tmp_playbin);
		mutex.lock ();
		if (seek_failed)
		    play_pending = true;
	    } else {
		play_pending = true;
	    }
	}

	if (play_pending) {
	    logD (stream, _func, "play_pending");
	    assert (!seek_pending);
	    play_pending = false;
	    if (stream_closed) {
		mutex.unlock ();
		return;
	    }

	    GstElement * const tmp_playbin = playbin;
	    gst_object_ref (tmp_playbin);
	    mutex.unlock ();

	    logD (stream, _func, "Setting pipeline state to PLAYING");
	    if (gst_element_set_state (tmp_playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		logE_ (_func, "gst_element_set_state() failed (PLAYING)");

	    gst_object_unref (tmp_playbin);
	    mutex.lock ();
	}
    }

    logD (stream, _func, "done");
    reporting_status_events = false;
    mutex.unlock ();
}

void
GstStream::getTrafficStats (TrafficStats * const mt_nonnull ret_traffic_stats)
{
    mutex.lock ();
    ret_traffic_stats->rx_bytes = rx_bytes;
    ret_traffic_stats->rx_audio_bytes = rx_audio_bytes;
    ret_traffic_stats->rx_video_bytes = rx_video_bytes;
    mutex.unlock ();
}

void
GstStream::resetTrafficStats ()
{
    rx_bytes = 0;
    rx_audio_bytes = 0;
    rx_video_bytes = 0;
}

mt_const void
GstStream::init (CbDesc<MediaSource::Frontend> const &frontend,
                 Timers            * const timers,
                 DeferredProcessor * const deferred_processor,
		 PagePool          * const page_pool,
		 VideoStream       * const video_stream,
		 VideoStream       * const mix_video_stream,
		 Time                const initial_seek,
                 ChannelOptions    * const channel_opts,
                 PlaybackItem      * const playback_item)
{
    logD (pipeline, _this_func_);

    this->frontend = frontend;

    this->timers = timers;
    this->page_pool = page_pool;
    this->video_stream = video_stream;
    this->mix_video_stream = mix_video_stream;

    this->channel_opts  = channel_opts;
    this->playback_item = playback_item;

    this->initial_seek = initial_seek;
    if (initial_seek == 0)
        initial_seek_complete = true;

    deferred_reg.setDeferredProcessor (deferred_processor);

    {
        workqueue_thread =
                grab (new (std::nothrow) Thread (CbDesc<Thread::ThreadFunc> (workqueueThreadFunc,
                                                                             this,
                                                                             this)));
        if (!workqueue_thread->spawn (true /* joinable */))
            logE_ (_func, "Failed to spawn workqueue thread: ", exc->toString());
    }

    if (mix_video_stream) {
	mix_audio_caps = gst_caps_new_simple ("audio/x-speex",
#if 0
					      "rate", G_TYPE_INT, 16000,
					      "channels", G_TYPE_INT, 1,
#endif
					      NULL);

//	mix_video_caps = gst_caps_new_simple ("video/x-flv",
	mix_video_caps = gst_caps_new_simple ("video/x-flash-video",
#if 0
					      "width",  G_TYPE_INT, (int) default_width,
					      "height", G_TYPE_INT, (int) default_height,
#endif
					      "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
					      NULL);

	mix_video_stream->getEventInformer()->subscribe (
		CbDesc<VideoStream::EventHandler> (
			&mix_stream_handler,
			this,
			this));
    }
}

GstStream::GstStream ()
    : timers    (this /* coderef_container */),
      page_pool (this /* coderef_container */),

      video_stream (NULL),
      mix_video_stream (NULL),

      mix_audio_caps (NULL),
      mix_video_caps (NULL),

      no_video_timer (NULL),

      playbin (NULL),
      audio_probe_id (0),
      video_probe_id (0),

      got_audio_pad (false),
      got_video_pad (false),

      mix_audio_src (NULL),
      mix_video_src (NULL),

      initial_seek (0),
      initial_seek_pending  (true),
      initial_seek_complete (false),
      initial_play_pending  (true),

      metadata_reported (false),

      last_frame_time (0),

      audio_codec_id (VideoStream::AudioCodecId::Unknown),
      audio_rate (44100),
      audio_channels (1),

      video_codec_id (VideoStream::VideoCodecId::Unknown),

      got_in_stats (false),
      got_video (false),
      got_audio (false),

      first_audio_frame (true),
      first_video_frame (true),

      audio_skip_counter (0),
      video_skip_counter (0),

      is_adts_aac_stream (false),
      got_adts_aac_codec_data (false),

      is_h264_stream (false),
      avc_codec_data_buffer (NULL),

      prv_audio_timestamp (0),

      changing_state_to_playing (false),
      reporting_status_events (false),

      seek_pending (false),
      play_pending (false),

      no_video_pending (false),
      got_video_pending (false),
      error_pending  (false),
      eos_pending    (false),
      close_notified (false),

      stream_closed (false),

      rx_bytes (0),
      rx_audio_bytes (0),
      rx_video_bytes (0),

      tlocal (NULL)
{
    logD (pipeline, _this_func_);

    deferred_task.cb = CbDesc<DeferredProcessor::TaskCallback> (deferredTask, this, this);
}

GstStream::~GstStream ()
{
    logD (pipeline, _this_func_);

    mutex.lock ();
    assert (stream_closed);
    assert (!workqueue_thread);
    mutex.unlock ();

    if (mix_audio_caps)
	gst_caps_unref (mix_audio_caps);
    if (mix_video_caps)
	gst_caps_unref (mix_video_caps);

    if (avc_codec_data_buffer)
        gst_buffer_unref (avc_codec_data_buffer);

    deferred_reg.release ();
}

}

