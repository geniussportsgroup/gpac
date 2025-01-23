#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/bitstream.h>
#include <gpac/webvtt.h>
#include <gpac/mpegts.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>

#include <stdio.h>

#ifndef GPAC_DISABLE_VTT


#define REFRAME_TS_WVTT_DEFAULT_TIMESCALE 90000

///////////////////////////////////////////////////////////////////////////////

typedef struct
{
    u32 timescale;
    GF_FilterPid *ipid;
    GF_FilterPid *opid;
} GF_ReframeTsVttCtx;

///////////////////////////////////////////////////////////////////////////////
// Callbacks for WebVTT parser

static GF_Err reframe_ts_wvtt_parse_callback_report(void *user, GF_Err e, char *message, const char *line) {
    GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_parse_callback_report: line: %s, message: %s\n", line, message));
    return e;
}

static void reframe_ts_wvtt_parse_callback_header(void *user, const char *config)
{
    // nothing to do
}

static void reframe_ts_wvtt_parse_callback_sample(void *user, GF_WebVTTSample *sample) {

    if (!sample) {
        return;
    }

    // in milliseconds
    u64 start = gf_webvtt_sample_get_start(sample);
    u64 end = gf_webvtt_sample_get_end(sample);

    if (!gf_isom_webvtt_cues_count(sample)) {
        return;
    }

    GF_ISOSample *iso_sample = NULL;
    iso_sample = gf_isom_webvtt_to_sample(sample);

    if (iso_sample) {
        GF_ReframeTsVttCtx* ctx = (GF_ReframeTsVttCtx*) user;
        GF_FilterPacket *pck;
        u8 *pck_data;

        pck = gf_filter_pck_new_alloc(ctx->opid, iso_sample->dataLength, &pck_data);
        if (pck)
        {
            memcpy(pck_data, iso_sample->data, iso_sample->dataLength);
            gf_filter_pck_set_cts(pck, (u64)(ctx->timescale * start / 1000));
            gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);

            if (end && (end >= start))
            {
                gf_filter_pck_set_duration(pck, (u32)(ctx->timescale * (end - start) / 1000));
            }
            gf_filter_pck_send(pck);
        }

        gf_isom_sample_del(&iso_sample);
    }

    gf_webvtt_sample_del(sample);
}


///////////////////////////////////////////////////////////////////////////////
// Filter callbacks

static GF_Err reframe_ts_wvtt_initialize(GF_Filter *filter)
{
    return GF_OK;
}

GF_Err reframe_ts_wvtt_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
    GF_ReframeTsVttCtx *ctx = gf_filter_get_udta(filter);

    if (is_remove)
    {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: is_remove\n"));
        ctx->ipid = NULL;
        if (ctx->opid) {
            gf_filter_pid_remove(ctx->opid);
            ctx->opid = NULL;
        }
        return GF_OK;
    }

    if (!gf_filter_pid_check_caps(pid)) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: not supported\n"));
        return GF_NOT_SUPPORTED;
    }

    const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
    if (!p) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: no codec id\n"));
        return GF_NOT_SUPPORTED;
    }

    const GF_PropertyValue* timescale = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
    if (!timescale) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: no timescale using default value\n"));
        ctx->timescale = REFRAME_TS_WVTT_DEFAULT_TIMESCALE;
    } else {
        ctx->timescale = timescale->value.uint;
    }

    ctx->ipid = pid;
    ctx->opid = gf_filter_pid_new(filter);

    // configure output PID properties
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((u8 *)"WEBVTT", 7));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_DATA((u8 *)"text/vtt", 9));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_WEBVTT));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_FALSE));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(ctx->timescale));

    return GF_OK;
}

GF_Err reframe_ts_wvtt_process(GF_Filter *filter)
{
    GF_ReframeTsVttCtx *ctx = gf_filter_get_udta(filter);
    GF_FilterPacket *pck = NULL;
    u32 pck_size;
    u8 *pck_data = NULL;

    pck = gf_filter_pid_get_packet(ctx->ipid);
    if (!pck) {
        if (gf_filter_pid_is_eos(ctx->ipid)) {
            gf_filter_pid_set_eos(ctx->opid);
            return GF_EOS;
        }
        return GF_OK;
    }

    pck_data = (char *)gf_filter_pck_get_data(pck, &pck_size);


    ///////////////////////////////////////////////////////
    GF_WebVTTParser *parser = gf_webvtt_parser_new();

    // create a memory file to read the packet data
    FILE *mem_file = fmemopen(pck_data, pck_size, "r");

    // Assume UTF-8 encoding
    gf_webvtt_parser_init(parser, mem_file, 0, GF_FALSE, ctx,  reframe_ts_wvtt_parse_callback_report, reframe_ts_wvtt_parse_callback_sample, reframe_ts_wvtt_parse_callback_header);

    // as the input packets do not contain the WEBVTT signature at the beginning
    // we have to force the parser to start looking for cues immediately
    gf_webvtt_parser_force_state(parser, WEBVTT_PARSER_STATE_WAITING_CUE);

    // the parser will call reframe_ts_wvtt_parse_callback_sample for each sample found
    gf_webvtt_parser_parse(parser);
    gf_webvtt_parser_del(parser);

    ///////////////////////////////////////////////////////
    // cleanup
    gf_filter_pid_drop_packet(ctx->ipid);
    fclose(mem_file);

    return GF_OK;
}

static void reframe_ts_wvtt_finalize(GF_Filter *filter)
{
    // nothing to do
}

static const GF_FilterCapability ReframeTsVttCaps[] =
{
        // receive a text stream using the custom 4CC code for WVTT in TS
        CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
        CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_M2TS_META_WVTT),

        // then, produce a Metadata stream with WebVTT cues
        CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
        CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
        CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_FALSE),
        {0},
};

#define OFFS(_n) #_n, offsetof(GF_ReframeTsVttCtx, _n)

GF_FilterRegister ReframeTsWebVTT = {
    .name = "rftsvtt",
    GF_FS_SET_DESCRIPTION("Reframer for WebVTT subtitles in Transport Stream")
    GF_FS_SET_HELP("Transform Transport Stream PES metadata content to WebVTT cues")
    .private_size = sizeof(GF_ReframeTsVttCtx),
    .initialize = reframe_ts_wvtt_initialize,
    .finalize = reframe_ts_wvtt_finalize,
    SETCAPS(ReframeTsVttCaps),
    .configure_pid = reframe_ts_wvtt_configure_pid,
    .process = reframe_ts_wvtt_process};

const GF_FilterRegister *rftsvtt_register(GF_FilterSession *session)
{
    return &ReframeTsWebVTT;
}
#else
const GF_FilterRegister *rftsvtt_register(GF_FilterSession *session)
{
    return NULL;
}

#endif /*GPAC_DISABLE_VTT*/
