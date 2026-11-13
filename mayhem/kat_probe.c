/*
 * Known-answer probe over the real protocol/ and nfc/ sources, for mayhem/test.sh.
 * Prints what the library computes for a given input; test.sh compares it with independently derived values.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol/tagtinker_proto.h"
#include "nfc/tagtinker_nfc.h"

static size_t unhex(const char* s, uint8_t* out, size_t cap) {
    size_t n = 0;
    while(s[0] && s[1] && n < cap) {
        unsigned v;
        if(sscanf(s, "%2x", &v) != 1) break;
        out[n++] = (uint8_t)v;
        s += 2;
    }
    return n;
}

static void hex(const char* label, const uint8_t* d, size_t n) {
    printf("%s=", label);
    for(size_t i = 0; i < n; i++) printf("%02x", d[i]);
    printf("\n");
}

int main(int argc, char** argv) {
    if(argc < 3) return 2;
    const char* cmd = argv[1];
    uint8_t buf[512];

    if(!strcmp(cmd, "crc")) {
        size_t n = unhex(argv[2], buf, sizeof(buf));
        printf("crc=%04x\n", tagtinker_crc16(buf, n));
    } else if(!strcmp(cmd, "barcode")) {
        uint8_t plid[4];
        uint16_t type = 0;
        TagTinkerTagProfile p;
        if(!tagtinker_barcode_to_plid(argv[2], plid)) { printf("invalid\n"); return 0; }
        tagtinker_barcode_to_type(argv[2], &type);
        hex("plid", plid, 4);
        printf("type=%u\n", type);
        if(tagtinker_barcode_to_profile(argv[2], &p))
            printf("profile=%ux%u kind=%d color=%d model=%s\n", p.width, p.height, p.kind, p.color, p.model_name);
        else
            printf("profile=unknown\n");
    } else if(!strcmp(cmd, "frames")) {
        uint8_t plid[4];
        if(!tagtinker_barcode_to_plid(argv[2], plid)) return 1;
        uint8_t chunk[TAGTINKER_IMAGE_DATA_BYTES_PER_FRAME];
        for(size_t i = 0; i < sizeof(chunk); i++) chunk[i] = (uint8_t)i;
        size_t n;
        n = tagtinker_make_ping_frame(buf, plid); hex("ping", buf, n);
        n = tagtinker_make_wake_frame(buf, plid); hex("wake", buf, n);
        n = tagtinker_make_refresh_frame(buf, plid); hex("refresh", buf, n);
        n = tagtinker_make_image_param_frame(buf, plid, 0x0140, 2, 2, 152, 296, 8, 16); hex("param", buf, n);
        n = tagtinker_make_image_data_frame(buf, plid, 0x0102, chunk); hex("data", buf, n);
        const uint8_t payload[] = {0x06, 0xAA, 0xBB};
        n = tagtinker_make_addressed_frame(buf, plid, payload, sizeof(payload)); hex("addr", buf, n);
        n = tagtinker_build_broadcast_page_frame(buf, 3, true, 0x1234); hex("bpage", buf, n);
        n = tagtinker_build_broadcast_debug_frame(buf); hex("bdebug", buf, n);
    } else if(!strcmp(cmd, "payload") && argc >= 4) {
        /* argv[2]: auto|raw|rle, argv[3]: string of '0'/'1' pixels */
        size_t n = strlen(argv[3]);
        uint8_t* px = malloc(n);
        for(size_t i = 0; i < n; i++) px[i] = argv[3][i] == '1';
        TagTinkerCompressionMode m = !strcmp(argv[2], "raw") ? TagTinkerCompressionRaw :
                                     !strcmp(argv[2], "rle") ? TagTinkerCompressionRle :
                                                               TagTinkerCompressionAuto;
        TagTinkerImagePayload pl;
        if(!tagtinker_encode_image_payload(px, (uint16_t)n, 1, false, m, &pl)) { printf("fail\n"); return 1; }
        printf("bytes=%zu comp=%u\n", pl.byte_count, pl.comp_type);
        hex("data", pl.data, pl.byte_count);
        tagtinker_free_image_payload(&pl);
        free(px);
    } else if(!strcmp(cmd, "nfc")) {
        static MfUltralightData mfu;
        size_t n = unhex(argv[2], (uint8_t*)mfu.page, sizeof(mfu.page));
        mfu.pages_read = (uint16_t)(n / 4);
        char barcode[18];
        TagTinkerNfcResult r = tagtinker_nfc_classify(&mfu, barcode);
        printf("result=%d barcode=%s\n", (int)r, barcode);
    } else if(!strcmp(cmd, "host") && argc >= 4) {
        printf("host=%d\n", tagtinker_nfc_url_host_is(argv[2], argv[3]));
    } else {
        return 2;
    }
    return 0;
}
