/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / WebVTT stream unframer filter
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/bitstream.h>
#include <gpac/webvtt.h>
#include <gpac/internal/media_dev.h>
#include <gpac/internal/isomedia_dev.h>

#include <stdio.h>

#ifndef GPAC_DISABLE_VTT

///////////////////////////////////////////////////////////////////////////////

typedef struct
{
    // // opts
    // Bool exporter, merge_cues;

    // // only one input pid declared
    // GF_FilterPid *ipid;
    // // only one output pid declared
    // GF_FilterPid *opid;

    // u32 codecid;
    // u32 timescale;

    // GF_Fraction64 duration;
    // s64 delay;

    // u8 *cues_buffer;
    // u32 cues_buffer_size;

    // GF_WebVTTParser *parser;

    // GF_FilterPacket *src_pck;
    // Bool dash_mode;
    // u32 seg_pck_in, seg_pck_out;

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
    // GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_parse_callback_header: %s\n", config));
    // nothing to d
}

static void reframe_ts_wvtt_parse_callback_sample(void *user, GF_WebVTTSample *sample) {

    if (!sample) {
        return;
    }

    u64 start = gf_webvtt_sample_get_start(sample);
    u64 end = gf_webvtt_sample_get_end(sample);

    ///////////////////////////////////////////////////////
    // GF_List* cues = gf_webvtt_sample_get_cues(sample);

    // for (u32 i = 0; i < gf_list_count(cues); i++) {
    //     GF_WebVTTCue *cue = (GF_WebVTTCue *)gf_list_get(cues, i);
    //     GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_parse_callback_sample: %llu -> %llu cue: %u: %s\n", start, end, i, cue->text));
    //     // reframe_ts_wvtt_cue_callback(user, cue);
    // }
    ///////////////////////////////////////////////////////

    if (!gf_isom_webvtt_cues_count(sample)) {
        return;
    }

    // u64 start = gf_webvtt_sample_get_start(sample);
    // u64 end = gf_webvtt_sample_get_end(sample);
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

            // TODO: receive GF_PROP_PID_TIMESCALE
            gf_filter_pck_set_cts(pck, (u64)(90000 * start / 1000));
            gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);

            if (end && (end >= start))
            {
                // TODO: receive GF_PROP_PID_TIMESCALE
                gf_filter_pck_set_duration(pck, (u32)(90000 * (end - start) / 1000));
            }
            gf_filter_pck_send(pck);
        }

        gf_isom_sample_del(&iso_sample);
    }

    gf_webvtt_sample_del(sample);
}

static void reframe_ts_wvtt_cue_callback(void *user, GF_WebVTTCue *cue) {
    GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_cue_callback:\n"));
}



///////////////////////////////////////////////////////////////////////////////
// Filter callbacks

static GF_Err reframe_ts_wvtt_initialize(GF_Filter *filter)
{
    // GF_ReframeTsVttCtx *ctx = (GF_ReframeTsVttCtx *)gf_filter_get_udta(filter);

    // initialize any internal attribute in ctx
    // ctx->parser = gf_webvtt_parser_new();
    // gf_webvtt_parser_cue_callback(ctx->parser, reframe_ts_wvtt_cue_callback, ctx);

    return GF_OK;
}

GF_Err reframe_ts_wvtt_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
    GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid\n"));
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

    GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
    if (!p) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: no codec id\n"));
        return GF_NOT_SUPPORTED;
    }

    GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: OK\n"));

    char *pid_name = gf_filter_pid_get_name(pid);
    GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_configure_pid: PID name: %s\n", pid_name));

    ctx->ipid = pid;
    ctx->opid = gf_filter_pid_new(filter);

    // configure output PID properties
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA((u8 *)"WEBVTT", 7));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_WEBVTT));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_FALSE));
    gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000));

    return GF_OK;
}

GF_Err reframe_ts_wvtt_process(GF_Filter *filter)
{
    GF_ReframeTsVttCtx *ctx = gf_filter_get_udta(filter);
    GF_FilterPacket *pck = NULL;
    u32 pck_size;
    u8 *pck_data = NULL;
    // u64 start_ts, end_ts;
    
    // GF_List *cues;
    // Bool keep_ref = GF_TRUE;

    pck = gf_filter_pid_get_packet(ctx->ipid);
    if (!pck) {
        if (gf_filter_pid_is_eos(ctx->ipid)) {
            gf_filter_pid_set_eos(ctx->opid);
            return GF_EOS;
        }
        return GF_OK;
    }

    pck_data = (char *)gf_filter_pck_get_data(pck, &pck_size);
    // GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("reframe_ts_wvtt_process: len %d\n", pck_size));


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

    ///////////////////////////////////////////////////////

    // GF_FilterPacket *dst = gf_filter_pck_new_alloc(ctx->opid, 0, NULL);

    // u8 *dst_data = NULL;
    // GF_FilterPacket *dst_pck = gf_filter_pck_new_copy(ctx->opid, pck, &dst_data);

    // gf_filter_pck_send(dst_pck);
    // gf_filter_pid_drop_packet(ctx->ipid);

    return GF_OK;
}

static void reframe_ts_wvtt_finalize(GF_Filter *filter)
{
    // GF_ReframeTsVttCtx *ctx = gf_filter_get_udta(filter);
    // if (ctx->cues_buffer)
    //     gf_free(ctx->cues_buffer);

    // if (ctx->parser)
    //     gf_webvtt_parser_del(ctx->parser);

    // if (ctx->src_pck)
    // {
    //     gf_filter_pck_unref(ctx->src_pck);
    //     ctx->src_pck = NULL;
    // }
}

static const GF_FilterCapability ReframeTsVttCaps[] =
    {
        // receive a text stream without any codec
        CAP_UINT(GF_CAPS_INPUT_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
        CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),

        // TODO: then, produce a Metadata stream with WebVTT cues
        // CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
        // CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_SIMPLE_TEXT),
        CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_WEBVTT),
        CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_FALSE),
        // CAP_BOOL(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
        {0},

        // CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
        // CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
        // CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
        // CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
        // CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),

        // CAP_BOOL(GF_CAPS_INPUT, GF_PROP_PID_UNFRAMED, GF_TRUE), // without this caps, it connects to the TS demuxer
        // CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
        // CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_UNFRAMED, GF_TRUE),
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
