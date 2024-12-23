#include <gpac/filters.h>
#include <gpac/constants.h>

/////////////////////////////////////////////////
// TYPES
/////////////////////////////////////////////////

struct _passthrough_pid_ctx
{
    GF_FilterPid *input_pid;
    GF_FilterPid *output_pid;

    // any other attribute needed for this PID
};

struct _passthrough_ctx
{
    char *name;

    // "private" attributes
    GF_List *pids;
    Bool reconfigure;
};

/////////////////////////////////////////////////
// FORWARD DECLARATIONS
/////////////////////////////////////////////////

// GS: create filter: declare filter PID and Context types
typedef struct _passthrough_pid_ctx PassthroughPid;
typedef struct _passthrough_ctx GF_PassthroughCtx;

/////////////////////////////////////////////////
// METHOD IMPLEMENTATIONS
/////////////////////////////////////////////////

/**
 * GS: create filter: function to configure input PIDs into the filter.
 * @param filter a pointer to this filter instance.
 * @param pid input PID
 * @param is_remove whether or not the PID has been removed
 * @return
 */
static GF_Err passthrough_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
    // obtain the user-defined filter data from the filter parameter
    GF_PassthroughCtx *ctx = (GF_PassthroughCtx *)gf_filter_get_udta(filter);
    PassthroughPid *pidctx = NULL;

    if (!is_remove)
    {

        const char *pidName = gf_filter_pid_get_name(pid);
        // register a new PID
        GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] configure_pid: registering new PID: %5s\n", ctx->name, pidName))
        GF_SAFEALLOC(pidctx, PassthroughPid);
        if (!pidctx)
        {
            GF_LOG(GF_LOG_ERROR, GF_LOG_APP, ("[passthrough][%s] configure_pid: error registering new PID: %5s: bad allocation\n", ctx->name, pidName))
            return GF_OUT_OF_MEM;
        }

        // store the pid parameter as input
        pidctx->input_pid = pid;

        // create a new PID for the passthrough output
        pidctx->output_pid = gf_filter_pid_new(filter);

        // as the passthrough filter does nothing to the PID packets,
        // we copy all the properties of the input PID into the output.
        gf_filter_pid_copy_properties(pidctx->output_pid, pidctx->input_pid);

        // finally, add the newly created PID to the filters user-defined data
        gf_list_add(ctx->pids, pidctx);

        return GF_OK;
    }
    else
    {

        // delete the PID from the filter
        const u32 count = gf_list_count(ctx->pids);
        for (u32 i = 0; i < count; i++)
        {

            pidctx = (PassthroughPid *)gf_list_get(ctx->pids, i);

            // check if this PID context contains the input PID, and if so
            // deletes the entry from the filter context
            if (pidctx->input_pid == pid)
            {

                const char *pidName = gf_filter_pid_get_name(pid);
                // register a new PID
                GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] configure_pid: deleting PID: %5s\n", ctx->name, pidName))

                // delete the passthrough output as well. This will
                // trigger other filters down the chain to be reconfigured.
                gf_filter_pid_remove(pidctx->output_pid);

                // delete the item from the filter context
                gf_list_del_item(ctx->pids, pidctx);
                break;
            }
        }

        return GF_OK;
    }
}

/**
 * GS: create filter: callback function called when there is data available
 * in the input PIDs for processing.
 * @param filter
 * @return
 */
static GF_Err passthrough_process(GF_Filter *filter)
{
    // obtain the filter's internal user-data
    GF_PassthroughCtx *ctx = (GF_PassthroughCtx *)gf_filter_get_udta(filter);

    // iterate over all PIDs to check which ones have available data
    const u32 count = gf_list_count(ctx->pids);
    for (u32 i = 0; i < count; i++)
    {
        const PassthroughPid *pidctx = gf_list_get(ctx->pids, i);

        // read one packet from the input PID
        GF_FilterPacket *packet = gf_filter_pid_get_packet(pidctx->input_pid);
        if (packet != NULL)
        {

            u32 inputSizeBytes = 0;

            // DTS: Decoding Timestamp
            const u64 dts = gf_filter_pck_get_dts(packet);

            // this function returns a pointer to the packet data, but
            // we don't use it.
            gf_filter_pck_get_data(packet, &inputSizeBytes);

            // create a new packet, as a clone of the input, to be sent through the
            // output PID. All the properties of the input packet are copied, so we
            // do not need to set them manually.
            u8 *outData = NULL;
            GF_FilterPacket *outPacket = gf_filter_pck_new_clone(pidctx->output_pid, packet, &outData);
            gf_filter_pck_send(outPacket);

            // drop the packet from the input PID buffer, effectively marking it
            // as processed. This allows for new packets to arrive in the next
            // invocation of gf_filter_pid_get_packet
            gf_filter_pid_drop_packet(pidctx->input_pid);

            const char *pidName = gf_filter_pid_get_name(pidctx->input_pid);
            GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] process: PID: %4s, size: %6u, DTS: %6u\n", ctx->name, pidName, inputSizeBytes, dts))
        }
        // check if an EndOfStream signal has been received in the input PID
        // and if so, propagate it to the output.
        else if (gf_filter_pid_eos_received(pidctx->input_pid))
        {
            gf_filter_pid_set_eos(pidctx->output_pid);
        }
    }

    // processing was successful, return OK
    return GF_OK;
}

static GF_Err passthrough_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
    // TODO
    const GF_PassthroughCtx *ctx = (GF_PassthroughCtx *)gf_filter_get_udta(filter);

    GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] update_arg\n", ctx->name))
    return GF_OK;
}

/**
 * GS: create filter: initialize the filter's internal user data
 * @param filter
 * @return
 */
static GF_Err passthrough_initialize(GF_Filter *filter)
{
    GF_PassthroughCtx *ctx = (GF_PassthroughCtx *)gf_filter_get_udta(filter);

    GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] initialize\n", ctx->name))

    // initialize any internal attribute in ctx
    ctx->pids = gf_list_new();

    return GF_OK;
}

/**
 * GS: create filter: finalize the filter's internal user data
 *
 * @param filter
 */
static void passthrough_finalize(GF_Filter *filter)
{
    GF_PassthroughCtx *ctx = (GF_PassthroughCtx *)gf_filter_get_udta(filter);

    GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("[passthrough][%s] finalize\n", ctx->name))

    while (gf_list_count(ctx->pids))
    {
        PassthroughPid *pctx = gf_list_pop_back(ctx->pids);

        // FIXME: Adarve: should the output PID be removed here?

        // free memory
        gf_free(pctx);
    }
    gf_list_del(ctx->pids);
}

#define OFFS(_n) #_n, offsetof(GF_PassthroughCtx, _n)
static GF_FilterArgs PassthroughArgs[] =
    {
        {OFFS(name), "A name assigned to the passthrough filter.", GF_PROP_STRING, "", NULL, GF_FS_ARG_UPDATE},
        {0}};

static const GF_FilterCapability PassthroughCaps[] =
    {
        // FIXME: Adarve: figure out these
        CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
        CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
        CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
        CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
        CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

/**
 * GS: create filter: Registry struct with all pieces that form the filter.
 *
 */
GF_FilterRegister PassthroughRegister = {
    .name = "passthrough",
    GF_FS_SET_DESCRIPTION("Passthrough filter")
        GF_FS_SET_HELP("Passthrough FILTER HELP STRING\n")
            .private_size = sizeof(GF_PassthroughCtx),
    .max_extra_pids = 0xFFFFFFFF,
    .flags = GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_ALLOW_CYCLIC,
    .args = PassthroughArgs,
    SETCAPS(PassthroughCaps),
    .initialize = passthrough_initialize,
    .finalize = passthrough_finalize,
    .configure_pid = passthrough_configure_pid,
    .process = passthrough_process,
    .update_arg = passthrough_update_arg};

/**
 * GS: create filter: filter registration function
 *
 * This function is called in filter_register.c to include this filter
 * as part of the filter collection available to GPAC.
 *
 * @param session
 * @return
 */
const GF_FilterRegister *passthrough_register(GF_FilterSession *session)
{
    return (const GF_FilterRegister *)&PassthroughRegister;
}