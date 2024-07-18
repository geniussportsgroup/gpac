
#ifndef _GF_ID3_H_
#define _GF_ID3_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/bitstream.h>

// First 36 bytes of a Nielsen ID3 tag: "ID3\x04\x00 \x00\x00\x02\x05PRIV\x00\x00\x01{\x00\x00www.nielsen.com/"
static const u32 NIELSEN_ID3_TAG_PREFIX_LEN = 36;
static const u8 NIELSEN_ID3_TAG_PREFIX[] = {0x49, 0x44, 0x33, 0x04, 0x00, 0x20,
											0x00, 0x00, 0x02, 0x05, 0x50, 0x52,
											0x49, 0x56, 0x00, 0x00, 0x01, 0x7B,
											0x00, 0x00, 0x77, 0x77, 0x77, 0x2E,
											0x6E, 0x69, 0x65, 0x6C, 0x73, 0x65,
											0x6E, 0x2E, 0x63, 0x6F, 0x6D, 0x2F};

static const char *ID3_PROP_SCHEME_URI = "https://aomedia.org/emsg/ID3";
static const char *ID3_PROP_VALUE_URI_NIELSEN = "www.nielsen.com:id3:v1";
static const char *ID3_PROP_VALUE_URI_DEFAULT = "https://aomedia.org/emsg/ID3";

typedef struct {
    u32 timescale;
    u64 pts;
    char* scheme_uri;
    char* value_uri;
    u32 data_length;
    u8* data;
} GF_ID3_TAG;

GF_Err id3_tag_new(GF_ID3_TAG *tag, u32 timescale, u64 pts, u8 *data, u32 data_length);

void id3_tag_free(GF_ID3_TAG *tag);

void id3_to_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs);

void id3_from_bitstream(GF_ID3_TAG *tag, GF_BitStream *bs);

#ifdef __cplusplus
}
#endif

#endif // _GF_ID3_H_