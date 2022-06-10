/*
gcc gstwebrtc.c -o gstwebrtc `pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0`
 */

#define GST_USE_UNSTABLE_API

#include <glib.h>
#include <glib-unix.h>
#include <libsoup/soup.h>
#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/webrtc/webrtc.h>
#include <gst/gst.h>
#include <stdio.h>
#include <string.h>
#include <json-glib/json-glib.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/rtp/gstrtcpbuffer.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#define BILLION 1000000000L

#define STUN_SERVER "stun.l.google.com:19302"
#define RTP_CAPS_OPUS "application/x-rtp,media=video,encoding-name=H264,payload="
#define TURN_SERVER "  turn-server=turn://usamawizard%40gmail.com:pa55w0rd@158.69.221.198:3478 "

static void start_pipeline(void);
static gchar *ws_server_addr = "192.168.0.105";
static gint ws_server_port = 8999;
static gchar *ourid;
static gchar *peerid;
static gboolean is_wss = FALSE;
static GMainLoop *main_loop;
GError *error = NULL;
int rtptime = 1;
GOptionContext *context;
GstElement *webrtc;
SoupWebsocketConnection *connection;
gint64 RTCPNTPtime, ntpns;
gint64 rtpdiff, ntpnschange1, clientclockdiffrece, ntpnschange;
guint32 RTCPRTPtime, RTCPRTPtimechange;
long int ns;
GstPipeline *gstpipline;
gint64 all;
time_t sec;
struct timespec spec;

static gboolean sig_handler(gpointer data)
{
  g_main_loop_quit(main_loop);
  return G_SOURCE_REMOVE;
}
static GstPadProbeReturn on_rtcp_callback(GstPad *pad, GstBuffer *info, gpointer *parent)
{
  gint32 packetcount, octatecount, ssrc;
  gint32 RTCPpacketcount;
  gboolean more;
  GstRTCPBuffer RTCPbuffer = {
      NULL,
  };
  GstRTCPPacket packet;

  info = gst_buffer_make_writable(info);

  g_print("rtcp callback\n");
  if (gst_rtcp_buffer_validate(info))
  {
    gst_rtcp_buffer_map(info, GST_MAP_READWRITE, &RTCPbuffer);

    more = gst_rtcp_buffer_get_first_packet(&RTCPbuffer, &packet);
    while (more)
    {
      if (gst_rtcp_packet_get_type(&packet) == GST_RTCP_TYPE_SR)
      {
        gst_rtcp_packet_sr_get_sender_info(&packet, &ssrc, &RTCPNTPtime, &RTCPRTPtime, &packetcount, &octatecount);
        //  gst_rtcp_packet_sr_set_sender_info(&packet, ssrc, RTCPNTPtime + 50000, RTCPRTPtime, packetcount, octatecount);
        ntpns = gst_rtcp_ntp_to_unix(RTCPNTPtime) ;
        clock_gettime(CLOCK_REALTIME, &spec);
        sec = spec.tv_sec;
        ns = spec.tv_nsec;

        all = (uint64_t)sec * BILLION + (uint64_t)ns;
        clientclockdiffrece = all - ntpns;
        printf("\n NTP %" G_GINT64_FORMAT " RTCPRTP %d clock diff %ld\n ", ntpns, RTCPRTPtime, all - ntpns);
        return GST_PAD_PROBE_OK;
      }
      more = gst_rtcp_packet_move_to_next(&packet);
    }
  }
}
static GstPadProbeReturn
get_rtp(GstPad *pad,
        GstPadProbeInfo *info,
        GstElement *pipe)
{
  gint x, y;
  GstMapInfo map;
  gint16 *ptr, t;
  GstBuffer *buffer;
  GstRTPBuffer rtp = {NULL};
  int a, b;
  gint32 ssrc;
  gint16 sequenceno;
  buffer = GST_PAD_PROBE_INFO_BUFFER(info);
  buffer = gst_buffer_make_writable(buffer);
  if (buffer == NULL)
    return GST_PAD_PROBE_OK;

  // a = GST_BUFFER_PTS(buffer);
  // b = GST_BUFFER_DTS(buffer);
  gst_rtp_buffer_map(buffer, GST_MAP_WRITE, &rtp);

  // global_clock = gst_system_clock_obtain();

  // GST_BUFFER_DTS (buffer)+=  dtimestamp;
  if (RTCPRTPtime != 0)
  {
    // if (abs(gst_rtp_buffer_get_timestamp(&rtp)) >= abs(RTCPRTPtime))
    // {
    //  g_print("changed to new NTP \n ");
    ntpnschange = ntpns;
    RTCPRTPtimechange = RTCPRTPtime; //+ 90000;
    RTCPRTPtime = 0;

    // }
  }
  if (rtptime != gst_rtp_buffer_get_timestamp(&rtp))
  {

    rtpdiff = (((int)(gst_rtp_buffer_get_timestamp(&rtp) - RTCPRTPtimechange) / 90000.0f) * 1000000000.0f);

    g_print("\ndfsaff%f\n", ((int)(gst_rtp_buffer_get_timestamp(&rtp) - RTCPRTPtimechange) / 90000.0f) * 1000000000);
  }
  if (4000000000 - (all - rtpdiff) > 0)
  {
    // gst_pipeline_set_latency(gstpipline, 4000000000-(all-rtpdiff));
    // GST_BUFFER_PTS (buffer)+=( 4000000000-(all-rtpdiff));
    //  GST_BUFFER_DTS (buffer)+=( 4000000000-(all-rtpdiff));
  }

  rtptime = gst_rtp_buffer_get_timestamp(&rtp);
  sequenceno = gst_rtp_buffer_get_seq(&rtp);
  clock_gettime(CLOCK_REALTIME, &spec);
  sec = spec.tv_sec;
  ns = spec.tv_nsec;

  all = (uint64_t)sec * BILLION + (uint64_t)ns;
  ntpnschange1 = rtpdiff + ntpns;
  // g_print("rtcp diff %ld  %ld\n ", rtpdiff,ntpns);

  printf("%d %ld %ld %ld \n", gst_rtp_buffer_get_timestamp(&rtp), rtpdiff, ntpnschange1, ((all - ntpnschange1)));
  // GST_BUFFER_PTS (buffer)+= (4000000000-abs(((all-clientclockdiffrece)-ntpnschange1)));
  // GST_BUFFER_DTS (buffer)+= 4000000000;

  // if(1000000000>((all-clientclockdiffrece)-ntpnschange1))
  // gst_pad_set_offset(pad,1000000000);

  // printf("asdf %ld",gst_clock_get_time(gst_pipeline_get_pipeline_clock(gstpipline))-gst_element_get_base_time(gst_bin_get_by_name(GST_BIN(pipe),"xvimagesink")));
  // printf("asfdaaaasdfffffff%lu",);
  gst_rtp_buffer_unmap(&rtp);

  //gst_pipeline_set_latency(gstpipline, 2000000000);
  //  GstMapInfo lsMap;
  //     if(!gst_buffer_map(buffer, &lsMap, GST_MAP_READ))
  //     {
  //        return GST_PAD_PROBE_OK;
  //     }
  //   unsigned short lnSeqNum;
  //     int lnSize = 2;
  //   //  printf("%ld",gst_buffer_get_size(buffer));
  //     int lnRetVal = gst_buffer_extract(buffer, 4, (gpointer)&lnSeqNum, 4);

  //    if(lnRetVal == 4)
  //    {
  //       //
  //       // reverse the bits of the sequence number
  //       //

  //       char * lpnReversedSeqNum = (char*)&lnSeqNum;

  //       unsigned long lnNewSeqNum = 0;
  //      ((char*)&lnNewSeqNum)[0] = lpnReversedSeqNum[3];
  //      ((char*)&lnNewSeqNum)[1] = lpnReversedSeqNum[2];
  //      ((char*)&lnNewSeqNum)[2] = lpnReversedSeqNum[1];
  //      ((char*)&lnNewSeqNum)[3] = lpnReversedSeqNum[0];

  //      printf(" %d \n", rtptime);

  //      //printf("%d %d  \n",rtptime,a);
  //    }

  //    gst_buffer_unmap(buffer, &lsMap);

  return GST_PAD_PROBE_OK;
}

static gchar *
get_string_from_json_object(JsonObject *object)
{
  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;
  /* Make it the root node */
  root = json_node_init_object(json_node_alloc(), object);
  generator = json_generator_new();
  json_generator_set_root(generator, root);
  text = json_generator_to_data(generator, NULL);

  /* Release everything */
  g_object_unref(generator);
  json_node_free(root);
  return text;
}
static void on_message(SoupWebsocketConnection *conn, gint type, GBytes *message, gpointer data)
{

  const gchar *text;
  JsonParser *json_parser;
  JsonObject *object;
  JsonObject *root_json_object;
  JsonNode *root_json;
  gsize size;
  gchar *data_string;
  JsonNode *root;
  const gchar *msg_type;
  const gchar *type_string;
  GstWebRTCSessionDescription *answersdp;
  GstPromise *promise;
  GstSDPMessage *sdp;
  const gchar *answer;
  int ret;
  gint mline_index;
  switch (type)
  {
  case SOUP_WEBSOCKET_DATA_BINARY:
    g_error("Received unknown binary message, ignoring\n");
    g_bytes_unref(message);
    return;

  case SOUP_WEBSOCKET_DATA_TEXT:
    data = g_bytes_unref_to_data(message, &size);
    /* Convert to NULL-terminated string */
    data_string = g_strndup(data, size);
    g_print("received text message: %s\n", (gchar *)data_string);

    g_free(data);
    break;

  default:
    g_assert_not_reached();
  }

  json_parser = json_parser_new();

  if (json_parser_load_from_data(json_parser, data_string, -1, NULL))
  {
    root = json_parser_get_root(json_parser);
    object = json_node_get_object(root);
    msg_type = json_object_get_string_member(object, "type");

    if (strcmp(msg_type, "answer") == 0)
    {

      answer = json_object_get_string_member(object, "sdp");
      g_print("andar aaya");
      g_print("%s\n", answer);
      ret = gst_sdp_message_new(&sdp);
      g_assert_cmphex(ret, ==, GST_SDP_OK);
      ret = gst_sdp_message_parse_buffer((guint8 *)answer, strlen(answer), sdp);
      if (ret != GST_SDP_OK)
      {
        g_error("Could not parse SDP string\n");
        exit(1);
      }
      answersdp = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER,
                                                     sdp);
      promise = gst_promise_new();
      g_signal_emit_by_name(webrtc, "set-remote-description",
                            answersdp, promise);
      gst_promise_interrupt(promise);
    }
    if (strcmp(msg_type, "ice") == 0)
    {
      type_string = json_object_get_string_member(object, "candidate");
      g_print("%s\n", type_string);
      gint mline_index = json_object_get_int_member(object, "sdpMLineIndex");
      g_print("ice %d\n", mline_index);
      g_signal_emit_by_name(webrtc, "add-ice-candidate", 1, type_string);
    }
  }
}

static void on_close(SoupWebsocketConnection *conn, gpointer data)
{
  soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
  g_print("WebSocket connection closed\n");
}

static void on_connection(SoupSession *session, GAsyncResult *res, gpointer data)
{
  SoupWebsocketConnection *conn;
  GError *error = NULL;
  JsonObject *registerme;

  conn = soup_session_websocket_connect_finish(session, res, &error);
  connection = conn;
  gchar *JSONmyidstr;
  if (error)
  {
    g_print("Error: %s\n", error->message);
    g_error_free(error);
    g_main_loop_quit(main_loop);
    return;
  }

  g_signal_connect(conn, "message", G_CALLBACK(on_message), NULL);
  g_signal_connect(conn, "closed", G_CALLBACK(on_close), NULL);

  //  soup_websocket_connection_send_text(conn, (is_wss) ? "Hello Secure Websocket !" : "{\"Hello\" :\"Websocket\"}");
  registerme = json_object_new();
  json_object_set_string_member(registerme, "type", "registerme");
  json_object_set_string_member(registerme, "id", ourid);
  JSONmyidstr = get_string_from_json_object(registerme);
  soup_websocket_connection_send_text(conn, JSONmyidstr);

  start_pipeline();
}

static void websocket_connect()
{

  g_unix_signal_add(SIGINT, (GSourceFunc)sig_handler, NULL);

  SoupSession *session;
  SoupMessage *msg;
  gchar *uri = NULL;
  session = soup_session_new();

  uri = g_strdup_printf("%s://%s:%d", "ws", ws_server_addr, ws_server_port);

  msg = soup_message_new(SOUP_METHOD_GET, uri);
  g_free(uri);

  soup_session_websocket_connect_async(
      session,
      msg,
      NULL, NULL, NULL,
      (GAsyncReadyCallback)on_connection,
      NULL);
}
static void on_offer_created(GstPromise *promise, gpointer data)
{
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply;
  gchar *desc;
  gchar *sdp_string;
  gchar *json_string;
  reply = gst_promise_get_reply(promise);
  gst_structure_get(reply, "offer",
                    GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
                    &offer, NULL);
  gst_promise_unref(promise);

  /* We can edit this offer before setting and sending */
  g_signal_emit_by_name(webrtc, "set-local-description", offer, NULL);

  /* Implement this and send offer to peer using signalling */
  //  send_sdp_offer (offer);
  sdp_string = gst_sdp_message_as_text(offer->sdp);
  g_print(" offer created:\n%s\n", sdp_string);
  JsonObject *sdp_json = json_object_new();
  json_object_set_string_member(sdp_json, "type", "offer");
  json_object_set_string_member(sdp_json, "sdp", sdp_string);
  json_object_set_string_member(sdp_json, "sdp", sdp_string);
  json_object_set_string_member(sdp_json, "from", ourid);
  json_object_set_string_member(sdp_json, "to", peerid);

  json_string = get_string_from_json_object(sdp_json);

  soup_websocket_connection_send_text(connection, json_string);

  gst_webrtc_session_description_free(offer);
}
static void
on_negotiation_needed(GstElement *webrtc, gpointer user_data)
{
  GstPromise *promise;
  g_print("negosiation  needed");
  promise = gst_promise_new_with_change_func(on_offer_created, user_data, NULL);
  g_signal_emit_by_name(webrtc, "create-offer", NULL, promise);
}
static void
handle_media_stream(GstPad *pad, GstElement *pipe, const char *convert_name,
                    const char *sink_name)
{
  GstPad *qpad;
  GstElement *q, *conv, *resample, *sink;
  GstPadLinkReturn ret;
  g_print("Trying to handle stream with %s ! %s", convert_name, sink_name);

  q = gst_element_factory_make("queue", NULL);
  g_assert_nonnull(q);
  conv = gst_element_factory_make(convert_name, NULL);
  g_assert_nonnull(conv);
  sink = gst_element_factory_make(sink_name, sink_name);
  //  g_object_set(sink, "ts-offset", 1000000000, NULL);
  g_assert_nonnull(sink);

  if (g_strcmp0(convert_name, "audioconvert") == 0)
  {
    /* Might also need to resample, so add it just in case.
     * Will be a no-op if it's not required. */
    resample = gst_element_factory_make("audioresample", NULL);
    g_assert_nonnull(resample);
    gst_bin_add_many(GST_BIN(pipe), q, conv, resample, sink, NULL);
    gst_element_sync_state_with_parent(q);
    gst_element_sync_state_with_parent(conv);
    gst_element_sync_state_with_parent(resample);
    gst_element_sync_state_with_parent(sink);
    gst_element_link_many(q, conv, resample, sink, NULL);
  }
  else
  {
    gst_bin_add_many(GST_BIN(pipe), q, conv, sink, NULL);
    gst_element_sync_state_with_parent(q);
    gst_element_sync_state_with_parent(conv);
    gst_element_sync_state_with_parent(sink);
    gst_element_link_many(q, conv, sink, NULL);
  }

  qpad = gst_element_get_static_pad(q, "sink");
 // gst_pad_set_offset(qpad,1000000000);
  ret = gst_pad_link(pad, qpad);
  g_assert_cmphex(ret, ==, GST_PAD_LINK_OK);
}
static void
on_incoming_decodebin_stream(GstElement *decodebin, GstPad *pad,
                             GstElement *pipe)
{
  GstCaps *caps;
  GstElement *rtpbin;
  GObject *session;

  const gchar *name;

  if (!gst_pad_has_current_caps(pad))
  {
    g_printerr("Pad '%s' has no caps, can't do anything, ignoring\n",
               GST_PAD_NAME(pad));
    return;
  }
// gst_pad_set_offset(pad,1000000000);
  caps = gst_pad_get_current_caps(pad);
  name = gst_structure_get_name(gst_caps_get_structure(caps, 0));

  if (g_str_has_prefix(name, "video"))
  {
    handle_media_stream(pad, pipe, "videoconvert", "xvimagesink");
  }
  else if (g_str_has_prefix(name, "audio"))
  {
    handle_media_stream(pad, pipe, "audioconvert", "alsasink");
  }
  else
  {
    g_printerr("Unknown pad %s, ignoring", GST_PAD_NAME(pad));
  }
  rtpbin = gst_bin_get_by_name(GST_BIN(webrtc), "rtpbin");
  g_object_set(G_OBJECT(rtpbin), "latency", 1, NULL);
  g_object_set(G_OBJECT(rtpbin), "ntp-sync", 1, NULL);

  // g_object_set(G_OBJECT(rtpbin), "drop-on-latency", TRUE, NULL);
  /// g_object_set(G_OBJECT(rtpbin), "do-lost", TRUE, NULL);
  // g_object_set(G_OBJECT(rtpbin), "buffer-mode ", 0, NULL);

  g_signal_emit_by_name(rtpbin, "get-internal-session", 1, &session);
  g_signal_connect_after(session, "on-receiving-rtcp",
                         G_CALLBACK(on_rtcp_callback), NULL);
}
static void
on_incoming_stream(GstElement *webrtc, GstPad *pad,
                   GstElement *pipe)
{
  GstElement *decodebin, *rtph264depay, *h264parse;
  GstPad *sinkpad, *srcpad;
  GstElement *rtpjitterbuffer;

  if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC)
    return;
  gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                    (GstPadProbeCallback)get_rtp, pipe, NULL);
  decodebin = gst_element_factory_make("decodebin", NULL);
  g_signal_connect(decodebin, "pad-added",
                   G_CALLBACK(on_incoming_decodebin_stream), pipe);
  gst_bin_add(GST_BIN(pipe), decodebin);
  gst_element_sync_state_with_parent(decodebin);
  rtph264depay = gst_element_factory_make("rtph264depay", NULL);
  h264parse = gst_element_factory_make("h264parse", "h264parse");
  rtpjitterbuffer = gst_element_factory_make("rtpjitterbuffer", NULL);
  // g_object_set(rtpjitterbuffer, "ts-offset", 100, NULL);
  gst_bin_add_many(GST_BIN(pipe), rtpjitterbuffer, rtph264depay, h264parse, NULL);
  sinkpad = gst_element_get_static_pad(decodebin, "sink");
  g_assert(gst_pad_link(pad, sinkpad) == GST_PAD_LINK_OK);

  // gst_element_link_many(rtpjitterbuffer,rtph264depay,h264parse, NULL);

  // srcpad= gst_element_get_static_pad(h264parse, "src");
  // sinkpad= gst_element_get_static_pad(decodebin, "sink");

  // g_assert(gst_pad_link(srcpad, sinkpad)==GST_PAD_LINK_OK);

  // gst_object_unref(sinkpad);
}

static void send_ice_candidate_message(GstElement *webrtcbin, guint mline_index,
                                       gchar *candidate, gpointer user_data)
{
  JsonObject *ice_json;
  JsonObject *ice_data_json;
  gchar *json_string;
  // ReceiverEntry *receiver_entry = (ReceiverEntry *) user_data;

  ice_json = json_object_new();
  json_object_set_string_member(ice_json, "type", "ice");
  json_object_set_string_member(ice_json, "to", peerid);

  ice_data_json = json_object_new();
  json_object_set_int_member(ice_data_json, "sdpMLineIndex", mline_index);
  json_object_set_string_member(ice_data_json, "candidate", candidate);
  json_object_set_object_member(ice_json, "data", ice_data_json);

  json_string = get_string_from_json_object(ice_json);
  json_object_unref(ice_json);

  soup_websocket_connection_send_text(connection, json_string);
  g_free(json_string);
}

static void start_pipeline()
{
  GstElement *pipe;
  pipe = gst_parse_launch("webrtcbin name=webrtcbin stun-server=stun://" STUN_SERVER TURN_SERVER
                          "v4l2src  ! videoconvert  ! queue ! x264enc tune=zerolatency ! rtph264pay ! "
                          "queue ! " RTP_CAPS_OPUS "96 ! webrtcbin. ",
                          NULL);
  gstpipline = GST_PIPELINE(pipe);
  webrtc = gst_bin_get_by_name(GST_BIN(pipe), "webrtcbin");

  GstCaps *caps = gst_caps_from_string("application/x-rtp,media=video,encoding-name=H264/9000,payload=96");
  g_signal_emit_by_name(webrtc, "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, caps);

  g_assert(webrtc != NULL);
  g_signal_connect(webrtc, "on-negotiation-needed", G_CALLBACK(on_negotiation_needed), NULL);

  g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(send_ice_candidate_message), NULL);
  g_signal_connect(webrtc, "pad-added", G_CALLBACK(on_incoming_stream), pipe);
  // gst_element_set_state (pipe, GST_STATE_READY);

  gst_element_set_state(pipe, GST_STATE_PLAYING);
}
gint main(gint argc, gchar **argv)
{

  gst_init(&argc, &argv);

  if (argc >= 3)
  {
    peerid = argv[1];
    ourid = argv[2];
  }
  g_print("start main loop\n");
  main_loop = g_main_loop_new(NULL, FALSE);

  websocket_connect();

  g_main_loop_run(main_loop);

  g_main_loop_unref(main_loop);
  return 0;
}
