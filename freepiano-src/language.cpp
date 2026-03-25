#include "pch.h"
#include "language.h"
#include "config.h"
#include "json_loader.h"
#include "fp_log.h"

#include <vector>
#include <map>
#include <set>
#include <string>

static int lang_current;
static const wchar_t* string_names_w[FP_IDS_COUNT];
static const wchar_t* string_debug_keys_w[FP_IDS_COUNT];
static const wchar_t* string_debug_fallback_w[FP_IDS_COUNT];
static const wchar_t* string_table_w[FP_LANG_COUNT][FP_IDS_COUNT];
static std::vector<wchar_t*> localized_wstrings;

// JSON string tables (when USE_JSON_TRANSLATIONS is enabled)
static std::map<int, JsonLoader::StringMap> json_string_tables;
static bool json_loaded = false;
static std::set<std::string> logged_fallback_keys;
static std::set<std::string> logged_missing_keys;
static std::set<std::string> logged_invalid_utf8_keys;

static wchar_t* wide_dup_owned(const wchar_t *text) {
  if (text == NULL)
    return NULL;

  const size_t len = wcslen(text) + 1;
  wchar_t *copy = new wchar_t[len];
  wcscpy_s(copy, len, text);
  localized_wstrings.push_back(copy);
  return copy;
}

// Convert UTF-8 to wide string, memory is managed internally
static wchar_t* utf8_to_wide_owned(const char *text) {
  if (text == NULL)
    return NULL;

  int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
  if (wide_length <= 0)
    return NULL;

  wchar_t *wide_text = new wchar_t[wide_length];
  if (0 == MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide_text, wide_length)) {
    delete[] wide_text;
    return NULL;
  }
  localized_wstrings.push_back(wide_text);
  return wide_text;
}

// Convert UTF-8 to wide string with explicit length (for embedded nulls)
static wchar_t* utf8_to_wide_owned_len(const char *text, size_t len) {
  if (text == NULL || len == 0)
    return NULL;

  int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, static_cast<int>(len), NULL, 0);
  if (wide_length <= 0)
    return NULL;

  wchar_t *wide_text = new wchar_t[wide_length + 1];
  if (0 == MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, static_cast<int>(len), wide_text, wide_length)) {
    delete[] wide_text;
    return NULL;
  }
  wide_text[wide_length] = L'\0';  // Add null terminator at the end
  localized_wstrings.push_back(wide_text);
  return wide_text;
}

// Convert UTF-8 filter string with embedded nulls to wide string
static const wchar_t* utf8_filter_to_wide(const char *text) {
  if (text == NULL)
    return L"";

  // Find total length including embedded nulls (double-null terminated)
  int total_len = 0;
  const char *p = text;
  while (*p || *(p + 1)) {
    total_len++;
    p++;
  }
  total_len += 2;

  int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, total_len, NULL, 0);
  if (wide_length <= 0)
    return L"";

  wchar_t *wide_text = new wchar_t[wide_length];
  if (0 == MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, total_len, wide_text, wide_length)) {
    delete[] wide_text;
    return L"";
  }

  localized_wstrings.push_back(wide_text);
  return wide_text;
}

// Get string name from ID
static const char* get_string_name(uint uid) {
  static const struct { uint id; const char* name; } id_to_name[] = {
#define STR_ENGLISH(id, str) { id, #id },
#define STR_SCHINESE(id, str)
#include "language_strdef.h"
#undef STR_ENGLISH
#undef STR_SCHINESE
  };

  for (const auto& entry : id_to_name) {
    if (entry.id == uid) {
      return entry.name;
    }
  }
  return "";
}

static std::string make_logged_key(int lang_id, uint uid) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%s:%u", lang_get_code(lang_id), uid);
  return std::string(buffer);
}

static void log_fallback_once(int from_lang, uint uid) {
  const std::string key = make_logged_key(from_lang, uid);
  if (logged_fallback_keys.insert(key).second) {
    fp_log_warn(L"i18n fallback: key=%S current=%S fallback=en", get_string_name(uid), lang_get_code(from_lang));
  }
}

static void log_missing_once(int lang_id, uint uid) {
  const std::string key = make_logged_key(lang_id, uid);
  if (logged_missing_keys.insert(key).second) {
    fp_log_error(L"i18n missing key: key=%S current=%S, using hardcoded fallback", get_string_name(uid), lang_get_code(lang_id));
  }
}

static void log_invalid_utf8_once(int lang_id, uint uid) {
  const std::string key = make_logged_key(lang_id, uid);
  if (logged_invalid_utf8_keys.insert(key).second) {
    fp_log_error(L"i18n invalid UTF-8: key=%S current=%S, using fallback", get_string_name(uid), lang_get_code(lang_id));
  }
}

static int lang_normalize(int languageid) {
  if (languageid >= 0 && languageid < FP_LANG_COUNT)
    return languageid;
  return FP_LANG_ENGLISH;
}

static const wchar_t* lang_get_debug_key_string(uint uid) {
#if defined(I18N_DEBUG_SHOW_KEY)
  if (uid > 0 && uid < FP_IDS_COUNT && string_debug_keys_w[uid] != NULL)
    return string_debug_keys_w[uid];
#endif
  return L"";
}

static const wchar_t* lang_get_debug_fallback_string(uint uid) {
#if defined(I18N_DEBUG_SHOW_FALLBACK)
  if (uid > 0 && uid < FP_IDS_COUNT && string_debug_fallback_w[uid] != NULL)
    return string_debug_fallback_w[uid];
#endif
  return L"";
}

static void validate_json_string_table(int lang_id, const JsonLoader::StringMap& strings) {
  for (auto it = strings.begin(); it != strings.end(); ++it) {
    if (it->first.empty()) {
      fp_log_warn(L"Locale %S contains empty key", lang_get_code(lang_id));
    }
    if (it->second.empty()) {
      fp_log_warn(L"Locale %S contains empty value for key=%S", lang_get_code(lang_id), it->first.c_str());
    }
  }
}

// Load JSON language file
static bool load_json_language(int lang_id) {
  const char* code = lang_get_code(lang_id);
  if (code == nullptr || strcmp(code, "auto") == 0) {
    return false;
  }

  // Try multiple paths
  const char* paths[] = {
    "locales/%s.json",
    "res/locales/%s.json",
    "../locales/%s.json",
    "../res/locales/%s.json",
  };

  for (const char* path_template : paths) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), path_template, code);

    JsonLoader::StringMap strings;
    if (JsonLoader::parse_file(path, strings)) {
      validate_json_string_table(lang_id, strings);
      json_string_tables[lang_id] = strings;
      fp_log_info(L"Loaded JSON locale: lang=%S path=%S strings=%u", code, path, static_cast<unsigned int>(strings.size()));
      return true;
    }
  }

  fp_log_warn(L"Failed to load JSON locale for lang=%S, will use hardcoded fallback", code);
  return false;
}

void lang_init() {
  // Initialize string names (needed for dialog localization)
  for (int id = 0; id < FP_IDS_COUNT; id++) {
    string_names_w[id] = L"";
    string_debug_keys_w[id] = L"";
    string_debug_fallback_w[id] = L"";
    for (int lang = 0; lang < FP_LANG_COUNT; lang++) {
      string_table_w[lang][id] = L"";
    }
  }

  // Initialize string names from macro definitions
#define STR_ENGLISH(id, str) \
  string_names_w[id] = utf8_to_wide_owned(#id); \
  { \
    wchar_t debug_key[128]; \
    wchar_t debug_fallback[160]; \
    swprintf_s(debug_key, ARRAYSIZE(debug_key), L"[%hs]", #id); \
    swprintf_s(debug_fallback, ARRAYSIZE(debug_fallback), L"[%hs]!", #id); \
    string_debug_keys_w[id] = wide_dup_owned(debug_key); \
    string_debug_fallback_w[id] = wide_dup_owned(debug_fallback); \
  }
#define STR_SCHINESE(id, str)
#include "language_strdef.h"
#undef STR_ENGLISH
#undef STR_SCHINESE

#if USE_JSON_TRANSLATIONS
  // Load JSON language files
  json_loaded = true;
  bool en_loaded = load_json_language(FP_LANG_ENGLISH);
  bool zh_loaded = load_json_language(FP_LANG_SCHINESE);
  (void)zh_loaded;

  if (!en_loaded) {
    fp_log_warn(L"English JSON locale not available, JSON translations disabled for this run");
    json_loaded = false;
  }
#endif

  // Always load hardcoded strings as fallback
#define STR_ENGLISH(id, str) string_table_w[FP_LANG_ENGLISH][id] = utf8_to_wide_owned(str);
#define STR_SCHINESE(id, str) string_table_w[FP_LANG_SCHINESE][id] = utf8_to_wide_owned(str);
#include "language_strdef.h"
#undef STR_ENGLISH
#undef STR_SCHINESE

  // Auto choose language by default
  lang_set_current(FP_LANG_AUTO);
#if defined(I18N_DEBUG_SHOW_KEY)
  fp_log_info(L"I18N_DEBUG_SHOW_KEY is enabled");
#endif
#if defined(I18N_DEBUG_SHOW_FALLBACK)
  fp_log_info(L"I18N_DEBUG_SHOW_FALLBACK is enabled");
#endif
  fp_log_info(L"Language subsystem initialized: current=%S json=%ls", lang_get_code(lang_current), json_loaded ? L"enabled" : L"fallback-only");
}

// Get default language from system or config
int lang_get_default() {
  int configured_language = config_get_ui_language();
  if (configured_language != FP_LANG_AUTO)
    return configured_language;

  LANGID id = GetUserDefaultUILanguage();
  switch (PRIMARYLANGID(id)) {
  case LANG_ENGLISH:
    return FP_LANG_ENGLISH;

  case LANG_CHINESE:
    return FP_LANG_SCHINESE;
  }
  return FP_LANG_ENGLISH;
}

// Select current language
void lang_set_current(int languageid) {
  int requested = languageid;
  if (languageid > FP_LANG_AUTO && languageid < FP_LANG_COUNT) {
    lang_current = languageid;
  }
  else {
    lang_current = lang_get_default();
  }

  fp_log_info(L"Language selected: requested=%S effective=%S", lang_get_code(requested), lang_get_code(lang_current));
}

// Get current language
int lang_get_current() {
  return lang_current;
}

const char* lang_get_code(int languageid) {
  switch (languageid) {
  case FP_LANG_ENGLISH:
    return "en";
  case FP_LANG_SCHINESE:
    return "zh-CN";
  default:
    return "auto";
  }
}

int lang_from_code(const char* code) {
  if (code == NULL || code[0] == 0 || _stricmp(code, "auto") == 0)
    return FP_LANG_AUTO;

  if (_stricmp(code, "en") == 0 || _stricmp(code, "en-US") == 0)
    return FP_LANG_ENGLISH;

  if (_stricmp(code, "zh-CN") == 0 || _stricmp(code, "zh") == 0 || _stricmp(code, "zh-Hans") == 0)
    return FP_LANG_SCHINESE;

  return FP_LANG_AUTO;
}

static const wchar_t* lang_get_hardcoded_string(int languageid, uint uid) {
  const int normalized = lang_normalize(languageid);
  if (uid < 1 || uid >= FP_IDS_COUNT)
    return L"";

  const wchar_t* text = string_table_w[normalized][uid];
  if (text != NULL && text[0] != 0)
    return text;

#if defined(I18N_DEBUG_SHOW_KEY)
  return lang_get_debug_key_string(uid);
#else
  return L"";
#endif
}


// Load string from JSON or fallback to hardcoded
const wchar_t* lang_load_string_w(uint uid) {
  if (uid < 1 || uid >= FP_IDS_COUNT)
    return L"";

  const int current_lang = lang_normalize(lang_current);

#if USE_JSON_TRANSLATIONS
  if (json_loaded) {
    const char* name = get_string_name(uid);
    if (name == NULL || name[0] == 0) {
      fp_log_error(L"i18n invalid string id: %u", uid);
      return lang_get_hardcoded_string(current_lang, uid);
    }

    auto it = json_string_tables.find(current_lang);
    if (it != json_string_tables.end()) {
      auto str_it = it->second.find(name);
      if (str_it != it->second.end()) {
        if (str_it->second.empty()) {
          fp_log_warn(L"i18n empty value: key=%S current=%S, using fallback", name, lang_get_code(current_lang));
        }
        else {
          const wchar_t* cached = utf8_to_wide_owned(str_it->second.c_str());
          if (cached)
            return cached;
          log_invalid_utf8_once(current_lang, uid);
        }
      }
    }

    if (current_lang != FP_LANG_ENGLISH) {
      auto en_it = json_string_tables.find(FP_LANG_ENGLISH);
      if (en_it != json_string_tables.end()) {
        auto str_it = en_it->second.find(name);
        if (str_it != en_it->second.end() && !str_it->second.empty()) {
          log_fallback_once(current_lang, uid);
#if defined(I18N_DEBUG_SHOW_FALLBACK)
          return lang_get_debug_fallback_string(uid);
#else
          const wchar_t* cached = utf8_to_wide_owned(str_it->second.c_str());
          if (cached)
            return cached;
          log_invalid_utf8_once(FP_LANG_ENGLISH, uid);
#endif
        }
      }
    }

    log_missing_once(current_lang, uid);
#if defined(I18N_DEBUG_SHOW_FALLBACK)
    return lang_get_debug_fallback_string(uid);
#else
    return lang_get_hardcoded_string(current_lang, uid);
#endif
  }
#endif

  return lang_get_hardcoded_string(current_lang, uid);
}

// Load string as narrow (converts from wide)
const char* lang_load_string(uint uid) {
  static char buffer[4096];
  const wchar_t* wstr = lang_load_string_w(uid);
  if (wstr == NULL)
    wstr = L"";

  if (0 == WideCharToMultiByte(CP_ACP, 0, wstr, -1, buffer, sizeof(buffer), NULL, NULL)) {
    buffer[0] = 0;
    fp_log_warn(L"WideCharToMultiByte failed for string id=%u", uid);
  }
  return buffer;
}

// Format wide string
int lang_format_string_w(wchar_t *buff, size_t size, uint strid, ...) {
  if (buff == NULL || size == 0)
    return -1;

  const wchar_t *format = lang_load_string_w(strid);
  if (format == NULL)
    format = L"";

  va_list args;
  va_start(args, strid);
  int result = _vsnwprintf(buff, size, format, args);
  va_end(args);
  buff[size - 1] = 0;
  return result;
}

// Format narrow string
int lang_format_string(char *buff, size_t size, uint strid, ...) {
  if (buff == NULL || size == 0)
    return -1;

  const char *format = lang_load_string(strid);
  if (format == NULL)
    format = "";

  va_list args;
  va_start(args, strid);
  int result = _vsnprintf(buff, size, format, args);
  va_end(args);
  buff[size - 1] = 0;
  return result;
}

// Load filter string for file dialogs
const wchar_t* lang_load_filter_w(uint uid) {
  const int current_lang = lang_normalize(lang_current);

#if USE_JSON_TRANSLATIONS
  if (json_loaded) {
    const char* name = get_string_name(uid);
    if (name == NULL || name[0] == 0) {
      fp_log_error(L"i18n invalid filter id: %u", uid);
      return L"";
    }

    auto it = json_string_tables.find(current_lang);
    if (it != json_string_tables.end()) {
      auto str_it = it->second.find(name);
      if (str_it != it->second.end()) {
        const std::string& str = str_it->second;
        if (!str.empty()) {
          const wchar_t* wide = utf8_to_wide_owned_len(str.data(), str.size());
          if (wide)
            return wide;
          log_invalid_utf8_once(current_lang, uid);
        }
      }
    }

    if (current_lang != FP_LANG_ENGLISH) {
      auto en_it = json_string_tables.find(FP_LANG_ENGLISH);
      if (en_it != json_string_tables.end()) {
        auto str_it = en_it->second.find(name);
        if (str_it != en_it->second.end() && !str_it->second.empty()) {
          log_fallback_once(current_lang, uid);
          const std::string& str = str_it->second;
          const wchar_t* wide = utf8_to_wide_owned_len(str.data(), str.size());
          if (wide)
            return wide;
          log_invalid_utf8_once(FP_LANG_ENGLISH, uid);
        }
      }
    }

    log_missing_once(current_lang, uid);
  }
#endif

  // Fallback to hardcoded filter strings
  if (uid < FP_IDS_COUNT) {
    // Find the filter string by ID
    static bool init = false;
    static const char* filter_map[FP_IDS_COUNT];

    if (!init) {
      memset(filter_map, 0, sizeof(filter_map));
#define STR_ENGLISH(id, str) if (strstr(#id, "FILTER") || strstr(#id, "OPEN_FILTER") || strstr(#id, "SAVE_FILTER")) filter_map[id] = str;
#define STR_SCHINESE(id, str)
#include "language_strdef.h"
#undef STR_ENGLISH
#undef STR_SCHINESE
      init = true;
    }

    if (filter_map[uid]) {
      const wchar_t* wide = utf8_filter_to_wide(filter_map[uid]);
      return wide ? wide : L"";
    }
  }

  return L"";
}

// Load string array (legacy)
// Legacy-only compatibility path for old Win32 string resources.
// New code should use JSON-backed language APIs instead.
const char* * lang_load_string_array(uint uid) {
  static bool logged_legacy_use = false;
  static char temp[1024];
  static char *arr[256];
  const uint temp_size = sizeof(temp) / sizeof(temp[0]);

  if (!logged_legacy_use) {
    fp_log_warn(L"lang_load_string_array() is using legacy Win32 string resources (LoadStringA)");
    logged_legacy_use = true;
  }

  uint len = LoadStringA(GetModuleHandle(NULL), uid, temp, temp_size - 1);
  temp[len < temp_size ? len : 0] = 0;

  arr[0] = temp;
  uint arrlen = 1;

  for (uint i = 0; i < len; i++) {
    if (temp[i] == ',') {
      temp[i] = '\0';
      if (arrlen < 256) {
        arr[arrlen] = temp + i + 1;
        arrlen++;
      }
    }
  }
  arr[arrlen] = 0;
  return (const char * *)arr;
}

// Text resource handling
static struct lang_text_t {
  char *data;
  char *end;
  HGLOBAL hrc;
} lang_text = { NULL, NULL, NULL };

static WORD system_language(int lang_id) {
  switch (lang_id) {
  case FP_LANG_ENGLISH:  return MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL);
  case FP_LANG_SCHINESE: return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  }
  return MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
}

int lang_text_open(uint textid) {
  lang_text_close();

  HINSTANCE module = GetModuleHandle(NULL);
  HRSRC hrsrc = FindResourceExA(module, "TEXT", MAKEINTRESOURCEA(textid), system_language(lang_current));

  if (hrsrc == NULL)
    hrsrc = FindResourceExA(module, "TEXT", MAKEINTRESOURCEA(textid), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL));

  if (hrsrc) {
    HGLOBAL hrc = LoadResource(0, hrsrc);
    if (hrc) {
      lang_text.hrc = LoadResource(0, hrsrc);
      lang_text.data = (char *)LockResource(hrc);
      lang_text.end = lang_text.data + SizeofResource(NULL, hrsrc);
      return lang_text.data - lang_text.end;
    }
  }

  return 0;
}

int lang_text_readline(char *line, size_t size) {
  char *line_end = line;

  while (lang_text.data < lang_text.end) {
    if (*lang_text.data == '\n') {
      if (line_end > line) {
        *line_end = 0;
        lang_text.data++;
        return line_end - line;
      }
    } else {
      if (line_end < line + size - 1)
        *line_end++ = *lang_text.data;
    }
    lang_text.data++;
  }

  if (line_end > line) {
    *line_end = 0;
    return line_end - line;
  }

  return 0;
}

void lang_text_close() {
  if (lang_text.hrc)
    FreeResource(lang_text.hrc);

  lang_text.data = NULL;
  lang_text.end = NULL;
  lang_text.hrc = NULL;
}

// Error handling
static char error_message[1024] = "Unknown error.";
static wchar_t error_message_w[1024] = L"Unknown error.";

static void sync_last_error_wide_from_narrow() {
  if (0 == MultiByteToWideChar(CP_ACP, 0, error_message, -1, error_message_w, ARRAYSIZE(error_message_w))) {
    wcscpy_s(error_message_w, L"Unknown error.");
  }
}

void lang_set_last_error(const char *format, ...) {
  if (format == NULL)
    format = "Unknown error.";

  va_list args;
  va_start(args, format);
  vsnprintf_s(error_message, sizeof(error_message), _TRUNCATE, format, args);
  va_end(args);
  sync_last_error_wide_from_narrow();
  fp_log_error(L"last_error: %S", error_message);
}

void lang_set_last_error(uint id, ...) {
  const char *format = lang_load_string(id);
  if (format == NULL || format[0] == 0)
    format = "Unknown error.";

  va_list args;
  va_start(args, id);
  vsnprintf_s(error_message, sizeof(error_message), _TRUNCATE, format, args);
  va_end(args);
  sync_last_error_wide_from_narrow();
  fp_log_error(L"last_error: %S", error_message);
}

const char * lang_get_last_error() {
  return error_message;
}

const wchar_t * lang_get_last_error_w() {
  return error_message_w;
}

// Dialog localization
static void set_window_text_localized(HWND hwnd, const wchar_t *text) {
  SetWindowTextW(hwnd, text);
}

static BOOL CALLBACK localize_hwnd(HWND hwnd, LPARAM lParam) {
  wchar_t className[256];
  wchar_t text[64];
  GetClassNameW(hwnd, className, ARRAYSIZE(className));
  GetWindowTextW(hwnd, text, ARRAYSIZE(text));

  for (int i = 1; i < FP_IDS_COUNT; i++) {
    if (wcscmp(text, string_names_w[i]) == 0) {
      set_window_text_localized(hwnd, lang_load_string_w(i));
      return TRUE;
    }
  }

  return TRUE;
}

void lang_localize_dialog(HWND hwnd) {
  localize_hwnd(hwnd, NULL);
  EnumChildWindows(hwnd, &localize_hwnd, NULL);
}
