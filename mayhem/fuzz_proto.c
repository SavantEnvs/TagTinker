/*
 * Fuzzes the ESL protocol layer: barcode parsing and tag profile lookup, the
 * IR frame builders, and the RLE / raw image payload encoders. The first byte
 * picks the operation, the rest is that operation's input.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "protocol/tagtinker_proto.h"

static uint8_t pixel_cb(size_t idx, void* ctx) {
    return ((const uint8_t*)ctx)[idx];
}

static void run_barcode(const uint8_t* d, size_t n) {
    char* bc = malloc(n + 1);
    if(!bc) return;
    memcpy(bc, d, n);
    bc[n] = '\0';

    uint8_t plid[4] = {0};
    uint16_t type = 0;
    TagTinkerTagProfile profile;
    (void)tagtinker_is_barcode_valid(bc);
    (void)tagtinker_barcode_to_type(bc, &type);
    (void)tagtinker_type_is_known(type);
    if(tagtinker_barcode_to_profile(bc, &profile)) {
        uint16_t gw, gh;
        tagtinker_profile_glass_size(&profile, &gw, &gh);
    }
    if(tagtinker_barcode_to_plid(bc, plid)) {
        uint8_t buf[TAGTINKER_MAX_FRAME_SIZE];
        uint8_t chunk[TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME];
        memset(chunk, plid[0], sizeof(chunk));
        (void)tagtinker_make_ping_frame(buf, plid);
        (void)tagtinker_make_wake_frame(buf, plid);
        (void)tagtinker_make_refresh_frame(buf, plid);
        (void)tagtinker_make_image_param_frame(
            buf, plid, plid[0] << 8 | plid[1], plid[2], plid[3], type, type, plid[0], plid[1]);
        (void)tagtinker_make_image_data_frame(buf, plid, type, chunk);
    }
    free(bc);
}

static void run_payload(
    const uint8_t* d, size_t n, TagTinkerCompressionMode mode, bool color_clear) {
    TagTinkerImagePayload payload;
    /* width x 1 strip: pixel count is exactly what the buffer holds (n <= 65535). */
    if(tagtinker_encode_image_payload(d, (uint16_t)n, 1, color_clear, mode, &payload))
        tagtinker_free_image_payload(&payload);
    if(tagtinker_encode_fn_payload(pixel_cb, (void*)d, n, mode, &payload))
        tagtinker_free_image_payload(&payload);
    if(tagtinker_encode_planes_payload(d, d + n / 2, n / 2, mode, &payload))
        tagtinker_free_image_payload(&payload);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if(size < 2 || size > 65535) return -1;
    uint8_t op = data[0] & 3;
    uint8_t arg = data[0] >> 2;
    const uint8_t* d = data + 1;
    size_t n = size - 1;

    switch(op) {
    case 0:
        run_barcode(d, n);
        break;
    case 1:
        run_payload(d, n, (TagTinkerCompressionMode)(arg % 3), arg & 4);
        break;
    case 2: {
        /* tagtinker_make_addressed_frame reads payload[0], so it needs one byte;
         * the frame is 5 + len + 2 bytes and the app's buffer is MAX_FRAME_SIZE. */
        if(n < 5 || n - 4 > TAGTINKER_MAX_FRAME_SIZE - 7) return -1;
        uint8_t buf[TAGTINKER_MAX_FRAME_SIZE];
        (void)tagtinker_make_addressed_frame(buf, d, d + 4, n - 4);
        break;
    }
    default: {
        uint8_t buf[TAGTINKER_MAX_FRAME_SIZE];
        (void)tagtinker_build_broadcast_page_frame(buf, d[0], d[0] & 0x80, (uint16_t)(arg << 8 | d[0]));
        (void)tagtinker_build_broadcast_debug_frame(buf);
        (void)tagtinker_crc16(d, n);
        break;
    }
    }
    return 0;
}
