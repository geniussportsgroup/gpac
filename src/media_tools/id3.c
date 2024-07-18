#include <gpac/id3.h>

GF_Err id3_tag_new(GF_ID3_TAG *tag, u32 timescale, u64 pts, u8 *data, u32 data_length)
{
    if (data == NULL || data_length == 0) {
        return GF_BAD_PARAM;
    }

    tag->timescale = timescale;
    tag->pts = pts;
    tag->scheme_uri = gf_strdup(ID3_PROP_SCHEME_URI);

    // test if the data is a Nielsen ID3 tag
    if (memcmp(data, NIELSEN_ID3_TAG_PREFIX, NIELSEN_ID3_TAG_PREFIX_LEN) == 0)
    {
        tag->value_uri = gf_strdup(ID3_PROP_VALUE_URI_NIELSEN);
    }
    else
    {
        tag->value_uri = gf_strdup(ID3_PROP_VALUE_URI_DEFAULT);
    }

    tag->data = gf_malloc(data_length);
    memcpy(tag->data, data, data_length);

    return GF_OK;
}

void id3_tag_free(GF_ID3_TAG *tag)
{
    if (!tag) {
        return;
    }

    if (tag->scheme_uri) {
        gf_free(tag->scheme_uri);
        tag->scheme_uri = NULL;
    }

    if (tag->value_uri) {
        gf_free(tag->value_uri);
        tag->value_uri = NULL;
    }

    if (tag->data) {
        gf_free(tag->data);
        tag->data = NULL;
    }
}

void id3_to_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs)
{
    gf_bs_write_u32(bs, tag->timescale);
    gf_bs_write_u64(bs, tag->pts);

    u32 scheme_uri_len = strlen(tag->scheme_uri) + 1; // plus null character
    u32 value_uri_len = strlen(tag->value_uri) + 1;   // plus null character

    gf_bs_write_u32(bs, scheme_uri_len);
    gf_bs_write_data(bs, (const u8 *)tag->scheme_uri, scheme_uri_len);

    gf_bs_write_u32(bs, value_uri_len);
    gf_bs_write_data(bs, (const u8 *)tag->value_uri, value_uri_len);

    gf_bs_write_u32(bs, tag->data_length);
    gf_bs_write_data(bs, tag->data, tag->data_length);
}

void id3_from_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs)
{
    // TODO: aqui voy
}