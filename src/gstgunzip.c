/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2009 Ugo Riboni <nerochiaro@gmail.com>
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
 * SECTION:element-gunzip
 *
 * This plugin decompress the input using zlib
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * filesrc location=myfile.data.gz ! gunzip ! filesink location=myfile.data
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstgunzip.h"

GST_DEBUG_CATEGORY_STATIC (gst_gunzip_debug);
#define GST_CAT_DEFAULT gst_gunzip_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * TODO: allow only input as application/x-gzip 
 * TODO: see http://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/html/section-boiler-padtemplates.html
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

GST_BOILERPLATE (GstGunzip, gst_gunzip, GstElement,
    GST_TYPE_ELEMENT);

static gboolean gst_gunzip_set_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn gst_gunzip_chain (GstPad * pad, GstBuffer * buf);
static GstStateChangeReturn gst_gunzip_change_state (GstElement *element, GstStateChange transition);

/* GObject vmethod implementations */

static void
gst_gunzip_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "GZIP Decompressor",
    "Filter/Gunzip",
    "Decompress a gzipped stream",
    "Ugo Riboni <nerochiaro@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the gunzip's class */
static void
gst_gunzip_class_init (GstGunzipClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gstelement_class->change_state = gst_gunzip_change_state;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_gunzip_init (GstGunzip * filter,
    GstGunzipClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_gunzip_set_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_gunzip_chain));

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
gst_gunzip_set_caps (GstPad * pad, GstCaps * caps)
{
  GstGunzip *filter;
  GstPad *otherpad;

  filter = GST_GUNZIP (gst_pad_get_parent (pad));
  otherpad = (pad == filter->srcpad) ? filter->sinkpad : filter->srcpad;
  gst_object_unref (filter);

  return gst_pad_set_caps (otherpad, caps);
}

static GstStateChangeReturn
gst_gunzip_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstGunzip *filter = GST_GUNZIP (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      // Initialize zlib decompression session for this stream
      filter->strm.zalloc = Z_NULL;
      filter->strm.zfree = Z_NULL;
      filter->strm.opaque = Z_NULL;
      filter->strm.avail_in = 0;
      filter->strm.next_in = Z_NULL;

      // inflateInit2 needed to decode gzip format in addition to zlib format.
      // the second parameter needs to be 15+32 to enable automatic detection of format.
      ret = inflateInit2(&filter->strm, 15+32);
      if (ret != Z_OK)
      {
            GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, DECODE, ("ZLib init failed"), ("inflateInit2 returned %d", ret));
            return GST_STATE_CHANGE_FAILURE;
      }
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      // Clean up zlib session when stream is over or forcibly interrupted
      inflateEnd(&filter->strm);
      break;
    default:
      break;
  }

  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_gunzip_chain (GstPad * pad, GstBuffer * buf)
{
  GstGunzip *filter;
  GstBuffer *out;
  GstFlowReturn pushret;
  int ret;

  filter = GST_GUNZIP (GST_OBJECT_PARENT (pad));

  // decompress until deflate stream ends or end of file. more gst buffers may be necessary
  filter->strm.avail_in = GST_BUFFER_SIZE (buf);
  filter->strm.next_in = GST_BUFFER_DATA (buf);
  do
  {
    // allocate a new gst buffer to hold the next chunk of decompressed data and set it up on the zlib session,
    // then decompress the input buffer.
    out = gst_buffer_new_and_alloc (OUTBUF_SIZE);
    filter->strm.avail_out = OUTBUF_SIZE;
    filter->strm.next_out = GST_BUFFER_DATA (out);
    
    ret = inflate (&filter->strm, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
    {
      gst_buffer_unref (out);
      gst_buffer_unref (buf);
      GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, DECODE, ("ZLib failed to decompress data"), ("inflate returned %d", ret));
      return GST_FLOW_ERROR;
    }

    // the decompressed data may not fill the buffer entirely, so adjust its size counter to the real amount.
    if (filter->strm.avail_out != 0) GST_BUFFER_SIZE (out) = OUTBUF_SIZE - filter->strm.avail_out;

    pushret = gst_pad_push (filter->srcpad, out);
    if (pushret != GST_FLOW_OK)
    {
      gst_buffer_unref (out);
      gst_buffer_unref (buf);
      GST_ELEMENT_ERROR (GST_ELEMENT (filter), STREAM, DECODE, ("Failed to push output buffer"), (""));
      return pushret;
    }
  } while (ret != Z_STREAM_END && filter->strm.avail_out == 0);

  // do not call inflateEnd here, it will be called by the state change function

  gst_buffer_unref (buf);
  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
gunzip_init (GstPlugin * gunzip)
{
  /* debug category for fltering log messages
   */
  GST_DEBUG_CATEGORY_INIT (gst_gunzip_debug, "gunzip",
      0, "GZIP Decompressor");

  return gst_element_register (gunzip, "gunzip", GST_RANK_NONE,
      GST_TYPE_GUNZIP);
}

/* gstreamer looks for this structure to register gunzips
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gunzip",
    "GZIP Decompressor",
    gunzip_init,
    VERSION,
    "LGPL",
    "GStreamer",
    "http://github.com/nerochiaro/gst-gunzip/tree/master"
)
