/*
 * Type-only stand-in for the Flipper Zero firmware headers that tagtinker_app.h pulls in.
 * It lets the unmodified upstream protocol/ and nfc/ sources compile on a desktop.
 * No firmware behaviour is faked: every function in those sources runs as shipped.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef COUNT_OF
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef struct Gui Gui;
typedef struct ViewDispatcher ViewDispatcher;
typedef struct View View;
typedef struct SceneManager SceneManager;
typedef struct Submenu Submenu;
typedef struct TextInput TextInput;
typedef struct VariableItemList VariableItemList;
typedef struct Popup Popup;
typedef struct Widget Widget;
typedef struct TextBox TextBox;
typedef struct DialogsApp DialogsApp;
typedef struct NotificationApp NotificationApp;
typedef struct File File;
typedef struct Bt Bt;
typedef struct FuriHalBleProfileBase FuriHalBleProfileBase;
typedef struct FuriThread FuriThread;
typedef struct Nfc Nfc;

typedef int BtStatus;

typedef struct {
    uint32_t type;
    uint32_t event;
} SceneManagerEvent;

/* Layout subset of the firmware's MfUltralightData used by nfc/tagtinker_nfc.c. */
#define MF_ULTRALIGHT_MAX_PAGE_NUM 231

typedef struct {
    uint8_t data[4];
} MfUltralightPage;

typedef struct {
    MfUltralightPage page[MF_ULTRALIGHT_MAX_PAGE_NUM];
    uint16_t pages_read;
    uint16_t pages_total;
} MfUltralightData;
