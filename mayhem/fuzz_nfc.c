/*
 * Fuzzes the NFC tag decoder: a raw Mifare Ultralight / NTAG page dump goes
 * through the TLV walker, the NDEF URI parser, the ESL id decoder and the
 * host matcher exactly as the app's NFC scan scene feeds them.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include "nfc/tagtinker_nfc.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    /* The poller reports whole 4-byte pages, 4..MF_ULTRALIGHT_MAX_PAGE_NUM of them. */
    if(size < 4 || size > sizeof(((MfUltralightData*)0)->page)) return -1;

    MfUltralightData mfu;
    memset(&mfu, 0, sizeof(mfu));
    memcpy(mfu.page, data, size);
    mfu.pages_read = (uint16_t)(size / 4);
    mfu.pages_total = MF_ULTRALIGHT_MAX_PAGE_NUM;

    char barcode[18];
    (void)tagtinker_nfc_classify(&mfu, barcode);
    (void)tagtinker_nfc_decode_barcode(&mfu, barcode);

    char url[TAGTINKER_NFC_URL_LEN];
    if(tagtinker_nfc_extract_url(&mfu, url, sizeof(url))) {
        (void)tagtinker_nfc_url_host_is(url, "nfc.imagotag.com");
        (void)tagtinker_nfc_decode_url(url, barcode);
    }
    return 0;
}
