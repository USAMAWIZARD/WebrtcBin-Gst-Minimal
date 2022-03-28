/*
gcc gstwebrtc.c -o gstwebrtc `pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0`
 */

#define GST_USE_UNSTABLE_API
#define RTP_CAPS_VP8 "application/x-rtp,media=video,encoding-name=H264,payload="
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

#define STUN_SERVER " stun-server=stun://stun.l.google.com:19302 "
#define RTP_CAPS_OPUS "application/x-rtp,media=audio,encoding-name=OPUS,payload="
static void start_pipeline(void);
static gchar *ws_server_addr = "52.90.173.248";
static gint ws_server_port = 8999;
static gchar *ourid;
const gchar *offer;
static gchar *peerid;
static gboolean is_wss = FALSE;
static GMainLoop *main_loop;
GError *error = NULL;
guint64 rtptime = 1;
GOptionContext *context;
GstElement *webrtc;
SoupWebsocketConnection *connection;
guint64 RTCPNTPtime, ntpns, rtpdiff, ntpnschange, ntpnschange1, clientclockdiffrece;
guint32 RTCPRTPtime, RTCPRTPtimechange;
long int ns;
GstPipeline *gstpipline;
guint64 all;
time_t sec;
struct timespec spec;

static gboolean sig_handler(gpointer data)
{
    g_main_loop_quit(main_loop);
    return G_SOURCE_REMOVE;
}
static GstPadProbeReturn on_rtcp_callback(GstPad *pad, GstBuffer *info, gpointer *parent)
{
    guint32 packetcount, octatecount, ssrc;
    guint32 RTCPpacketcount;
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
                ntpns = gst_rtcp_ntp_to_unix(RTCPNTPtime) - 1000000000;
                clock_gettime(CLOCK_REALTIME, &spec);
                sec = spec.tv_sec;
                ns = spec.tv_nsec;

                all = (uint64_t)sec * BILLION + (uint64_t)ns;
                clientclockdiffrece = all - ntpns;
                printf("\n NTP %" G_GUINT64_FORMAT " RTCPRTP %d clock diff %ld\n ", ntpns, RTCPRTPtime, all - ntpns);
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
    guint16 *ptr, t;
    GstBuffer *buffer;
    GstRTPBuffer rtp = {NULL};
    int a, b;
    guint32 ssrc;
    guint16 sequenceno;
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

        rtpdiff = (int)(((float)(abs(gst_rtp_buffer_get_timestamp(&rtp)) - abs(RTCPRTPtimechange)) / 90000) * 1000000000);

        // g_print("\ndfsafff%d %d\n",abs(gst_rtp_buffer_get_timestamp(&rtp)) , abs(RTCPRTPtimechange));
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
    ntpnschange1 = llabs(rtpdiff) + ntpns;
    // g_print("rtcp diff %ld  %ld\n ", rtpdiff,ntpns);

    printf("%d %ld %ld %ld \n", RTCPRTPtimechange, rtpdiff, all - clientclockdiffrece, ((all - ntpnschange1 - clientclockdiffrece)));
    // GST_BUFFER_PTS (buffer)+= (4000000000-abs(((all-clientclockdiffrece)-ntpnschange1)));
    // GST_BUFFER_DTS (buffer)+= 4000000000;

    // if(1000000000>((all-clientclockdiffrece)-ntpnschange1))
    // gst_pad_set_offset(pad,1000000000);

    // printf("asdf %ld",gst_clock_get_time(gst_pipeline_get_pipeline_clock(gstpipline))-gst_element_get_base_time(gst_bin_get_by_name(GST_BIN(pipe),"xvimagesink")));
    // printf("asfdaaaasdfffffff%lu",);
    gst_rtp_buffer_unmap(&rtp);

    gst_pipeline_set_latency(gstpipline, 2000000000);
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
static void
on_answer_created(GstPromise *promise, gpointer to)
{
    gchar *text;
    JsonObject *sdp_answer;
    GstWebRTCSessionDescription *answer = NULL;
    const GstStructure *reply;

    g_assert_cmphex(gst_promise_wait(promise), ==, GST_PROMISE_RESULT_REPLIED);
    reply = gst_promise_get_reply(promise);
    gst_structure_get(reply, "answer",
                      GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
    gst_promise_unref(promise);

    promise = gst_promise_new();
    g_signal_emit_by_name(webrtc, "set-local-description", answer, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);

    /* Send answer to peer */
    text = gst_sdp_message_as_text(answer->sdp);
    sdp_answer = json_object_new();
    json_object_set_string_member(sdp_answer, "sdp", text);
    json_object_set_string_member(sdp_answer, "type", "answer");
    json_object_set_string_member(sdp_answer, "to", to);
    text = get_string_from_json_object(sdp_answer);
    soup_websocket_connection_send_text(connection, text);
    printf("local description set");
    gst_webrtc_session_description_free(answer);
}
static void
on_offer_set(GstPromise *promise, gpointer to)
{
    gst_promise_unref(promise);
    promise = gst_promise_new_with_change_func(on_answer_created, to, NULL);
    g_signal_emit_by_name(webrtc, "create-answer", NULL, promise);
}

static void send_ice_candidate_message(GstElement *webrtcbin, guint mline_index,
                                       gchar *candidate, gpointer to)
{
    JsonObject *ice_json;
    JsonObject *ice_data_json;
    gchar *json_string;
    // ReceiverEntry *receiver_entry = (ReceiverEntry *) user_data;

    ice_json = json_object_new();
    json_object_set_string_member(ice_json, "type", "ice");
    json_object_set_string_member(ice_json, "to", to);

    ice_data_json = json_object_new();
    json_object_set_int_member(ice_data_json, "sdpMLineIndex", mline_index);
    json_object_set_string_member(ice_data_json, "candidate", candidate);
    json_object_set_object_member(ice_json, "data", ice_data_json);

    json_string = get_string_from_json_object(ice_json);
    json_object_unref(ice_json);

    soup_websocket_connection_send_text(connection, json_string);
    g_free(json_string);
}
void on_message(SoupWebsocketConnection *conn, gint type, GBytes *message, gpointer data)
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
    GstWebRTCSessionDescription *offersdp;
    GstPromise *promise;
    GstSDPMessage *sdp;
    const gchar *to, *candidate;

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
        if (strcmp(msg_type, "offer") == 0)
        {
            g_signal_connect(webrtc, "on-ice-candidate", G_CALLBACK(send_ice_candidate_message), (gchar *)json_object_get_string_member(object, "from"));

            offer = json_object_get_string_member(object, "sdp");
            g_print("andar aaya");
            g_print("%s\n", offer);
            ret = gst_sdp_message_new(&sdp);
            g_assert_cmphex(ret, ==, GST_SDP_OK);
            ret = gst_sdp_message_parse_buffer((guint8 *)offer, strlen(offer), sdp);
            if (ret != GST_SDP_OK)
            {
                g_error("Could not parse SDP string\n");
                exit(1);
            }
            offersdp = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
            to = json_object_get_string_member(object, "from");
            printf("%s to is this\n", to);
            promise = gst_promise_new_with_change_func(on_offer_set, (gchar *)to, NULL);
            g_signal_emit_by_name(webrtc, "set-remote-description",
                                  offersdp, promise);
            gst_promise_interrupt(promise);
        }
        if (strcmp(msg_type, "ice") == 0)
        {
            candidate = json_object_get_string_member(json_object_get_object_member(object, "data"), "candidate");
            gint mline_index = json_object_get_int_member(json_object_get_object_member(object, "data"), "sdpMLineIndex");
            g_print("ice %d\n", mline_index);
            g_signal_emit_by_name(webrtc, "add-ice-candidate", mline_index, candidate);
        }
    }
}

static void on_close(SoupWebsocketConnection *conn, gpointer data)
{
    soup_websocket_connection_close(conn, SOUP_WEBSOCKET_CLOSE_NORMAL, NULL);
    g_print("WebSocket connection closed\n");
}

void on_connection(SoupSession *session, GAsyncResult *res, gpointer data)
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
    ourid = "10";
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
// static void on_offer_created(GstPromise *promise, gpointer data)
// {
//     GstWebRTCSessionDescription *offer = NULL;
//     const GstStructure *reply;
//     gchar *desc;
//     gchar *sdp_string;
//     gchar *json_string;
//     reply = gst_promise_get_reply(promise);
//     gst_structure_get(reply, "offer",
//                       GST_TYPE_WEBRTC_SESSION_DESCRIPTION,
//                       &offer, NULL);
//     gst_promise_unref(promise);

//     /* We can edit this offer before setting and sending */
//     g_signal_emit_by_name(webrtc, "set-local-description", offer, NULL);

//     /* Implement this and send offer to peer using signalling */
//     //  send_sdp_offer (offer);
//     sdp_string = gst_sdp_message_as_text(offer->sdp);
//     g_print(" offer created:\n%s\n", sdp_string);
//     JsonObject *sdp_json = json_object_new();
//     json_object_set_string_member(sdp_json, "type", "offer");
//     json_object_set_string_member(sdp_json, "sdp", sdp_string);
//     json_object_set_string_member(sdp_json, "sdp", sdp_string);
//     json_object_set_string_member(sdp_json, "from", ourid);
//     json_object_set_string_member(sdp_json, "to", peerid);

//     json_string = get_string_from_json_object(sdp_json);

//     soup_websocket_connection_send_text(connection, json_string);

//     gst_webrtc_session_description_free(offer);
// }
static void
on_negotiation_needed(GstElement *webrtc, gpointer user_data)
{
    GstPromise *promise;
    g_print("negosiation  needed");
    // promise = gst_promise_new_with_change_func(on_offer_created, user_data, NULL);
}

static void start_pipeline()
{
    GstElement *pipe;
      GArray *transceivers;
  GstWebRTCRTPTransceiver *trans;

    pipe =
        gst_parse_launch("webrtcbin  name=sendrecv " STUN_SERVER
                         "videotestsrc is-live=true pattern=ball ! videoconvert ! queue ! x264enc ! rtph264pay ! "
                         "queue ! " RTP_CAPS_VP8 "96 ! sendrecv. ",
                         &error);

    // printf("%s\n", error->message);
    //   exit(1);

    gstpipline = GST_PIPELINE(pipe);
    webrtc = gst_bin_get_by_name(GST_BIN(pipe), "sendrecv");

    GstCaps *caps = gst_caps_from_string("application/x-rtp,media=video,encoding-name=H264/9000,payload=96");
  g_signal_emit_by_name (webrtc, "get-transceivers",
      &transceivers);
  g_assert (transceivers != NULL && transceivers->len > 0);
  trans = g_array_index (transceivers, GstWebRTCRTPTransceiver *, 0);
  trans->direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
  g_array_unref (transceivers);
    g_assert(webrtc != NULL);
    g_signal_connect(webrtc, "on-negotiation-needed", G_CALLBACK(on_negotiation_needed), NULL);

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
