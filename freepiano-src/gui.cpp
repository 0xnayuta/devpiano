#include "pch.h"
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <windowsx.h>

#include "gui.h"
#include "config.h"
#include "midi.h"
#include "output_dsound.h"
#include "output_wasapi.h"
#include "output_asio.h"
#include "synthesizer_vst.h"
#include "display.h"
#include "keyboard.h"
#include "song.h"
#include "language.h"
#include "export_mp4.h"
#include "export_wav.h"
#include "fp_log.h"
#include "../res/resource.h"

#pragma comment(lib, "Shlwapi.lib")

// enable vistual style.
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")



static void try_open_song(int err) {
  if (err == 0) {
    gui_show_song_info();
    song_start_playback();
    return;
  }

  switch (err) {
   case -1: MessageBoxW(gui_get_window(), lang_load_string_w(IDS_ERR_OPEN_SONG), APP_NAME_W, MB_OK); break;
   case -2: MessageBoxW(gui_get_window(), lang_load_string_w(IDS_ERR_OPEN_SONG_VERSION), APP_NAME_W, MB_OK); break;
   default: MessageBoxW(gui_get_window(), lang_load_string_w(IDS_ERR_OPEN_SONG_FORMAT), APP_NAME_W, MB_OK); break;
  }
}

static bool narrow_to_wide_local_path(const char* text, wchar_t* wide_text, size_t wide_count) {
  if (wide_text == NULL || wide_count == 0)
    return false;

  wide_text[0] = 0;
  if (text == NULL || text[0] == 0)
    return false;

  const int result = MultiByteToWideChar(CP_ACP, 0, text, -1, wide_text, static_cast<int>(wide_count));
  if (result <= 0) {
    fp_log_warn(L"ACP->Wide path conversion failed: %S", text);
    return false;
  }

  return true;
}

static bool wide_to_narrow_local_path(const wchar_t* wide_text, char* text, size_t text_size) {
  if (text == NULL || text_size == 0)
    return false;

  text[0] = 0;
  if (wide_text == NULL || wide_text[0] == 0)
    return false;

  const int result = WideCharToMultiByte(CP_ACP, 0, wide_text, -1, text, static_cast<int>(text_size), NULL, NULL);
  if (result <= 0) {
    fp_log_warn(L"Wide->ACP path conversion failed: %ls", wide_text);
    return false;
  }

  return true;
}

static bool decode_text_codepage(UINT codepage, const char *text, wchar_t* wide_text, size_t wide_count, const wchar_t* context) {
  if (wide_text == NULL || wide_count == 0)
    return false;

  wide_text[0] = 0;
  if (text == NULL)
    return false;

  const int length = MultiByteToWideChar(codepage, 0, text, -1, wide_text, static_cast<int>(wide_count));
  if (length > 0)
    return true;

  fp_log_warn(L"%ls conversion failed: codepage=%u text=%S", context ? context : L"Text", codepage, text);
  return false;
}

static bool decode_menu_text(UINT codepage, const char *text, wchar_t* wide_text, size_t wide_count) {
  return decode_text_codepage(codepage, text, wide_text, wide_count, L"Menu text");
}

static void set_window_text_local_codepage(HWND hwnd, const char* text, UINT codepage) {
  if (hwnd == NULL)
    return;

  wchar_t wide_text[4096];
  if (decode_text_codepage(codepage, text ? text : "", wide_text, ARRAYSIZE(wide_text), L"Window text")) {
    SetWindowTextW(hwnd, wide_text);
  }
  else {
    SetWindowTextW(hwnd, L"");
  }
}

static void set_window_text_local(HWND hwnd, const char* text) {
  set_window_text_local_codepage(hwnd, text, CP_ACP);
}

static void set_dlg_item_text_local(HWND hwnd, int itemid, const char* text) {
  set_window_text_local(GetDlgItem(hwnd, itemid), text);
}

static int get_window_text_local(HWND hwnd, char* text, size_t text_size) {
  if (text == NULL || text_size == 0)
    return 0;

  text[0] = 0;
  if (hwnd == NULL)
    return 0;

  wchar_t wide_text[4096] = {0};
  const int wide_len = GetWindowTextW(hwnd, wide_text, ARRAYSIZE(wide_text));
  if (wide_len <= 0)
    return 0;

  if (!wide_to_narrow_local_path(wide_text, text, text_size)) {
    fp_log_warn(L"Window text cannot be represented in ACP");
    text[0] = 0;
    return 0;
  }

  return static_cast<int>(strlen(text));
}

static int get_dlg_item_text_local(HWND hwnd, int itemid, char* text, size_t text_size) {
  return get_window_text_local(GetDlgItem(hwnd, itemid), text, text_size);
}

static bool open_dialog(char *buff, size_t size, const wchar_t* filters, const char* init_dir = NULL) {
  wchar_t wbuff[260] = {0};
  wchar_t wdir[260] = {0};

  OPENFILENAMEW ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = gui_get_window();
  ofn.lpstrFile = wbuff;
  ofn.lpstrFile[0] = 0;
  ofn.nMaxFile = ARRAYSIZE(wbuff);
  ofn.lpstrFilter = filters;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  ofn.nFileExtension = 0;

  if (init_dir) {
    char dir[260] = {0};
    config_get_media_path(dir, sizeof(dir), init_dir);
    PathRemoveFileSpecA(dir);
    if (narrow_to_wide_local_path(dir, wdir, ARRAYSIZE(wdir))) {
      ofn.lpstrInitialDir = wdir;
    }
  }

  bool result = GetOpenFileNameW(&ofn) != 0;
  if (result && wbuff[0]) {
    if (!wide_to_narrow_local_path(wbuff, buff, size)) {
      fp_log_error(L"Open dialog returned a path that cannot be represented in ACP");
      return false;
    }
  }
  return result;
}

static bool save_dialog(char *buff, size_t size, const wchar_t *filters, const char* init_dir = NULL) {
  wchar_t wbuff[260] = {0};
  wchar_t wdir[260] = {0};

  OPENFILENAMEW ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = gui_get_window();
  ofn.lpstrFile = wbuff;
  ofn.lpstrFile[0] = 0;
  ofn.nMaxFile = ARRAYSIZE(wbuff);
  ofn.lpstrFilter = filters;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST;
  ofn.nFileExtension = 0;

  if (init_dir) {
    char dir[260] = {0};
    config_get_media_path(dir, sizeof(dir), init_dir);
    PathRemoveFileSpecA(dir);
    if (narrow_to_wide_local_path(dir, wdir, ARRAYSIZE(wdir))) {
      ofn.lpstrInitialDir = wdir;
    }
  }

  bool result = GetSaveFileNameW(&ofn) != 0;
  if (result && wbuff[0]) {
    if (!wide_to_narrow_local_path(wbuff, buff, size)) {
      fp_log_error(L"Save dialog returned a path that cannot be represented in ACP");
      return false;
    }
  }
  return result;
}

static BOOL append_menu_text_codepage(HMENU menu, UINT flags, UINT_PTR item_id, const char *text, UINT codepage) {
  if (text == NULL)
    return AppendMenuW(menu, flags, item_id, NULL);

  wchar_t wide_text[4096];
  if (decode_menu_text(codepage, text, wide_text, ARRAY_COUNT(wide_text)))
    return AppendMenuW(menu, flags, item_id, wide_text);

  return AppendMenuW(menu, flags, item_id, L"");
}

static BOOL append_menu_text(HMENU menu, UINT flags, UINT_PTR item_id, const char *text) {
  return append_menu_text_codepage(menu, flags, item_id, text, CP_ACP);
}

static BOOL append_menu_text_w(HMENU menu, UINT flags, UINT_PTR item_id, const wchar_t *text) {
  return AppendMenuW(menu, flags, item_id, text);
}

static BOOL append_controller_menu_text(HMENU menu, UINT flags, UINT_PTR item_id, const char *text) {
  return append_menu_text_codepage(menu, flags, item_id, text, CP_UTF8);
}

static BOOL listview_set_item_text_a(HWND list, int item, int subitem, LPSTR text) {
  LVITEMA lv = {};
  lv.iSubItem = subitem;
  lv.pszText = text ? text : const_cast<LPSTR>("");
  return static_cast<BOOL>(SendMessageA(list, LVM_SETITEMTEXTA, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&lv)));
}

static BOOL listview_set_item_text_w(HWND list, int item, int subitem, LPWSTR text) {
  LVITEMW lv = {};
  lv.iSubItem = subitem;
  lv.pszText = text ? text : const_cast<LPWSTR>(L"");
  return static_cast<BOOL>(SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&lv)));
}

static int listview_insert_item_a(HWND list, LVITEMA *item) {
  return static_cast<int>(SendMessageA(list, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(item)));
}

static int listview_insert_column_a(HWND list, int column, LVCOLUMNA *column_info) {
  return static_cast<int>(SendMessageA(list, LVM_INSERTCOLUMNA, static_cast<WPARAM>(column), reinterpret_cast<LPARAM>(column_info)));
}

static int listview_insert_column_w(HWND list, int column, LVCOLUMNW *column_info) {
  return static_cast<int>(SendMessageW(list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(column), reinterpret_cast<LPARAM>(column_info)));
}

static BOOL listview_get_item_text_a(HWND list, int item, int subitem, LPSTR text, int cchTextMax) {
  LVITEMA lv = {};
  lv.iSubItem = subitem;
  lv.cchTextMax = cchTextMax;
  lv.pszText = text;
  return static_cast<BOOL>(SendMessageA(list, LVM_GETITEMTEXTA, static_cast<WPARAM>(item), reinterpret_cast<LPARAM>(&lv)));
}

static HTREEITEM treeview_insert_item_a(HWND tree, TVINSERTSTRUCTA *item) {
  return reinterpret_cast<HTREEITEM>(SendMessageA(tree, TVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(item)));
}

static HTREEITEM treeview_insert_item_w(HWND tree, TVINSERTSTRUCTW *item) {
  return reinterpret_cast<HTREEITEM>(SendMessageW(tree, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(item)));
}

static int combobox_add_string_a(HWND combo, const char *text) {
  if (text == NULL)
    text = "";

  wchar_t wide_text[4096];
  if (decode_text_codepage(CP_ACP, text, wide_text, ARRAYSIZE(wide_text), L"ComboBox text"))
    return static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide_text)));

  return static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"")));
}

static int combobox_add_string_w(HWND combo, const wchar_t *text) {
  if (text == NULL)
    text = L"";

  return static_cast<int>(SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text)));
}

// Numeric edit control
#define EN_VALUE_VALID      0xFE01
#define EN_VALUE_INVALID    0xFE02

static int lparam_to_int(LPARAM value) {
  return static_cast<int>(value);
}

static LRESULT CALLBACK NumericEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
  switch (uMsg) {
  case WM_CHAR:
    if (wParam == VK_RETURN) {
      SetFocus(NULL);
      SetFocus(hWnd);
      return 0;
    }
    break;

  case WM_SETFOCUS:
    PostMessage(hWnd, EM_SETSEL, 0, static_cast<LPARAM>(-1));
    break;

  case WM_KILLFOCUS:
    {
      HWND parent = GetParent(hWnd);
      WORD id = GetDlgCtrlID(hWnd);

      if (parent && id) {
        if (dwRefData) {
          int value = 0;
          char temp[256];
          get_window_text_local(hWnd, temp, sizeof(temp));

          if (sscanf(temp, (const char*)dwRefData, &value) == 1)
            PostMessage(parent, WM_COMMAND, MAKEWPARAM(id, EN_VALUE_VALID), static_cast<LPARAM>(value));
          else
            PostMessage(parent, WM_COMMAND, MAKEWPARAM(id, EN_VALUE_INVALID), 0);
        }
        else {
          PostMessage(parent, WM_COMMAND, MAKEWPARAM(id, EN_VALUE_INVALID), 0);
        }
      }
    }
    break;
  }

  return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

static void update_numeric_edit(HWND hwnd, int itemid, const char *value, const char* format) {
  HWND edit = GetDlgItem(hwnd, itemid);
  set_window_text_local(edit, value);
  SetWindowSubclass(edit, NumericEditProc, 0, reinterpret_cast<DWORD_PTR>(format));
}

static void update_numeric_edit(HWND hwnd, int itemid, int value) {
  char buff[32];
  _snprintf(buff, sizeof(buff), "%d", value);
  buff[sizeof(buff) - 1] = 0;
  update_numeric_edit(hwnd, itemid, buff, "%d");
}

static void update_numeric_edit(HWND hwnd, int itemid, double value) {
  char buff[32];
  _snprintf(buff, sizeof(buff), "%g", value);
  buff[sizeof(buff) - 1] = 0;
  update_numeric_edit(hwnd, itemid, buff, "%g");
}

static void update_slider(HWND hwnd, int itemid, int value, short value_min, short value_max) {
  HWND slider = GetDlgItem(hwnd, itemid);
  SendMessage(slider, TBM_SETRANGE, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(MAKELONG(value_min, value_max)));
  SendMessage(slider, TBM_SETPOS, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(value));
}

// -----------------------------------------------------------------------------------------
// config window
// -----------------------------------------------------------------------------------------
static HWND setting_hwnd = NULL;
static HWND setting_tab = NULL;
static HWND setting_page = NULL;

static INT_PTR CALLBACK settings_midi_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static HMENU menu_midi_input_channel_popup = NULL;
  static HMENU menu_midi_input_enable_popup = NULL;

  struct helpers {
    static void update_midi_input(HWND list, int id, const char *device) {
      wchar_t wbuff[256];
      midi_input_config_t config;
      config_get_midi_input_config(device, &config);

      listview_set_item_text_w(list, id, 1, const_cast<LPWSTR>(lang_load_string_w(config.enable ? IDS_MIDI_INPUT_LIST_ENABLED : IDS_MIDI_INPUT_LIST_DISABLED)));

      if (config.remap == 0) {
        wcscpy_s(wbuff, lang_load_string_w(IDS_MIDI_INPUT_LIST_REMAP_OUTPUT));
      }
      else {
        lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_MIDI_INPUT_LIST_REMAP_INPUT, config.remap - 1);
      }
      listview_set_item_text_w(list, id, 2, wbuff);
    }

    static void refresh_midi_inputs(HWND hWnd) {
      HWND input_list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);

      struct enum_callback : midi_enum_callback {
        void operator () (const char *value) {
          LVITEMA lvI = {0};

          lvI.pszText   = (char*)value;
          lvI.mask      = LVIF_TEXT | LVIF_STATE;
          lvI.stateMask = 0;
          lvI.iSubItem  = 0;
          lvI.state     = 0;
          lvI.iItem     = index;
          listview_insert_item_a(list, &lvI);

          update_midi_input(list, index, value);
          index ++;
        }

        HWND list;
        int index;
      };

      // clear content
      ListView_DeleteAllItems(input_list);

      // build input list
      enum_callback callback;
      callback.list = input_list;
      callback.index = 0;
      midi_enum_input(callback);

      bool keep_selected = false;
      if (keep_selected) {
        int selected = ListView_GetNextItem(input_list, -1, LVNI_FOCUSED);
        ListView_SetItemState(input_list, -1, 0, LVIS_SELECTED); // deselect all items
        ListView_EnsureVisible(input_list, selected, FALSE);
        ListView_SetItemState(input_list, selected, LVIS_SELECTED ,LVIS_SELECTED); // select item
        ListView_SetItemState(input_list, selected, LVIS_FOCUSED ,LVIS_FOCUSED); // optional
      }
    }

    static HMENU create_popup_menu() {
      HMENU menu = CreatePopupMenu();

      MENUINFO menuinfo;
      menuinfo.cbSize = sizeof(MENUINFO);
      menuinfo.fMask = MIM_STYLE;
      menuinfo.dwStyle = MNS_NOTIFYBYPOS;
      SetMenuInfo(menu, &menuinfo);

      return menu;
    }

    static void show_popup_menu(HMENU menu, HWND hWnd) {
      POINT pos;
      GetCursorPos(&pos);
      TrackPopupMenuEx(menu, TPM_LEFTALIGN, pos.x, pos.y, hWnd, NULL);
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     LVCOLUMNW lvc = {0};
     HWND input_list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);

     lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
     lvc.fmt = LVCFMT_LEFT;

     // Device
     lvc.pszText = const_cast<LPWSTR>(lang_load_string_w(IDS_MIDI_INPUT_LIST_DEVICE));
     lvc.cx = 300;
     listview_insert_column_w(input_list, 0, &lvc);

     // Enable.
     lvc.pszText = const_cast<LPWSTR>(lang_load_string_w(IDS_MIDI_INPUT_LIST_ENABLE));
     lvc.cx = 60;
     lvc.fmt = LVCFMT_CENTER;
     listview_insert_column_w(input_list, 2, &lvc);

     // Channel.
     lvc.pszText = const_cast<LPWSTR>(lang_load_string_w(IDS_MIDI_INPUT_LIST_REMAP));
     lvc.cx = 60;
     listview_insert_column_w(input_list, 2, &lvc);

     // Set extended style.
     ListView_SetExtendedListViewStyle(input_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

     helpers::refresh_midi_inputs(hWnd);
     break;
   }

   case WM_DEVICECHANGE: {
     helpers::refresh_midi_inputs(hWnd);
     break;
   }

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
     case IDC_MIDI_INPUT_LIST:
       break;
     }
     break;

   case WM_MENUCOMMAND: {
     DWORD pos = wParam;
     HMENU menu = (HMENU)lParam;

     if (menu == menu_midi_input_channel_popup || menu == menu_midi_input_enable_popup) {
       HWND list = GetDlgItem(hWnd, IDC_MIDI_INPUT_LIST);
       int row = ListView_GetNextItem(list, -1, LVNI_FOCUSED);

       char device_name[256] = "";
       listview_get_item_text_a(list, row, 0, device_name, sizeof(device_name));
       midi_input_config_t config;
       config_get_midi_input_config(device_name, &config);

       if (menu == menu_midi_input_channel_popup) {
         config.remap = pos;
       }
       else if (menu == menu_midi_input_enable_popup) {
         config.enable = pos == 0;
       }

       config_set_midi_input_config(device_name, config);
       midi_open_inputs();

       helpers::update_midi_input(list, row, device_name);
     }

     DestroyMenu(menu_midi_input_channel_popup);
     menu_midi_input_channel_popup = NULL;

     DestroyMenu(menu_midi_input_enable_popup);
     menu_midi_input_enable_popup = NULL;
   }
   break;

   case WM_NOTIFY:
     if (LOWORD(wParam) == IDC_MIDI_INPUT_LIST) {
       switch (((LPNMHDR)lParam)->code) {
         case NM_CLICK:
         case NM_DBLCLK: {
           LPNMITEMACTIVATE item = (LPNMITEMACTIVATE)lParam;
           int row = item->iItem;
           int column = item->iSubItem;

           if (column == 1) {
             menu_midi_input_enable_popup = helpers::create_popup_menu();

             append_menu_text_w(menu_midi_input_enable_popup,  MF_STRING, static_cast<UINT_PTR>(0), lang_load_string_w(IDS_MIDI_INPUT_LIST_ENABLED));
             append_menu_text_w(menu_midi_input_enable_popup,  MF_STRING, static_cast<UINT_PTR>(0), lang_load_string_w(IDS_MIDI_INPUT_LIST_DISABLED));

             helpers::show_popup_menu(menu_midi_input_enable_popup, hWnd);
           }

           else if (column == 2) {
             menu_midi_input_channel_popup = helpers::create_popup_menu();

             append_menu_text_w(menu_midi_input_channel_popup,  MF_STRING, static_cast<UINT_PTR>(0), lang_load_string_w(IDS_MIDI_INPUT_LIST_REMAP_OUTPUT));
             for (int i = 0; i < 16; i++) {
               wchar_t wbuff[256];
               lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_MIDI_INPUT_LIST_REMAP_INPUT, i);
               append_menu_text_w(menu_midi_input_channel_popup,  MF_STRING, static_cast<UINT_PTR>(0), wbuff);
             }

             helpers::show_popup_menu(menu_midi_input_channel_popup, hWnd);
           }
           break;
         }
       }
     };
     break;
  }

  return 0;
}

static INT_PTR CALLBACK settings_audio_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static char output_delay_text_format[256];
  static char output_volume_text_format[256];

  static const char *output_types[] = {
    "NONE",
    "DirectSound",
    "WASAPI",
    "ASIO",
  };

  struct helpers {
    static void refresh_play_speed(HWND hWnd) {
      update_numeric_edit(hWnd, IDC_PLAYBACK_SPEED, song_get_play_speed());
      update_slider(hWnd, IDC_PLAYBACK_SPEED_SLIDER, song_get_play_speed() * 10, 0, 20);
    }

    static void refresh_output_delay(HWND hWnd) {
      update_numeric_edit(hWnd, IDC_OUTPUT_DELAY, config_get_output_delay());
      update_slider(hWnd, IDC_OUTPUT_DELAY_SLIDER, config_get_output_delay(), 0, 80);
    }

    static void refresh_output_volume(HWND hWnd) {
      update_numeric_edit(hWnd, IDC_OUTPUT_VOLUME, config_get_output_volume());
      update_slider(hWnd, IDC_OUTPUT_VOLUME_SLIDER, config_get_output_volume(), 0, 200);
    }

    static void refresh_output_devices(HWND hWnd) {
      HWND output_list = GetDlgItem(hWnd, IDC_OUTPUT_LIST);

      // output device list
      struct callback : dsound_enum_callback, wasapi_enum_callback, asio_enum_callback {
        void operator () (const char *value) {
          char buff[256];

          _snprintf(buff, sizeof(buff), "%s: %s", output_types[type], value);
          buff[sizeof(buff) - 1] = 0;
          combobox_add_string_a(list, buff);
          if (config_get_output_type() == type) {
            if (_stricmp(value, config_get_output_device()) == 0) {
              ComboBox_SetCurSel(list, ComboBox_GetCount(list) - 1);
              selected = true;
            }
          }

          count++;
        }

        void operator () (const char *value, void *device) {
          operator () (value);
        }

        callback(int type, HWND list)
          : type(type)
          , list(list)
          , selected(false)
          , count(0) {
        }

        ~callback() {
          if (config_get_output_type() == type && !selected) {
            if (count)
              ComboBox_SetCurSel(list, ComboBox_GetCount(list) - count);
          }
        }

        HWND list;
        int type;
        bool selected;
        int count;
      };

      // dsound output
      ComboBox_ResetContent(output_list);
      ComboBox_SetItemHeight(output_list, 0, 16);
      if (config_get_output_type() == OUTPUT_TYPE_AUTO) {
        wchar_t wbuff[256];
        wchar_t type_w[64];
        if (!decode_text_codepage(CP_ACP, output_types[config_get_current_output_type()], type_w, ARRAYSIZE(type_w), L"Audio output type")) {
          wcscpy_s(type_w, L"?");
        }
        swprintf_s(wbuff, L"%s: %s", lang_load_string_w(IDS_SETTING_AUDIO_AUTO), type_w);
        combobox_add_string_w(output_list, wbuff);
      }
      else {
        combobox_add_string_w(output_list, lang_load_string_w(IDS_SETTING_AUDIO_AUTO));
      }
      ComboBox_SetCurSel(output_list, 0);
      dsound_enum_device(callback(OUTPUT_TYPE_DSOUND, output_list));
      wasapi_enum_device(callback(OUTPUT_TYPE_WASAPI, output_list));
      asio_enum_device(callback(OUTPUT_TYPE_ASIO, output_list));
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     helpers::refresh_output_devices(hWnd);
     helpers::refresh_output_delay(hWnd);
     helpers::refresh_output_volume(hWnd);
     helpers::refresh_play_speed(hWnd);
   }
   break;

   case WM_COMMAND:
     if (LOWORD(wParam) == IDC_OUTPUT_LIST) {
       if (HIWORD(wParam) == CBN_SELCHANGE) {
         int result = 1;
         char temp[256];
         get_dlg_item_text_local(hWnd, IDC_OUTPUT_LIST, temp, sizeof(temp));

         for (int i = 0; i < ARRAY_COUNT(output_types); i++) {
           int len = static_cast<int>(strlen(output_types[i]));
           if (_strnicmp(output_types[i], temp, len) == 0) {
             result = config_select_output(i, temp + len + 2);
             break;
           }
         }

         if (result) {
           config_select_output(OUTPUT_TYPE_AUTO, "");
         }

         helpers::refresh_output_devices(hWnd);
       }
     }
     else if (LOWORD(wParam) == IDC_OUTPUT_DELAY) {
       if (HIWORD(wParam) == EN_VALUE_VALID) {
         config_set_output_delay(lparam_to_int(lParam));
         helpers::refresh_output_delay(hWnd);
       }
       else if (HIWORD(wParam) == EN_VALUE_INVALID) {
         helpers::refresh_output_delay(hWnd);
       }
     }
     else if (LOWORD(wParam) == IDC_OUTPUT_VOLUME) {
       if (HIWORD(wParam) == EN_VALUE_VALID) {
         config_set_output_volume(lparam_to_int(lParam));
         helpers::refresh_output_volume(hWnd);
       }
       else if (HIWORD(wParam) == EN_VALUE_INVALID) {
         helpers::refresh_output_volume(hWnd);
       }
     }
     else if (LOWORD(wParam) == IDC_PLAYBACK_SPEED) {
       if (HIWORD(wParam) == EN_VALUE_VALID) {
         char temp[64];
         get_dlg_item_text_local(hWnd, IDC_PLAYBACK_SPEED, temp, sizeof(temp));
         song_set_play_speed(atof(temp));
         helpers::refresh_play_speed(hWnd);
       }
       else if (HIWORD(wParam) == EN_VALUE_INVALID) {
         helpers::refresh_play_speed(hWnd);
       }
     }
     break;

   case WM_HSCROLL: {
     HWND output_delay = GetDlgItem(hWnd, IDC_OUTPUT_DELAY_SLIDER);
     HWND output_volume = GetDlgItem(hWnd, IDC_OUTPUT_VOLUME_SLIDER);
     HWND playback_speed = GetDlgItem(hWnd, IDC_PLAYBACK_SPEED_SLIDER);

     if (output_delay == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(output_delay, TBM_GETPOS, 0, 0));
       config_set_output_delay(pos);
       helpers::refresh_output_delay(hWnd);
     }
     else if (output_volume == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(output_volume, TBM_GETPOS, 0, 0));
       config_set_output_volume(pos);
       helpers::refresh_output_volume(hWnd);
     }
     else if (playback_speed == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(playback_speed, TBM_GETPOS, 0, 0));
       song_set_play_speed((double)pos / 10);
       helpers::refresh_play_speed(hWnd);
     }
   }
  }

  return 0;
}

static INT_PTR CALLBACK settings_play_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  static const char * keyboard_shifts[] =
  {
    "0", "+1", "-1",
  };

  static int current_page = 0;

  struct helpers
  {
    static void make_value(char *buff, size_t size, int value, int range_min = 0, int range_max = 127) {
      if (value >= range_min && value <= range_max) {
        _snprintf(buff, size, "%d", value);
      }
      else {
        _snprintf(buff, size, "-");
      }
      if (size > 0)
        buff[size - 1] = 0;
    }

    static void refresh(HWND hWnd)
    {
      for (int i = 0; i < 4; i++) {
        HWND button = GetDlgItem(hWnd, IDC_PLAY_PAGE1 + i);
        Button_SetState(button, i == current_page);
      }

      for (int id = 0; id < 4; id ++)
      {
        wchar_t wbuff[256];
        char buff[256];
        int channel = current_page * 4 + id;

        // input channel label
        lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_SETTING_PLAY_INPUT, channel);
        SetDlgItemTextW(hWnd, IDC_PLAY_IN_1 + id, wbuff);

        // output channel label
        lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_SETTING_PLAY_OUTPUT, config_get_output_channel(channel));
        SetDlgItemTextW(hWnd, IDC_PLAY_OUT_1 + id, wbuff);

        // velocity
        update_numeric_edit(hWnd, IDC_PLAY_VELOCITY1 + id, config_get_key_velocity(channel));

        // transpose
        make_value(buff, sizeof(buff), config_get_key_transpose(channel), -64, 64);
        update_numeric_edit(hWnd, IDC_PLAY_TRANSPOSE1 + id, buff, "%d");

        // follow key
        CheckDlgButton(hWnd, IDC_PLAY_FOLLOW_KEY1 + id, config_get_follow_key(channel) != 0);

        // octave
        HWND octshift = GetDlgItem(hWnd, IDC_PLAY_OCTAVE1 + id);
        ComboBox_ResetContent(octshift);
        for (int i = 0; i < ARRAY_COUNT(keyboard_shifts); i++)
        {
          combobox_add_string_a(octshift, keyboard_shifts[i]);
          if (atoi(keyboard_shifts[i]) == config_get_key_octshift(channel))
            ComboBox_SetCurSel(octshift, i);
        }

        // channel
        HWND keychannel = GetDlgItem(hWnd, IDC_PLAY_CHANNEL1 + id);
        ComboBox_ResetContent(keychannel);
        for (int i = 0; i < 16; i++)
        {
          char buff[256];
          _snprintf(buff, sizeof(buff), "%d", i);
          buff[sizeof(buff) - 1] = 0;
          combobox_add_string_a(keychannel, buff);
          if (i == config_get_output_channel(channel))
            ComboBox_SetCurSel(keychannel, i);
        }

        // program
        make_value(buff, sizeof(buff), config_get_program(channel));
        update_numeric_edit(hWnd, IDC_PLAY_PROGRAM1 + id, buff, "%d");

        // bank msb
        make_value(buff, sizeof(buff), config_get_controller(channel, 0));
        update_numeric_edit(hWnd, IDC_PLAY_BANK1 + id, buff, "%d");

        // sustain
        make_value(buff, sizeof(buff), config_get_controller(channel, 64));
        update_numeric_edit(hWnd, IDC_PLAY_SUSTAIN1 + id, buff, "%d");
      }
    }
  };

  bool refresh = false;

  switch (uMsg)
  {
  case WM_INITDIALOG:
    lang_localize_dialog(hWnd);
    refresh = true;
    break;

  case WM_ACTIVATE:
    if (WA_INACTIVE != LOWORD(wParam))
      refresh = true;
    break;

  case WM_COMMAND:
    for (int page = 0; page < 4; page++) {
      if (LOWORD(wParam) == IDC_PLAY_PAGE1 + page) {
        current_page = page;
        refresh = true;
      }
    }

    for (int id = 0; id < 4; id ++)
    {
      int value;
      int channel = current_page * 4 + id;

      if (LOWORD(wParam) == IDC_PLAY_VELOCITY1 + id)
      {
        switch (HIWORD(wParam)) {
        case EN_VALUE_VALID:
          value = lparam_to_int(lParam);
          if (value < 0) value = 0;
          if (value > 127) value = 127;
          song_send_event(SM_VELOCITY, channel, SM_VALUE_SET, value, true);
          refresh = true;
          break;

        case EN_VALUE_INVALID:
          refresh = true;
          break;
        }
      }

      else if (LOWORD(wParam) == IDC_PLAY_TRANSPOSE1 + id)
      {
        switch (HIWORD(wParam)) {
        case EN_VALUE_VALID:
          value = lparam_to_int(lParam);
          if (value < -48) value = -48;
          if (value > 48) value = 48;
          song_send_event(SM_TRANSPOSE, channel, SM_VALUE_SET, (char)value, true);
          refresh = true;
          break;

        case EN_VALUE_INVALID:
          refresh = true;
          break;
        }
      }

      else if (LOWORD(wParam) == IDC_PLAY_FOLLOW_KEY1 + id)
      {
        byte value = IsDlgButtonChecked(hWnd, IDC_PLAY_FOLLOW_KEY1 + id);
        song_send_event(SM_FOLLOW_KEY, channel, SM_VALUE_SET, value, true);
      } 

      else if (LOWORD(wParam) == IDC_PLAY_OCTAVE1 + id)
      {
        if (HIWORD(wParam) == CBN_SELCHANGE) {
          char temp[256];
          get_dlg_item_text_local(hWnd, IDC_PLAY_OCTAVE1 + id, temp, sizeof(temp));
          int value = atoi(temp);
          song_send_event(SM_OCTAVE, channel, SM_VALUE_SET, (char)value, true);
          refresh = true;
        }
      }

      else if (LOWORD(wParam) == IDC_PLAY_CHANNEL1 + id)
      {
        if (HIWORD(wParam) == CBN_SELCHANGE) {
          char temp[256];
          get_dlg_item_text_local(hWnd, IDC_PLAY_CHANNEL1 + id, temp, sizeof(temp));
          int value = atoi(temp);
          song_send_event(SM_CHANNEL, channel, SM_VALUE_SET, value, true);
          refresh = true;
        }
        break;
      }

      else if (LOWORD(wParam) == IDC_PLAY_PROGRAM1 + id)
      {
        switch (HIWORD(wParam)) {
        case EN_VALUE_VALID:
          value = lparam_to_int(lParam);
          if (value < 0) value = 0;
          if (value > 127) value = 127;
          song_send_event(SM_PROGRAM, channel, SM_VALUE_SET, value, true);
          refresh = true;
          break;

        case EN_VALUE_INVALID:
          config_set_program(channel, -1);
          refresh = true;
          break;
        }
      }

      else if (LOWORD(wParam) == IDC_PLAY_BANK1 + id)
      {
        switch (HIWORD(wParam)) {
        case EN_VALUE_VALID:
          value = lparam_to_int(lParam);
          if (value < 0) value = 0;
          if (value > 127) value = 127;

          song_send_event(SM_BANK_MSB, channel, SM_VALUE_SET, value, true);

          if (config_get_program(channel) < 128)
            song_send_event(SM_PROGRAM, channel, SM_VALUE_INC, 0, true);

          refresh = true;
          break;

        case EN_VALUE_INVALID:
          config_set_controller(channel, 0, -1);
          refresh = true;
          break;
        }
      }

      else if (LOWORD(wParam) == IDC_PLAY_SUSTAIN1 + id)
      {
        switch (HIWORD(wParam)) {
        case EN_VALUE_VALID:
          value = lparam_to_int(lParam);
          if (value < 0) value = 0;
          if (value > 127) value = 127;
          song_send_event(SM_SUSTAIN, channel, SM_VALUE_SET, value, true);
          refresh = true;
          break;

        case EN_VALUE_INVALID:
          config_set_controller(channel, 64, -1);
          refresh = true;
          break;
        }
      }
    }
    break;
  }

  if (refresh) {
    helpers::refresh(hWnd);
  }

  return 0;
}

static INT_PTR CALLBACK settings_keymap_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  struct helpers {
    static void update_content(HWND edit) {
      // save keymap config
      char *buff = config_save_keymap();

      LockWindowUpdate(edit);
      int scroll = GetScrollPos(edit, SB_VERT);
      set_window_text_local(edit, buff);
      Edit_Scroll(edit, scroll, 0);
      LockWindowUpdate(NULL);

      free(buff);
    }

    static void apply_content(HWND edit) {
      uint buff_size = Edit_GetTextLength(edit) + 1;
      char *buff = (char*)malloc(buff_size);

      get_window_text_local(edit, buff, buff_size);
      config_parse_keymap(buff);

      free(buff);
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     HWND output_content = GetDlgItem(hWnd, IDC_MAP_CONTENT);
     Edit_LimitText(output_content,-1);
     uint tabstops = 46;
     Edit_SetTabStops(output_content, 1, &tabstops);
     helpers::update_content(output_content);
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDC_MAP_SAVE:
        helpers::update_content(GetDlgItem(hWnd, IDC_MAP_CONTENT));
        break;

      case IDC_MAP_APPLY:
        HWND output_content = GetDlgItem(hWnd, IDC_MAP_CONTENT);

        helpers::apply_content(output_content);
        helpers::update_content(output_content);
        break;
     }
     break;
  }

  return 0;
}

static INT_PTR CALLBACK settings_gui_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  struct combo_option_t {
    int value;
    int name;
  };

  static combo_option_t auto_color_options[] = {
    { AUTO_COLOR_CLASSIC,   IDS_SETTING_GUI_AUTO_COLOR_CLASSIC },
    { AUTO_COLOR_CHANNEL,   IDS_SETTING_GUI_AUTO_COLOR_CHANNEL },
    { AUTO_COLOR_VELOCITY,  IDS_SETTING_GUI_AUTO_COLOR_VELOCITY },
  };

  static combo_option_t note_display_options[] = {
    { NOTE_DISPLAY_DOH,         IDS_SETTING_GUI_NOTE_DISPLAY_DOH },
    { NOTE_DIAPLAY_FIXED_DOH,   IDS_SETTING_GUI_NOTE_DISPLAY_FIXED_DOH },
    { NOTE_DISPLAY_NAME,        IDS_SETTING_GUI_NOTE_DISPLAY_NAME },
  };

  struct helpers {
    static void refresh_combobox(HWND combo, combo_option_t *options, int num_options, int value) {
      for (int i = 0; i < num_options; i++) {
        const wchar_t *name = lang_load_string_w(options[i].name);
        combobox_add_string_w(combo, name);
        if (options[i].value == value) {
          ComboBox_SetCurSel(combo, ComboBox_GetCount(combo) - 1);
        }
      }
    }

    static int combobox_get_value(HWND combo, combo_option_t *options, int num_options) {
      int current = ComboBox_GetCurSel(combo);
      if (current < num_options) {
        return options[current].value;
      }
      return 0;
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     CheckDlgButton(hWnd, IDC_GUI_ENABLE_RESIZE, config_get_enable_resize_window());
     CheckDlgButton(hWnd, IDC_GUI_ENABLE_HOTKEY, !config_get_enable_hotkey());
     CheckDlgButton(hWnd, IDC_GUI_MIDI_TRANSPOSE, config_get_midi_transpose());
     CheckDlgButton(hWnd, IDC_GUI_INSTRUMENT_SHOW_MIDI, config_get_instrument_show_midi());
     CheckDlgButton(hWnd, IDC_GUI_INSTRUMENT_SHOW_VSTI, config_get_instrument_show_vsti());
     CheckDlgButton(hWnd, IDC_GUI_PREVIEW_COLOR, config_get_preview_color());

     // key fade slider
     HWND key_fade = GetDlgItem(hWnd, IDC_GUI_KEY_ANIMATE);
     SendMessage(key_fade, TBM_SETRANGE, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(MAKELONG(0, 100)));
     SendMessage(key_fade, TBM_SETPOS,   static_cast<WPARAM>(TRUE), static_cast<LPARAM>(config_get_key_fade()));

     // gui transparency slider
     HWND key_trans = GetDlgItem(hWnd, IDC_GUI_TRANSPARENCY);
     SendMessage(key_trans, TBM_SETRANGE, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(MAKELONG(0, 255)));
     SendMessage(key_trans, TBM_SETPOS,   static_cast<WPARAM>(TRUE), static_cast<LPARAM>(config_get_gui_transparency()));

     // auto color combo box
     HWND auto_color = GetDlgItem(hWnd, IDC_GUI_AUTO_COLOR);
     helpers::refresh_combobox(auto_color, auto_color_options, ARRAYSIZE(auto_color_options),
       config_get_auto_color());

     // note display
     HWND note_display = GetDlgItem(hWnd, IDC_GUI_NOTE_DISPLAY);
     helpers::refresh_combobox(note_display, note_display_options, ARRAYSIZE(note_display_options),
       config_get_note_display());

     // preview color
     HWND preview_color = GetDlgItem(hWnd, IDC_GUI_PREVIEW_COLOR);
     SendMessage(preview_color, TBM_SETRANGE, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(MAKELONG(0, 100)));
     SendMessage(preview_color, TBM_SETPOS,   static_cast<WPARAM>(TRUE), static_cast<LPARAM>(config_get_preview_color()));
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDC_GUI_ENABLE_RESIZE:
        config_set_enable_resize_window(IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_RESIZE) != 0);
        break;

      case IDC_GUI_ENABLE_HOTKEY:
        config_set_enable_hotkey(!IsDlgButtonChecked(hWnd, IDC_GUI_ENABLE_HOTKEY));
        break;

      case IDC_GUI_MIDI_TRANSPOSE:
        config_set_midi_transpose(IsDlgButtonChecked(hWnd, IDC_GUI_MIDI_TRANSPOSE) != 0);
        break;

      case IDC_GUI_INSTRUMENT_SHOW_MIDI:
        config_set_instrument_show_midi(0 != IsDlgButtonChecked(hWnd, IDC_GUI_INSTRUMENT_SHOW_MIDI));
        break;

      case IDC_GUI_INSTRUMENT_SHOW_VSTI:
        config_set_instrument_show_vsti(0 != IsDlgButtonChecked(hWnd, IDC_GUI_INSTRUMENT_SHOW_VSTI));
        break;

      case IDC_GUI_AUTO_COLOR:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
          config_set_auto_color(helpers::combobox_get_value(reinterpret_cast<HWND>(lParam), auto_color_options, ARRAYSIZE(auto_color_options)));
        }
        break;

      case IDC_GUI_NOTE_DISPLAY:
        if (HIWORD(wParam) == CBN_SELCHANGE) {
          config_set_note_display(helpers::combobox_get_value(reinterpret_cast<HWND>(lParam), note_display_options, ARRAYSIZE(note_display_options)));
        }
        break;
     }
     break;

   case WM_HSCROLL: {
     HWND key_animate = GetDlgItem(hWnd, IDC_GUI_KEY_ANIMATE);
     HWND gui_transparency = GetDlgItem(hWnd, IDC_GUI_TRANSPARENCY);
     HWND gui_preview_color = GetDlgItem(hWnd, IDC_GUI_PREVIEW_COLOR);

     if (key_animate == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(key_animate, TBM_GETPOS, 0, 0));

       // update output delay
       config_set_key_fade(pos);
     }
     else if (gui_transparency == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(gui_transparency, TBM_GETPOS, 0, 0));

       // update output delay
       config_set_gui_transparency(pos);
     }
     else if (gui_preview_color == reinterpret_cast<HWND>(lParam)) {
       int pos = static_cast<int>(SendMessage(gui_preview_color, TBM_GETPOS, 0, 0));

       config_set_preview_color(pos);
     }
   } break;
  }

  return 0;
}

static struct setting_page_t {
  int dialog;
  DLGPROC proc;
  HTREEITEM item;
}
setting_pages[] = {
  { IDD_SETTING_AUDIO,    settings_audio_proc },
  { IDD_SETTING_MIDI,     settings_midi_proc },
  { IDD_SETTING_PLAY,     settings_play_proc },
  { IDD_SETTING_KEYMAP,   settings_keymap_proc },
  { IDD_SETTING_GUI,      settings_gui_proc },
};

// current selected page
static int setting_selected_page = IDD_SETTING_PLAY;

static void add_setting_page(HWND list, const wchar_t *text, int page_id) {
  LPARAM page_param = 0;
  for (int i = 0; i < ARRAY_COUNT(setting_pages); i++) {
    if (setting_pages[i].dialog == page_id) {
      page_param = i;
      break;
    }
  }

  TVINSERTSTRUCTW tvins = {};
  tvins.item.mask = TVIF_TEXT | TVIF_PARAM;
  tvins.item.pszText = const_cast<LPWSTR>(text);
  tvins.item.cchTextMax = 0;
  tvins.item.lParam = page_param;
  tvins.hInsertAfter = NULL;
  tvins.hParent = TVI_ROOT;

  setting_pages[page_param].item = treeview_insert_item_w(list, &tvins);
}

static INT_PTR CALLBACK settings_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     HWND setting_list = GetDlgItem(hWnd, IDC_SETTING_LIST);
     add_setting_page(setting_list, lang_load_string_w(IDS_SETTING_LIST_PLAY), IDD_SETTING_PLAY);
     add_setting_page(setting_list, lang_load_string_w(IDS_SETTING_LIST_AUDIO), IDD_SETTING_AUDIO);
     add_setting_page(setting_list, lang_load_string_w(IDS_SETTING_LIST_MIDI), IDD_SETTING_MIDI);
     add_setting_page(setting_list, lang_load_string_w(IDS_SETTING_LIST_GUI), IDD_SETTING_GUI);
     add_setting_page(setting_list, lang_load_string_w(IDS_SETTING_LIST_KEYMAP), IDD_SETTING_KEYMAP);
   }
   break;

   case WM_ACTIVATE:
     SendMessage(setting_page, WM_ACTIVATE, wParam, lParam);
     break;

   case WM_NOTIFY:
     switch (((LPNMHDR)lParam)->code) {
      case TVN_SELCHANGED: {
        LPNMTREEVIEW pnmtv = (LPNMTREEVIEW) lParam;

        switch (pnmtv->hdr.idFrom) {
         case IDC_SETTING_LIST: {
           if (setting_page)
             DestroyWindow(setting_page);

           HWND setting_list = pnmtv->hdr.hwndFrom;
           int id = pnmtv->itemNew.lParam;

           if (id >= 0 && id < ARRAY_COUNT(setting_pages)) {
             setting_page = CreateDialog(GetModuleHandle(NULL),
                                         MAKEINTRESOURCE(setting_pages[id].dialog), hWnd, setting_pages[id].proc);

             RECT rect;
             GetClientRect(hWnd, &rect);
             rect.left = 102;
             MoveWindow(setting_page, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, FALSE);
             ShowWindow(setting_page, SW_SHOW);

             setting_selected_page = setting_pages[id].dialog;
           }
         }
         break;
        }
      }
      break;
     }
     break;

   case WM_CLOSE:
     DestroyWindow(hWnd);
     break;

   case WM_DESTROY:
     setting_hwnd = NULL;
     setting_page = NULL;
     break;

   case WM_DEVICECHANGE:
     if(setting_page != NULL) {
       SendMessage(setting_page, WM_DEVICECHANGE, wParam, lParam);
     }
     break;
  }

  return 0;
}

static void settings_show() {
  HINSTANCE instance = GetModuleHandle(NULL);
  int selected = setting_selected_page;

  if (setting_hwnd == NULL) {
    setting_hwnd = CreateDialog(instance, MAKEINTRESOURCE(IDD_SETTINGS), NULL, settings_proc);
    ShowWindow(setting_hwnd, SW_SHOW);
  } else {
    SetForegroundWindow(setting_hwnd);
    SetActiveWindow(setting_hwnd);
  }

  for (int i = 0; i < ARRAY_COUNT(setting_pages); i++) {
    if (setting_pages[i].dialog == selected) {
      TreeView_SelectItem(GetDlgItem(setting_hwnd, IDC_SETTING_LIST), setting_pages[i].item);
    }
  }
}


static INT_PTR CALLBACK song_info_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     song_info_t *info = song_get_info();
     set_dlg_item_text_local(hWnd, IDC_SONG_TITLE, info->title);
     set_dlg_item_text_local(hWnd, IDC_SONG_AUTHOR, info->author);

     if (info->compatibility) {
       set_dlg_item_text_local(hWnd, IDC_SONG_COMMENT, info->comment);
     }
     else {
       wchar_t wbuff[1024];
       wchar_t comment_w[512] = {0};
       if (!decode_text_codepage(CP_ACP, info->comment, comment_w, ARRAYSIZE(comment_w), L"Song comment")) {
         wcscpy_s(comment_w, L"");
       }
       swprintf_s(wbuff, L"%s\r\n\r\n%s", lang_load_string_w(IDS_SONG_INFO_COMPATIBITY), comment_w);
       SetDlgItemTextW(hWnd, IDC_SONG_COMMENT, wbuff);
     }
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDOK: {
        song_info_t *info = song_get_info();
        if (song_allow_save()) {
          get_dlg_item_text_local(hWnd, IDC_SONG_TITLE, info->title, sizeof(info->title));
          get_dlg_item_text_local(hWnd, IDC_SONG_AUTHOR, info->author, sizeof(info->author));
          get_dlg_item_text_local(hWnd, IDC_SONG_COMMENT, info->comment, sizeof(info->comment));
        }
        EndDialog(hWnd, 1);
      }
      break;

      case IDCANCEL:
        EndDialog(hWnd, 0);
        break;
     }
     break;

   case WM_CLOSE:
     EndDialog(hWnd, 0);
     break;
  }
  return 0;
}

// show song info
int gui_show_song_info() {
  return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SONG_INFO), gui_get_window(), song_info_proc);
}

// -----------------------------------------------------------------------------------------
// MAIN MENU
// -----------------------------------------------------------------------------------------
static byte selected_key = 0;
static byte preview_key = 0;

static HMENU menu_main = NULL;
static HMENU menu_record = NULL;
static HMENU menu_output = NULL;
static HMENU menu_instrument = NULL;
static HMENU menu_config = NULL;
static HMENU menu_about = NULL;
static HMENU menu_keymap = NULL;
static HMENU menu_play_speed = NULL;
static HMENU menu_setting_group = NULL;
static HMENU menu_export = NULL;
static HMENU menu_setting_group_change = NULL;
static HMENU menu_language = NULL;

// menu id
enum MENU_ID {
  MENU_ID_NONE,
  MENU_ID_VST_PLUGIN,
  MENU_ID_INSTRUMENT_MIDI,
  MENU_ID_INSTRUMENT_VSTI_BROWSE,
  MENU_ID_INSTRUMENT_VSTI_EIDOTR,
  MENU_ID_CONFIG_OPTIONS,
  MENU_ID_KEY_MAP,
  MENU_ID_KEY_MAP_LOAD,
  MENU_ID_KEY_MAP_SAVE,
  MENU_ID_KEY_MAP_SAVEAS,
  MENU_ID_HELP_HOMEPAGE,
  MENU_ID_HELP_ONLINE,
  MENU_ID_HELP_ABOUT,

  MENU_ID_FILE_OPEN,
  MENU_ID_FILE_SAVE,
  MENU_ID_FILE_RECORD,
  MENU_ID_FILE_PLAY,
  MENU_ID_FILE_STOP,
  MENU_ID_FILE_EXPORT_MP4,
  MENU_ID_FILE_EXPORT_WAV,
  MENU_ID_FILE_INFO,

  MENU_ID_PLAY_SPEED,
  MENU_ID_SETTING_GROUP_CHANGE,
  MENU_ID_SETTING_GROUP_ADD,
  MENU_ID_SETTING_GROUP_INSERT,
  MENU_ID_SETTING_GROUP_DEL,
  MENU_ID_SETTING_GROUP_COPY,
  MENU_ID_SETTING_GROUP_PASTE,
  MENU_ID_SETTING_GROUP_DEFAULT,
  MENU_ID_SETTING_GROUP_CLEAR,

  MENU_ID_MIDI_INPUT_CHANNEL,

  MENU_ID_LANGUAGE_ENGLISH,
  MENU_ID_LANGUAGE_CHINESE,
};

// init menu
static int menu_init() {
  // delete sub menus
  while (int count = GetMenuItemCount(menu_main))
        DeleteMenu(menu_main, count - 1, MF_BYPOSITION);

  // create submenus
  menu_output = CreatePopupMenu();
  menu_instrument = CreatePopupMenu();
  menu_record = CreatePopupMenu();
  menu_config = CreatePopupMenu();
  menu_about = CreatePopupMenu();
  menu_keymap = CreatePopupMenu();
  menu_play_speed = CreatePopupMenu();
  menu_setting_group = CreatePopupMenu();
  menu_export = CreatePopupMenu();
  menu_setting_group_change = CreatePopupMenu();
  menu_language = CreatePopupMenu();

  MENUINFO menuinfo;
  menuinfo.cbSize = sizeof(MENUINFO);
  menuinfo.fMask = MIM_STYLE;
  menuinfo.dwStyle = MNS_NOTIFYBYPOS;

  SetMenuInfo(menu_main, &menuinfo);

  // Main menu
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_record, lang_load_string_w(IDS_MENU_FILE));
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_instrument, lang_load_string_w(IDS_MENU_INSTRUMENT));
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_keymap, lang_load_string_w(IDS_MENU_KEYMAP));
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_setting_group, lang_load_string_w(IDS_MENU_KEYGROUP));
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_config, lang_load_string_w(IDS_MENU_CONFIG));
  append_menu_text_w(menu_main, MF_POPUP, (UINT_PTR)menu_about, lang_load_string_w(IDS_MENU_HELP));

  // Record menu
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_OPEN, lang_load_string_w(IDS_MENU_FILE_OPEN));
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_SAVE, lang_load_string_w(IDS_MENU_FILE_SAVE));
  append_menu_text_w(menu_record, MF_POPUP, (UINT_PTR)menu_export, lang_load_string_w(IDS_MENU_FILE_EXPORT));
  append_menu_text_w(menu_record, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_RECORD, lang_load_string_w(IDS_MENU_FILE_RECORD));
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_PLAY, lang_load_string_w(IDS_MENU_FILE_PLAY));
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_STOP, lang_load_string_w(IDS_MENU_FILE_STOP));
  append_menu_text_w(menu_record, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_record, MF_STRING, (UINT_PTR)MENU_ID_FILE_INFO, lang_load_string_w(IDS_MENU_FILE_INFO));

  append_menu_text_w(menu_export, MF_STRING, (UINT_PTR)MENU_ID_FILE_EXPORT_MP4, lang_load_string_w(IDS_MENU_FILE_EXPORT_MP4));
  append_menu_text_w(menu_export, MF_STRING, (UINT_PTR)MENU_ID_FILE_EXPORT_WAV, lang_load_string_w(IDS_MENU_FILE_EXPORT_WAV));

  // Config menu
  append_menu_text_w(menu_config, MF_STRING, (UINT_PTR)MENU_ID_CONFIG_OPTIONS, lang_load_string_w(IDS_MENU_CONFIG_OPTIONS));
  append_menu_text_w(menu_config, MF_POPUP, (UINT_PTR)menu_play_speed, lang_load_string_w(IDS_MENU_CONFIG_PLAYSPEED));

  // About menu
  append_menu_text_w(menu_about, MF_POPUP, (UINT_PTR)menu_language, lang_load_string_w(IDS_MENU_LANGUAGE));
  append_menu_text_w(menu_about, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_HOMEPAGE, lang_load_string_w(IDS_MENU_HELP_HOMEPAGE));
  append_menu_text_w(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_ONLINE, lang_load_string_w(IDS_MENU_HELP_ONLINE));
  append_menu_text_w(menu_about, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_about, MF_STRING, (UINT_PTR)MENU_ID_HELP_ABOUT, lang_load_string_w(IDS_MENU_HELP_ABOUT));

  // language menu
  append_menu_text_w(menu_language, MF_STRING, MENU_ID_LANGUAGE_ENGLISH, lang_load_string_w(IDS_MENU_LANGUAGE_ENGLISH));
  append_menu_text_w(menu_language, MF_STRING, MENU_ID_LANGUAGE_CHINESE, lang_load_string_w(IDS_MENU_LANGUAGE_CHINESE));

  // Setting group menu
  append_menu_text_w(menu_setting_group, MF_POPUP, (UINT_PTR)menu_setting_group_change, lang_load_string_w(IDS_MENU_SETTING_GROUP_CHANGE));
  append_menu_text_w(menu_setting_group, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_ADD, lang_load_string_w(IDS_MENU_SETTING_GROUP_ADD));
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_INSERT, lang_load_string_w(IDS_MENU_SETTING_GROUP_INSERT));
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_DEL, lang_load_string_w(IDS_MENU_SETTING_GROUP_DEL));
  append_menu_text_w(menu_setting_group, MF_SEPARATOR, 0, NULL);
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_COPY, lang_load_string_w(IDS_MENU_SETTING_GROUP_COPY));
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_PASTE, lang_load_string_w(IDS_MENU_SETTING_GROUP_PASTE));
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_CLEAR, lang_load_string_w(IDS_MENU_SETTING_GROUP_CLEAR));
  append_menu_text_w(menu_setting_group, MF_STRING, MENU_ID_SETTING_GROUP_DEFAULT, lang_load_string_w(IDS_MENU_SETTING_GROUP_DEFAULT));

  return 0;
}

// shutdown menu
static void menu_shutdown() {
  DestroyMenu(menu_main);
  DestroyMenu(menu_output);
  DestroyMenu(menu_instrument);
  DestroyMenu(menu_record);
  DestroyMenu(menu_config);
  DestroyMenu(menu_about);
  DestroyMenu(menu_keymap);
  DestroyMenu(menu_play_speed);
  DestroyMenu(menu_setting_group);
  DestroyMenu(menu_export);
}

static INT_PTR CALLBACK about_dialog_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_INITDIALOG:
    lang_localize_dialog(hWnd);
    break;

   case WM_CLOSE:
     EndDialog(hWnd, 0);
     break;
  }

  return 0;
}

// process menu message
int menu_on_command(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  DWORD pos = wParam;
  HMENU menu = (HMENU)lParam;
  uint id = GetMenuItemID(menu, pos);
  char buff[256];

  switch (id) {
   case MENU_ID_INSTRUMENT_VSTI_BROWSE: {
     char temp[260];

     if (open_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_OPEN_FILTER_VST))) {
       if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, temp)) {
         wchar_t wbuff[256];
         lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_ERR_LOAD_VST, err);
         MessageBoxW(gui_get_window(), wbuff, APP_NAME_W, MB_OK);
       }
     }
   }
   break;

   case MENU_ID_INSTRUMENT_VSTI_EIDOTR:
     vsti_show_editor(!vsti_is_show_editor());
     break;

   case MENU_ID_INSTRUMENT_MIDI:
     if (GetMenuStringA(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       char *type_str = strchr(buff, '\t');
       if (type_str) *type_str = '\0';

       if (int err = config_select_instrument(INSTRUMENT_TYPE_MIDI, buff)) {
         wchar_t wbuff[256];
         lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_ERR_LOAD_MIDI, err);
         MessageBoxW(gui_get_window(), wbuff, APP_NAME_W, MB_OK);
       }
     }
     break;

   case MENU_ID_VST_PLUGIN:
     if (GetMenuStringA(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       char *type_str = strchr(buff, '\t');
       if (type_str) *type_str = '\0';

       if (int err = config_select_instrument(INSTRUMENT_TYPE_VSTI, buff)) {
         wchar_t wbuff[256];
         lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_ERR_LOAD_VST, err);
         MessageBoxW(gui_get_window(), wbuff, APP_NAME_W, MB_OK);
       }
     }
     break;

   case MENU_ID_CONFIG_OPTIONS:
     settings_show();
     break;

   case MENU_ID_KEY_MAP:
     if (GetMenuStringA(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       if (int err = config_set_keymap(buff)) {
         wchar_t wbuff[256];
         lang_format_string_w(wbuff, ARRAYSIZE(wbuff), IDS_ERR_LOAD_KEYMAP, err);
         MessageBoxW(gui_get_window(), wbuff, APP_NAME_W, MB_OK);
       }
     }
     break;

   case MENU_ID_HELP_HOMEPAGE:
     ShellExecuteW(NULL, L"open", L"http://freepiano.tiwb.com", NULL, NULL, 0);
     break;

   case MENU_ID_HELP_ONLINE:
     ShellExecuteW(NULL, L"open", L"http://freepiano.tiwb.com/category/help", NULL, NULL, 0);
     break;

   case MENU_ID_HELP_ABOUT:
     DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, about_dialog_proc);
     break;

   case MENU_ID_LANGUAGE_ENGLISH:
     gui_set_language(FP_LANG_ENGLISH);
     break;

   case MENU_ID_LANGUAGE_CHINESE:
     gui_set_language(FP_LANG_SCHINESE);
     break;

   case MENU_ID_FILE_OPEN: {
     char temp[260];
     if (open_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_OPEN_FILTER_SONG), "song\\")) {
       int result = -1;
       const char *extension = PathFindExtensionA(temp);

       song_close();

       if (strcmp(extension, ".lyt") == 0)
         try_open_song(song_open_lyt(temp));

       else if (strcmp(extension, ".fpm") == 0)
         try_open_song(song_open(temp));
     }
   }
   break;

   case MENU_ID_FILE_SAVE: {
     int result = gui_show_song_info();

     if (result) {
       char temp[260];
       if (save_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_SAVE_FILTER_SONG), "song\\")) {
         PathRenameExtensionA(temp, ".fpm");
         int result = song_save(temp);

         if (result != 0) {
           MessageBoxW(gui_get_window(), lang_load_string_w(IDS_ERR_SAVE_SONG), APP_NAME_W, MB_OK);
         }
       }
     }
   }
   break;

   case MENU_ID_FILE_RECORD:
     song_start_record();
     break;

   case MENU_ID_FILE_PLAY:
     song_start_playback();
     break;

   case MENU_ID_FILE_STOP:
     song_stop_playback();
     song_stop_record();
     break;

   case MENU_ID_FILE_INFO:
     gui_show_song_info();
     break;

   case MENU_ID_PLAY_SPEED:
     if (GetMenuStringA(menu, pos, buff, sizeof(buff), MF_BYPOSITION)) {
       double speed = atof(buff);
       if (speed <= 0) speed = 1;
       song_set_play_speed(speed);
     }
     break;

   case MENU_ID_KEY_MAP_LOAD: {
     char temp[260] = {0};
     if (open_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_FILTER_MAP), config_get_keymap())) {
       config_set_keymap(temp);
     }

   }
   break;

   case MENU_ID_KEY_MAP_SAVE:
   case MENU_ID_KEY_MAP_SAVEAS: {
     char temp[260] = {0};

     if (id == MENU_ID_KEY_MAP_SAVEAS) {
       if (save_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_FILTER_MAP), config_get_keymap())) {
         PathRenameExtensionA(temp, ".map");
       }
     }
     else {
       config_get_media_path(temp, sizeof(temp), config_get_keymap());
     }

     if (temp[0]) {
       char *buffer = config_save_keymap(FP_LANG_ENGLISH);

       FILE *fp = fopen(temp, "wb");
       if (fp) {
         fputs(buffer, fp);
         fclose(fp);

         config_set_keymap(temp);
       }

       free(buffer);
     }
   }
   break;

   case MENU_ID_SETTING_GROUP_CHANGE:
     if (GetMenuStringA(menu, pos, buff, sizeof(buff), MF_BYPOSITION))
       song_send_event(SM_SETTING_GROUP, 0, atoi(buff), 0, true);
     break;

   case MENU_ID_SETTING_GROUP_ADD:
     config_set_setting_group_count(config_get_setting_group_count() + 1);
     config_set_setting_group(config_get_setting_group_count() - 1);
     break;

   case MENU_ID_SETTING_GROUP_INSERT:
     config_insert_setting_group(config_get_setting_group());
     break;

   case MENU_ID_SETTING_GROUP_DEL:
     config_delete_setting_group(config_get_setting_group());
     break;

   case MENU_ID_SETTING_GROUP_COPY:
     config_copy_key_setting();
     break;

   case MENU_ID_SETTING_GROUP_PASTE:
     config_paste_key_setting();
     break;

   case MENU_ID_SETTING_GROUP_DEFAULT:
     config_default_key_setting();
     break;

   case MENU_ID_SETTING_GROUP_CLEAR:
     config_clear_key_setting();
     break;

   case MENU_ID_FILE_EXPORT_MP4: {
     song_stop_playback();

     char temp[260] = {0};
     if (save_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_SAVE_FILTER_MP4))) {
       PathRenameExtensionA(temp, ".mp4");
       export_mp4(temp);
     }
   }
   break;

   case MENU_ID_FILE_EXPORT_WAV: {
     song_stop_playback();

     char temp[260] = {0};
     if (save_dialog(temp, sizeof(temp), lang_load_filter_w(IDS_SAVE_FILTER_WAV))) {
       PathRenameExtensionA(temp, ".wav");
       export_wav(temp);
     }
   }
   break;
  }

  return 0;
}

int menu_on_popup(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  HMENU menu = (HMENU)wParam;

  if (uMsg == WM_INITMENUPOPUP) {
    if (menu == menu_instrument) {

      // enum midi devices
      struct enum_midi_callback : midi_enum_callback {
        void operator () (const char *value) {
          bool selected = false;
          char buffer[256];
          strncpy(buffer, value, sizeof(buffer));
          buffer[sizeof(buffer) - 1] = 0;

          if (!found) {
            if (config_get_instrument_type() == INSTRUMENT_TYPE_MIDI &&
              _stricmp(buffer, config_get_instrument_path()) == 0) {
                selected = true;
                found = true;
            }
          }

          strcat_s(buffer, "\tMIDI");
          append_menu_text(menu_instrument, MF_STRING | (selected ? MF_CHECKED : 0), static_cast<UINT_PTR>(MENU_ID_INSTRUMENT_MIDI), buffer);
        }

        enum_midi_callback() {
          found = false;
        }

        bool found;
      };

      // enum vsti plugins.
      struct enum_vsti_callback : vsti_enum_callback {
        void operator () (const char *value) {
          bool selected = false;
          char buffer[256];

          if (only_filename) {
            const char *start = PathFindFileNameA(value);
            if (start) {
              strncpy(buffer, start, sizeof(buffer));
              buffer[sizeof(buffer) - 1] = 0;
              PathRemoveExtensionA(buffer);
            } else {
              strcpy_s(buffer, value);
            }
          } else {
            strcpy_s(buffer, value);
          }

          if (!found) {
            if (config_get_instrument_type() == INSTRUMENT_TYPE_VSTI &&
              _stricmp(buffer, config_get_instrument_path()) == 0) {
                found = true;
                selected = true;
            }
          }

          strcat_s(buffer, "\tVSTi");
          append_menu_text(menu_instrument, MF_STRING | (selected ? MF_CHECKED : 0), static_cast<UINT_PTR>(MENU_ID_VST_PLUGIN), buffer);
        }

        enum_vsti_callback() {
          only_filename = true;
          found = false;
        }

        bool only_filename;
        bool found;
      };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      // Load vsti plugin menu
      append_menu_text_w(menu_instrument, MF_STRING,
        (UINT_PTR)MENU_ID_INSTRUMENT_VSTI_BROWSE, lang_load_string_w(IDS_MENU_INSTRUMENT_BROWSE));

      append_menu_text_w(menu_instrument, MF_STRING | (vsti_is_show_editor() ? MF_CHECKED : 0),
        (UINT_PTR)MENU_ID_INSTRUMENT_VSTI_EIDOTR, lang_load_string_w(IDS_MENU_INSTRUMENT_GUI));

      append_menu_text_w(menu_instrument, MF_SEPARATOR, 0, NULL);

      enum_midi_callback midi_cb;
      if (config_get_instrument_show_midi()) {
        midi_enum_output(midi_cb);
      }

      enum_vsti_callback vsti_cb;
      if (config_get_instrument_show_vsti()) {
        vsti_enum_plugins(vsti_cb);
      }

      if (!vsti_cb.found && config_get_instrument_type() == INSTRUMENT_TYPE_VSTI) {
        vsti_cb.only_filename = false;
        vsti_cb(config_get_instrument_path());
      }
    }
    // keyboard map
    else if (menu == menu_keymap) {
      struct enum_callback : keymap_enum_callback {
        void operator () (const char *value) {
          const char *filename = PathFindFileNameA(value);
          bool checked = _stricmp(filename, config_get_keymap()) == 0;
          append_menu_text(menu, MF_STRING | (checked ? MF_CHECKED : 0), MENU_ID_KEY_MAP, filename);
          found |= checked;
        };

        HMENU menu;
        bool found;
      };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      uint enable_save = config_get_keymap()[0] ? MF_ENABLED : MF_DISABLED;

      // append action menus
      append_menu_text_w(menu, MF_STRING | enable_save, MENU_ID_KEY_MAP_SAVE,   lang_load_string_w(IDS_MENU_KEYMAP_SAVE));
      append_menu_text_w(menu, MF_STRING, MENU_ID_KEY_MAP_LOAD,   lang_load_string_w(IDS_MENU_KEYMAP_LOAD));
      append_menu_text_w(menu, MF_STRING, MENU_ID_KEY_MAP_SAVEAS, lang_load_string_w(IDS_MENU_KEYMAP_SAVEAS));
      append_menu_text(menu, MF_SEPARATOR, 0, NULL);

      // enum key maps.
      enum_callback cb;
      cb.menu = menu;
      cb.found = !*config_get_keymap();
      keyboard_enum_keymap(cb);

      if (!cb.found)
        append_menu_text(menu, MF_STRING | MF_CHECKED, MENU_ID_KEY_MAP, config_get_keymap());
    }
    else if (menu == menu_record) {
      EnableMenuItem(menu, MENU_ID_FILE_SAVE, MF_BYCOMMAND | (song_allow_save() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_PLAY, MF_BYCOMMAND | (!song_is_empty() && !song_is_recording() && !song_is_playing() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_STOP, MF_BYCOMMAND | (song_is_playing() || song_is_recording() ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, MENU_ID_FILE_RECORD, MF_BYCOMMAND | (!song_is_recording() ? MF_ENABLED : MF_DISABLED));

      bool enable_wav_export = (config_get_instrument_type() == INSTRUMENT_TYPE_VSTI) && !song_is_empty();
#if defined(FREEPIANO_NO_X264_EXPORT)
      bool enable_mp4_export = false;
#else
      bool enable_mp4_export = enable_wav_export;
#endif
      EnableMenuItem(menu_export, MENU_ID_FILE_EXPORT_MP4, MF_BYCOMMAND | (enable_mp4_export ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu_export, MENU_ID_FILE_EXPORT_WAV, MF_BYCOMMAND | (enable_wav_export ? MF_ENABLED : MF_DISABLED));
      EnableMenuItem(menu, 2, MF_BYPOSITION | ((enable_mp4_export || enable_wav_export) ? MF_ENABLED : MF_DISABLED));
    }
    else if (menu == menu_play_speed) {
      static const char *speeds[] = { "0.25x", "0.5x", "1.0x", "2.0x" };

      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      for (int i = 0; i < ARRAY_COUNT(speeds); i++)
        append_menu_text(menu, MF_STRING | (song_get_play_speed() == atof(speeds[i]) ? MF_CHECKED : 0), MENU_ID_PLAY_SPEED, speeds[i]);
    }
    else if (menu == menu_setting_group_change) {
      // remove all menu items.
      while (int count = GetMenuItemCount(menu))
        RemoveMenu(menu, count - 1, MF_BYPOSITION);

      uint group_count = config_get_setting_group_count();

      for (uint i = 0; i < group_count; i++) {
        char buff[256];
        _snprintf(buff, sizeof(buff), "%d", i);
        buff[sizeof(buff) - 1] = 0;
        append_menu_text(menu, MF_STRING | (config_get_setting_group() == i ? MF_CHECKED : 0), MENU_ID_SETTING_GROUP_CHANGE, buff);
      }
    }

  } else if (uMsg == WM_UNINITMENUPOPUP) {
  }

  return 0;
}

// key setting window handle
static HWND key_setting_window = NULL;

// key setting proc
static INT_PTR CALLBACK key_setting_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static POINT move_pos;
  static key_bind_t last_bind(SM_NOTE_ON, 0, 60, 127);
  static bool auto_close = true;
  static bool auto_apply = true;
  static bool need_apply = false;

  static const char* note_display_doh[12] =  { "1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7" };
  static const char* note_display_name[12] = { "C", "#C", "D", "#D", "E", "F", "#F", "G", "#G", "A", "#A", "B" };
  static const char* note_octave_doh[12] =   { "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4" };
  static const char* note_octave_name[12] =  { "0", "1", "2", "3", "4", "5", "6", "7", "8" };

  struct helpers {
    static void refresh_controls(HWND hWnd) {
      key_bind_t keydown;
      config_bind_get_keydown(selected_key, &keydown, 1);

      bool is_note = keydown.a == SM_NOTE_ON || keydown.a == SM_NOTE_OFF;

      // remember this bind as last bind
      if (is_note) {
        last_bind = keydown;
      }

      // notes
      for (int note = 0; note < 12; note++) {
        HWND button = GetDlgItem(hWnd, IDC_KEY_SETTING_NOTE_1 + note);
        BOOL value = is_note && (keydown.c % 12 == note);
        Button_SetState(button, value);
      }

      // octave
      for (int octave = 0; octave < 9; octave++) {
        HWND button = GetDlgItem(hWnd, IDC_KEY_SETTING_OCTAVE_0 + octave);
        BOOL value = is_note && (keydown.c / 12 - 1 == octave);
        Button_SetState(button, value);
      }

      // channel
      for (int channel = 0; channel < 8; channel++) {
        HWND button = GetDlgItem(hWnd, IDC_KEY_SETTING_CHANNEL_0 + channel);
        BOOL value = is_note && (keydown.b == channel);
        Button_SetState(button, value);
      }

      // script
      char *buff = config_dump_keybind(selected_key);
      if (buff) {
        HWND edit = GetDlgItem(hWnd, IDC_KEY_SCRIPT);
        uint tabstops = 46;
        Edit_SetTabStops(edit, 1, &tabstops);
        set_window_text_local(edit, buff);
        Edit_SetSel(edit, -2, -1);
        free(buff);
      }

      // caption
      wchar_t temp[256];
      lang_format_string_w(temp, sizeof(temp) / sizeof(temp[0]), IDS_KEYSETTING_CAPTION, config_get_key_name(selected_key));
      SetWindowTextW(hWnd, temp);

      // select script edit control
      SetFocus(GetDlgItem(hWnd, IDC_KEY_SCRIPT));

      // no need to auto apply
      need_apply = false;
    }
    
    static key_bind_t get_keybind() {
      key_bind_t keydown;
      config_bind_get_keydown(selected_key, &keydown, 1);

      bool rebind = false;
      bool is_note = keydown.a == SM_NOTE_ON || keydown.a == SM_NOTE_OFF;

      // make a default note
      if (!is_note) {
        keydown.a = SM_NOTE_ON;
        keydown.b = last_bind.b;
        keydown.c = last_bind.c;
        keydown.d = last_bind.d;
      }

      return keydown;
    }

    static void update_keybind(key_bind_t keydown) {
      config_bind_set_label(selected_key, NULL);
      config_bind_clear_keydown(selected_key);
      config_bind_clear_keyup(selected_key);
      config_bind_add_keydown(selected_key, keydown);
    }
  };

  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     CheckDlgButton(hWnd, IDC_KEY_SETTING_AUTOCLOSE, auto_close);
     CheckDlgButton(hWnd, IDC_KEY_SETTING_AUTOAPPLY, auto_apply);

     // set note names
     switch (config_get_note_display()) {
     case NOTE_DISPLAY_DOH:
       for (int i = 0; i < 12; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_NOTE_1 + i, note_display_doh[i]);
       for (int i = 0; i < 9; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_OCTAVE_0 + i, note_octave_doh[i]);
       break;

     case NOTE_DIAPLAY_FIXED_DOH:
       for (int i = 0; i < 12; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_NOTE_1 + i, note_display_doh[(i + config_get_key_signature()) % 12]);
       for (int i = 0; i < 9; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_OCTAVE_0 + i, note_octave_doh[i]);
       break;

     case NOTE_DISPLAY_NAME:
       for (int i = 0; i < 12; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_NOTE_1 + i, note_display_name[(i + config_get_key_signature()) % 12]);
       for (int i = 0; i < 9; i++)
         set_dlg_item_text_local(hWnd, IDC_KEY_SETTING_OCTAVE_0 + i, note_octave_name[i]);
       break;
     }

     helpers::refresh_controls(hWnd);
   }
   break;

   case WM_ACTIVATE:
     if (WA_INACTIVE == LOWORD(wParam)) {
       preview_key = -1;
       if (IsDlgButtonChecked(hWnd, IDC_KEY_SETTING_AUTOAPPLY)) {
         if (need_apply) {
           PostMessage(hWnd, WM_COMMAND, IDC_KEY_SETTING_BUTTON_APPLY, 0);
         }
       }
       if (IsDlgButtonChecked(hWnd, IDC_KEY_SETTING_AUTOCLOSE)) {
         PostMessage(hWnd, WM_CLOSE, 0, 0);
       }
     } else {
       helpers::refresh_controls(hWnd);
     }
     break;

   case WM_CLOSE:
     EndDialog(hWnd, 0);
     DestroyWindow(hWnd);
     break;

   case WM_DESTROY:
     auto_close = IsDlgButtonChecked(hWnd, IDC_KEY_SETTING_AUTOCLOSE) != FALSE;
     auto_apply = IsDlgButtonChecked(hWnd, IDC_KEY_SETTING_AUTOAPPLY) != FALSE;
     preview_key = -1;
     key_setting_window = NULL;
     break;

   case WM_LBUTTONDOWN:
     {
       RECT rect;
       GetWindowRect(hWnd, &rect);
       ::GetCursorPos(&move_pos);
       move_pos.x -= rect.left;
       move_pos.y -= rect.top;
     }
     ::SetCapture(hWnd);
     break;

   case WM_LBUTTONUP:
     ::ReleaseCapture();
     break;

   case WM_MOUSEMOVE:
     if (::GetCapture() == hWnd) {
       POINT pos;
       ::GetCursorPos(&pos);
       pos.x -= move_pos.x;
       pos.y -= move_pos.y;
       SetWindowPos(hWnd, NULL, pos.x, pos.y, 0, 0, SWP_NOOWNERZORDER | SWP_NOSIZE);
     }
     break;

   case WM_NOTIFY:
     if (wParam == IDC_KEY_SCRIPT) {
       if (HIWORD(wParam) == EN_CHANGE) {
         need_apply = true;
       }
     }
     break;

   case WM_COMMAND:
     {
       UINT ctl = LOWORD(wParam);

       if (ctl >= IDC_KEY_SETTING_NOTE_1 && ctl <= IDC_KEY_SETTING_NOTE_12) {
         key_bind_t keydown = helpers::get_keybind();
         int note = ctl - IDC_KEY_SETTING_NOTE_1;
         keydown.c = (byte)((keydown.c / 12) * 12 + note);
         helpers::update_keybind(keydown);
         helpers::refresh_controls(hWnd);
       }

       else if (ctl >= IDC_KEY_SETTING_OCTAVE_0 && ctl <= IDC_KEY_SETTING_OCTAVE_8) {
         key_bind_t keydown = helpers::get_keybind();
         int octave = ctl - IDC_KEY_SETTING_OCTAVE_0;
         keydown.c = (byte)(12 + octave * 12 + (keydown.c % 12));
         helpers::update_keybind(keydown);
         helpers::refresh_controls(hWnd);
       }

       else if (ctl >= IDC_KEY_SETTING_CHANNEL_0 && ctl <= IDC_KEY_SETTING_CHANNEL_7) {
         key_bind_t keydown = helpers::get_keybind();
         int channel = ctl - IDC_KEY_SETTING_CHANNEL_0;
         keydown.b = channel;
         helpers::update_keybind(keydown);
         helpers::refresh_controls(hWnd);
       }

       else if (ctl == IDC_KEY_SCRIPT) {
         if (HIWORD(wParam) == EN_CHANGE) {
           need_apply = true;
         }
       }
       else if (ctl == IDC_KEY_SETTING_BUTTON_REFRESH) {
         helpers::refresh_controls(hWnd);
       }
       else if (ctl == IDC_KEY_SETTING_BUTTON_APPLY) {
         config_bind_set_label(selected_key, NULL);
         config_bind_clear_keydown(selected_key);
         config_bind_clear_keyup(selected_key);

         HWND edit = GetDlgItem(hWnd, IDC_KEY_SCRIPT);
         uint buff_size = Edit_GetTextLength(edit) + 1;
         char *buff = (char*)malloc(buff_size);
         get_window_text_local(edit, buff, buff_size);
         config_parse_keymap(buff, selected_key, -1);
         free(buff);

         helpers::refresh_controls(hWnd);
       }

       else if (ctl == IDC_KEY_SETTING_BUTTON_CLEAR) {
         config_bind_set_label(selected_key, NULL);
         config_bind_set_color(selected_key, 0);
         config_bind_clear_keydown(selected_key);
         config_bind_clear_keyup(selected_key);
         helpers::refresh_controls(hWnd);
       }

       else if (ctl == IDC_KEY_SETTING_BUTTON_PRESET) {
         if (lang_text_open(IDR_TEXT_CONTROLLERS)) {
           HWND button = GetDlgItem(hWnd, IDC_KEY_SETTING_BUTTON_PRESET);
           HMENU menu = CreatePopupMenu();
           HMENU target_menu = menu;
           char line[4096];
           int id = 0;
           while (lang_text_readline(line, sizeof(line))) {
             if (line[0] == '*') {
               target_menu = CreatePopupMenu();
               append_controller_menu_text(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(target_menu), line + 1);

             }
             if (line[0] == '#')
               append_controller_menu_text(target_menu, MF_STRING, static_cast<UINT_PTR>(++id), line + 1);
           }

           lang_text_close();

           RECT rect;
           GetClientRect(button, &rect);
           POINT pos = {rect.right, rect.top};
           ClientToScreen(button, &pos);

           UINT cmd = TrackPopupMenu(menu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD, pos.x, pos.y, 0, hWnd, NULL);
           if (cmd) {
             if (lang_text_open(IDR_TEXT_CONTROLLERS)) {
               config_bind_clear_keydown(selected_key);
               config_bind_clear_keyup(selected_key);
               config_bind_set_label(selected_key, NULL);
               config_bind_set_color(selected_key, 0);

               DWORD id = 0;
               char line[4096];
               while (lang_text_readline(line, sizeof(line))) {
                 if (line[0] == '#') {
                   if (++id > cmd)
                     break;
                 } else if (id == cmd) {
                   char temp[4096];
                   _snprintf(temp, sizeof(temp), line, config_get_key_name(selected_key));
                   temp[sizeof(temp) - 1] = 0;
                   config_parse_keymap(temp, selected_key, -1);
                 }
               }
               lang_text_close();
             }
           }
           helpers::refresh_controls(hWnd);
         }
       }
     }
     break;
  }
  return 0;
}

// popup key menu
void gui_popup_keymenu(byte code, int x, int y) {
  preview_key = selected_key = code;
  
  if (key_setting_window == NULL) {
    key_setting_window = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_KEY_SETTING), NULL, key_setting_proc);

    int x1, y1, x2, y2;
    display_get_key_rect(code, x1, y1, x2, y2);

    { POINT p = {x1, y1}; ClientToScreen(gui_get_window(), &p); x1 = p.x; y1 = p.y; }
    { POINT p = {x2, y2}; ClientToScreen(gui_get_window(), &p); x2 = p.x; y2 = p.y; }

    int border = 6;
    x = x1 + border;
    y = y2 + border;

    RECT workarea;
    if (SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0)) {
      RECT rect;
      GetWindowRect(key_setting_window, &rect);

      int width = rect.right - rect.left;
      int height = rect.bottom - rect.top;

      if (x1 + width > workarea.right && 
        x2 - width >= workarea.left) {
          x = x2 - width - border;
      }

      if (y2 + height > workarea.bottom &&
        y1 - height >= workarea.top) {
          y = y1 - height - border;
      }

      if (x + width > workarea.right) x = workarea.right - width;
      if (y + height > workarea.bottom) y = workarea.bottom - height;
    }

    SetWindowPos(key_setting_window, NULL, x , y, 0, 0, SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE);
  }

  ShowWindow(key_setting_window, SW_SHOW);
  SetForegroundWindow(key_setting_window);
  SetActiveWindow(key_setting_window);
}


static void notify_new_upate(uint version) {
  // from config.cpp
  extern int print_version(char *buff, int buff_size, uint value, const char *sep);

  if (version > APP_VERSION)
  if (version > config_get_update_version()) {
    wchar_t content[1024];
    char version_str[32];
    print_version(version_str, sizeof(version_str), version, "");
    lang_format_string_w(content, sizeof(content) / sizeof(content[0]), IDS_NOTIFY_UPDATE, version_str);

    int result = ::MessageBoxW(gui_get_window(), content, lang_load_string_w(IDS_NOTIFY_UPDATE_TITLE), MB_YESNO);
    if (result == IDYES) {
      ShellExecuteW(NULL, L"open", L"http://freepiano.tiwb.com", NULL, NULL, 0);
    }

    config_set_update_version(version);
    config_save("freepiano.cfg");
  }
}
// -----------------------------------------------------------------------------------------
// main window functions
// -----------------------------------------------------------------------------------------
static HWND mainhwnd = NULL;

#define WM_NEW_UPDATE     (WM_USER + 14)

static LRESULT CALLBACK windowproc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  static bool in_sizemove = false;

  switch (uMsg) {
   case WM_CREATE:
     DragAcceptFiles(hWnd, TRUE);
     break;

   case WM_ACTIVATE:
     keyboard_enable(WA_INACTIVE != LOWORD(wParam));
     break;

   case WM_SIZE:
     keyboard_enable(wParam != SIZE_MINIMIZED);
     break;

   case WM_SIZING: {
     if (!config_get_enable_resize_window()) {
       RECT fixed_size = { 0, 0, display_get_width(), display_get_height() };
       RECT *rect = (RECT *)lParam;
       AdjustWindowRect(&fixed_size, static_cast<DWORD>(GetWindowLongPtr(hWnd, GWL_STYLE)), GetMenu(hWnd) != NULL);

       switch (wParam) {
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
          rect->top = rect->bottom - (fixed_size.bottom - fixed_size.top);
          break;

        default:
          rect->bottom = rect->top + (fixed_size.bottom - fixed_size.top);
          break;
       }

       switch (wParam) {
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
        case WMSZ_BOTTOMLEFT:
          rect->left = rect->right - (fixed_size.right - fixed_size.left);
          break;

        default:
          rect->right = rect->left + (fixed_size.right - fixed_size.left);
          break;
       }

       return 1;
     }
   };

   case WM_PAINT:
     display_present();
     return DefWindowProc(hWnd, uMsg, wParam, lParam);

   case WM_CLOSE:
     DestroyWindow(hWnd);
     break;

   case WM_SYSCOMMAND:
     // disable sys menu
     if (wParam == SC_KEYMENU)
       return 0;
     break;

   case WM_ENTERMENULOOP:
     keyboard_enable(false);
     break;

   case WM_EXITMENULOOP:
     keyboard_enable(true);
     break;

   case WM_EXITSIZEMOVE:
     in_sizemove = false;
     break;

   case WM_TIMER:
     display_render();
     break;

   case WM_DESTROY:
     // quit application
     PostQuitMessage(0);
     break;

   case WM_MENUCOMMAND:
     return menu_on_command(hWnd, uMsg, wParam, lParam);

   case WM_INITMENUPOPUP:
   case WM_UNINITMENUPOPUP:
     return menu_on_popup(hWnd, uMsg, wParam, lParam);


   case WM_DROPFILES: {
     HDROP drop = (HDROP)wParam;
     char filepath[MAX_PATH];
     int len = DragQueryFileA(drop, 0, filepath, sizeof(filepath));

     if (len > 0) {
       const char *extension = PathFindExtensionA(filepath);

       // drop a music file
       if (_stricmp(extension, ".lyt") == 0) {
         try_open_song(song_open_lyt(filepath));
         return 0;
       }

       if (_stricmp(extension, ".fpm") == 0) {
         try_open_song(song_open(filepath));
         return 0;
       }

       // drop a instrument
       if (_stricmp(extension, ".dll") == 0) {
         config_select_instrument(INSTRUMENT_TYPE_VSTI, filepath);
         return 0;
       }

       // drop a map file
       if (_stricmp(extension, ".map") == 0) {
         config_set_keymap(filepath);
         return 0;
       }
     }
   }
   break;

   case WM_DEVICECHANGE: {
     midi_open_inputs();
   }
   break;

   case WM_NEW_UPDATE:
     notify_new_upate(lParam);
     break;
  }

  // display process message
  if (int ret = display_process_message(hWnd, uMsg, wParam, lParam))
    return ret;

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// init gui system
int gui_init() {
  HINSTANCE hInstance = GetModuleHandle(NULL);

  // register window class
  WNDCLASSEXW wc = { sizeof(wc), 0 };
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = &windowproc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_NORMAL));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = L"FreePianoMainWindow";
  wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_SMALL));

  RegisterClassExW(&wc);

  // create main menu
  menu_main = CreateMenu();

  // init menu
  menu_init();

  int screenwidth = GetSystemMetrics(SM_CXSCREEN);
  int screenheight = GetSystemMetrics(SM_CYSCREEN);


#if FULLSCREEN
  RECT rect = {0, 0, screenwidth, screenheight};
  uint style = WS_POPUP;

  AdjustWindowRect(&rect, style, FALSE);

  // create window
  mainhwnd = CreateWindowW(L"FreePianoMainWindow", APP_NAME_W, style,
                          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                          NULL, NULL, hInstance, NULL);
#else
  RECT rect;
  rect.left = (screenwidth - display_get_width()) / 2;
  rect.top = (screenheight - display_get_height()) / 2;
  rect.right = rect.left + display_get_width();
  rect.bottom = rect.top + display_get_height();

  uint style = WS_OVERLAPPEDWINDOW;

  AdjustWindowRect(&rect, style, TRUE);

  // create window
  mainhwnd = CreateWindowW(L"FreePianoMainWindow", APP_NAME_W, style,
                          rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                          NULL, menu_main, hInstance, NULL);
#endif

  if (mainhwnd == NULL) {
    fp_log_error(L"Failed to create main window");
    return -1;
  }

  // disable ime
  ImmAssociateContext(gui_get_window(), NULL);

  // create timer
  SetTimer(mainhwnd, 0, 1, NULL);

  return 0;
}

// shutdown gui system
void gui_shutdown() {
  menu_shutdown();

  if (mainhwnd) {
    DestroyWindow(mainhwnd);
    mainhwnd = NULL;
  }
}

// get main window handle
HWND gui_get_window() {
  return mainhwnd;
}

// show gui
void gui_show() {
  ShowWindow(mainhwnd, SW_SHOW);
  SetActiveWindow(mainhwnd);
  SetForegroundWindow(mainhwnd);
  display_render();
}

// get selected key
int gui_get_selected_key() {
  return preview_key;
}

static HWND export_hwnd = NULL;

static INT_PTR CALLBACK export_progress_proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
   case WM_INITDIALOG: {
     lang_localize_dialog(hWnd);

     // output volume slider
     HWND progress_bar = GetDlgItem(hWnd, IDC_EXPORT_PROGRESS);

     SendMessage(progress_bar, PBM_SETRANGE,
                 static_cast<WPARAM>(TRUE),                  // redraw flag
                 static_cast<LPARAM>(MAKELONG(0, 100)));     // min. & max. positions

     export_hwnd = hWnd;
   }
   break;

   case WM_COMMAND:
     switch (LOWORD(wParam)) {
      case IDCANCEL:
        EndDialog(hWnd, 0);
        break;
     }
     break;

   case WM_CLOSE:
     EndDialog(hWnd, 0);
     break;

   case WM_DESTROY:
     export_hwnd = NULL;
     break;
  }
  return 0;
}

// show export progress
int gui_show_export_progress() {
  return DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXPORTING), gui_get_window(), export_progress_proc);
}

// update progress
void gui_update_export_progress(int progress) {
  // output volume slider
  HWND progress_bar = GetDlgItem(export_hwnd, IDC_EXPORT_PROGRESS);
  PostMessage(progress_bar, PBM_SETPOS, static_cast<WPARAM>(progress), 0);
}

// close hide progress
void gui_close_export_progress() {
  PostMessage(export_hwnd, WM_CLOSE, 0, 0);
}

// is exporting
bool gui_is_exporting() {
  return export_hwnd != NULL;
}

// refresh all visible UI texts after language changes
void gui_refresh_all_texts() {
  if (mainhwnd) {
    SetWindowTextW(mainhwnd, APP_NAME_W);
  }

  menu_init();

  if (mainhwnd) {
    DrawMenuBar(mainhwnd);
    InvalidateRect(mainhwnd, NULL, FALSE);
    UpdateWindow(mainhwnd);
  }

  display_force_refresh();

  if (setting_hwnd) {
    fp_log_info(L"Refreshing settings window after language change");
    DestroyWindow(setting_hwnd);
  }

  if (key_setting_window) {
    fp_log_info(L"Refreshing key setting window after language change");
    DestroyWindow(key_setting_window);
  }
}

// change language
void gui_set_language(int lang) {
  config_set_ui_language(lang);
  lang_set_current(lang);
  gui_refresh_all_texts();
}

// notify update
void gui_notify_update(uint version) {
  PostMessage(mainhwnd, WM_NEW_UPDATE, 0, version);
}