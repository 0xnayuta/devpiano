#include "pch.h"
#include <shellapi.h>

#include "keyboard.h"
#include "display.h"
#include "config.h"
#include "gui.h"
#include "song.h"
#include "export_mp4.h"
#include "language.h"
#include "update.h"
#include "fp_log.h"

static int g_startup_forced_lang = FP_LANG_AUTO;

static bool narrow_from_wide_ascii(const wchar_t* input, char* output, size_t output_size) {
  if (output == NULL || output_size == 0)
    return false;

  output[0] = 0;
  if (input == NULL)
    return false;

  return 0 != WideCharToMultiByte(CP_UTF8, 0, input, -1, output, static_cast<int>(output_size), NULL, NULL);
}

static bool try_parse_forced_language_code(const wchar_t* code_w, int* out_lang) {
  if (out_lang == NULL || code_w == NULL || code_w[0] == 0)
    return false;

  char code[32] = {0};
  if (!narrow_from_wide_ascii(code_w, code, sizeof(code)))
    return false;

  const int lang = lang_from_code(code);
  if (lang == FP_LANG_AUTO)
    return false;

  *out_lang = lang;
  return true;
}

static bool try_get_forced_language_from_env(int* out_lang) {
  wchar_t value[32] = L"";
  DWORD length = GetEnvironmentVariableW(L"FREEPIANO_LANG", value, ARRAYSIZE(value));
  if (length == 0 || length >= ARRAYSIZE(value))
    return false;

  if (try_parse_forced_language_code(value, out_lang)) {
    fp_log_info(L"Forced language from environment: FREEPIANO_LANG=%ls -> %S", value, lang_get_code(*out_lang));
    return true;
  }

  fp_log_warn(L"Ignoring invalid FREEPIANO_LANG value: %ls", value);
  return false;
}

static bool try_get_forced_language_from_command_line(int* out_lang) {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv == NULL)
    return false;

  bool found = false;
  for (int i = 1; i < argc; ++i) {
    const wchar_t* arg = argv[i];
    if (arg == NULL)
      continue;

    const wchar_t prefix[] = L"--lang=";
    if (_wcsnicmp(arg, prefix, ARRAYSIZE(prefix) - 1) == 0) {
      const wchar_t* value = arg + ARRAYSIZE(prefix) - 1;
      if (try_parse_forced_language_code(value, out_lang)) {
        fp_log_info(L"Forced language from command line: %ls -> %S", arg, lang_get_code(*out_lang));
        found = true;
      }
      else {
        fp_log_warn(L"Ignoring invalid --lang value: %ls", arg);
      }
      break;
    }

    if (_wcsicmp(arg, L"--lang") == 0 && i + 1 < argc) {
      const wchar_t* value = argv[i + 1];
      if (try_parse_forced_language_code(value, out_lang)) {
        fp_log_info(L"Forced language from command line: --lang %ls -> %S", value, lang_get_code(*out_lang));
        found = true;
      }
      else {
        fp_log_warn(L"Ignoring invalid --lang value: %ls", value ? value : L"<null>");
      }
      break;
    }
  }

  LocalFree(argv);
  return found;
}

static void apply_startup_language_overrides() {
  int forced_lang = FP_LANG_AUTO;

  if (try_get_forced_language_from_env(&forced_lang)) {
    g_startup_forced_lang = forced_lang;
  }

  if (try_get_forced_language_from_command_line(&forced_lang)) {
    g_startup_forced_lang = forced_lang;
  }

  if (g_startup_forced_lang != FP_LANG_AUTO) {
    fp_log_info(L"Startup language override result: effective override=%S", lang_get_code(g_startup_forced_lang));
  }
  else {
    fp_log_info(L"Startup language override result: none, config/system selection remains active");
  }
}

#ifdef _DEBUG
int main()
#else
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
#endif
{
  //SetThreadUILanguage(LANG_ENGLISH);
  fp_log_info(L"FreePiano startup begin");

  // initialize com
  if (FAILED(CoInitialize(NULL))) {
    fp_log_error(L"COM initialization failed");
    return 1;
  }

  fp_log_info(L"COM initialized");

  // config init
  if (config_init()) {
    fp_log_error(L"Config initialization failed");
    MessageBoxW(NULL, lang_get_last_error_w(), APP_NAME_W, MB_OK);
    return 1;
  }

  // preload persisted ui language before gui initialization
  config_preload_ui_language("freepiano.cfg");
  apply_startup_language_overrides();

  // init language
  lang_init();
  if (g_startup_forced_lang != FP_LANG_AUTO) {
    lang_set_current(g_startup_forced_lang);
    fp_log_info(L"Applied startup language override before GUI init: %S", lang_get_code(g_startup_forced_lang));
  }

  // init gui
  if (gui_init()) {
    fp_log_error(L"GUI initialization failed");
    MessageBoxW(NULL, lang_get_last_error_w(), APP_NAME_W, MB_OK);
    return 1;
  }
  fp_log_info(L"GUI initialized");

  // init display
  if (display_init(gui_get_window())) {
    fp_log_error(L"Display initialization failed");
    MessageBoxW(NULL, lang_get_last_error_w(), APP_NAME_W, MB_OK);
    return 1;
  }
  fp_log_info(L"Display initialized");

  // intialize keyboard
  if (keyboard_init()) {
    fp_log_error(L"Keyboard initialization failed");
    MessageBoxW(NULL, lang_get_last_error_w(), APP_NAME_W, MB_OK);
    return 1;
  }
  fp_log_info(L"Keyboard initialized");

  // show gui
  gui_show();
  fp_log_info(L"Main window shown");

  // load default config
  config_load("freepiano.cfg");

  // check for update
#ifndef _DEBUG
  update_check_async();
#endif

  // open lyt
  //song_open_lyt("test3.lyt");
  //song_open("D:\\src\\freepiano\\trunk\\data\\song\\kiss the rain.fpm");
  //export_mp4("test.mp4");

  MSG msg;
  while (GetMessage(&msg, NULL, NULL, NULL))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  fp_log_info(L"Message loop exited");
  config_save("freepiano.cfg");

  // shutdown keyboard
  keyboard_shutdown();

  // shutdown config
  config_shutdown();

  // shutdown display
  display_shutdown();

  // shutdown COM
  CoUninitialize();

  fp_log_info(L"FreePiano shutdown complete");
  return 0;
}
