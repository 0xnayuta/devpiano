#pragma once

#define FP_LANG_AUTO       -1
#define FP_LANG_ENGLISH     0
#define FP_LANG_SCHINESE    1
#define FP_LANG_COUNT       2

// language string def
#define STR_ENGLISH(id, str)  id,
#define STR_SCHINESE(id, str) 
enum StringIDs {
  FP_IDS_EMPTY,
#include "language_strdef.h"
  FP_IDS_COUNT,
};
#undef STR_ENGLISH
#undef STR_SCHINESE

// initialize languages
void lang_init();

// select current language
void lang_set_current(int languageid);

// get current language
int lang_get_current();

// get language code: auto / en / zh-CN
const char* lang_get_code(int languageid);

// parse language code, returns FP_LANG_AUTO for unknown/auto
int lang_from_code(const char* code);

// load str
const char* lang_load_string(uint uid);

// load wide str
const wchar_t* lang_load_string_w(uint uid);

// load filter string for file dialogs (handles embedded nulls)
const wchar_t* lang_load_filter_w(uint uid);

// load legacy Win32 string resource array (legacy-only compatibility path)
// Prefer JSON/i18n APIs for all new code.
const char** lang_load_string_array(uint uid);

// format lang str
int lang_format_string(char *buff, size_t size, uint strid, ...);

// format wide lang str
int lang_format_string_w(wchar_t *buff, size_t size, uint strid, ...);

// open text
int lang_text_open(uint textid);

// readline
int lang_text_readline(char *buff, size_t size);

// close text
void lang_text_close();

// set last error
void lang_set_last_error(const char *format, ...);

// set last error
void lang_set_last_error(uint id, ...);

// get last error
const char * lang_get_last_error();

// get last error wide string
const wchar_t * lang_get_last_error_w();

// localize dialog
void lang_localize_dialog(HWND hwnd);