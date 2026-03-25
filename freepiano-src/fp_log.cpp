#include "pch.h"
#include "fp_log.h"

#include <stdarg.h>

static thread_lock_t g_fp_log_lock;
static bool g_fp_log_file_initialized = false;
static wchar_t g_fp_log_path[MAX_PATH] = L"";

static void fp_log_init_file_path() {
  if (g_fp_log_file_initialized)
    return;

  wchar_t module_path[MAX_PATH] = L"";
  GetModuleFileNameW(NULL, module_path, ARRAYSIZE(module_path));

  wchar_t* slash = wcsrchr(module_path, L'\\');
  if (slash)
    *slash = 0;

  wchar_t logs_dir[MAX_PATH] = L"";
  swprintf_s(logs_dir, L"%s\\logs", module_path);
  CreateDirectoryW(logs_dir, NULL);
  swprintf_s(g_fp_log_path, L"%s\\freepiano.log", logs_dir);

  g_fp_log_file_initialized = true;
}

static void fp_log_write_line(const wchar_t* level, const wchar_t* message) {
  thread_lock lock(g_fp_log_lock);

  fp_log_init_file_path();

  SYSTEMTIME st;
  GetLocalTime(&st);

  wchar_t line[4096];
  swprintf_s(
    line,
    L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] %s\r\n",
    st.wYear,
    st.wMonth,
    st.wDay,
    st.wHour,
    st.wMinute,
    st.wSecond,
    st.wMilliseconds,
    level ? level : L"INFO",
    message ? message : L"");

  OutputDebugStringW(line);

  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  if (console != NULL && console != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(console, &mode)) {
      DWORD written = 0;
      WriteConsoleW(console, line, static_cast<DWORD>(wcslen(line)), &written, NULL);
    }
  }

  if (g_fp_log_path[0]) {
    FILE* fp = nullptr;
    _wfopen_s(&fp, g_fp_log_path, L"ab");
    if (fp) {
      const int utf8_size = WideCharToMultiByte(CP_UTF8, 0, line, -1, NULL, 0, NULL, NULL);
      if (utf8_size > 1) {
        char* utf8 = new char[utf8_size];
        WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, utf8_size, NULL, NULL);
        fwrite(utf8, 1, utf8_size - 1, fp);
        delete[] utf8;
      }
      fclose(fp);
    }
  }
}

static void fp_log_v(const wchar_t* level, const wchar_t* fmt, va_list args) {
  wchar_t message[3072];
  _vsnwprintf_s(message, ARRAYSIZE(message), _TRUNCATE, fmt ? fmt : L"", args);
  fp_log_write_line(level, message);
}

void fp_log_info(const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fp_log_v(L"INFO", fmt, args);
  va_end(args);
}

void fp_log_warn(const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fp_log_v(L"WARN", fmt, args);
  va_end(args);
}

void fp_log_error(const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fp_log_v(L"ERROR", fmt, args);
  va_end(args);
}
