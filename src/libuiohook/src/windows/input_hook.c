/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
 * https://github.com/kwhat/libuiohook/
 *
 * libUIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libUIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <uiohook.h>
#include <windows.h>

#include "input_helper.h"
#include "logger.h"

#ifndef FOREGROUND_TIMER_MS
#define FOREGROUND_TIMER_MS 83 // 12 fps
#endif

// Thread and hook handles.
static DWORD hook_thread_id = 0;
static HHOOK keyboard_event_hhook = NULL, mouse_event_hhook = NULL;
static HWINEVENTHOOK win_foreground_hhook = NULL, win_minimizeend_hhook = NULL;
static UINT_PTR foreground_timer = 0;

static HWND foreground_window = NULL;
static bool is_blocked_by_uipi = true;

static UINT WM_UIOHOOK_UIPI_TEST = WM_NULL;

// The handle to the DLL module pulled in DllMain on DLL_PROCESS_ATTACH.
extern HINSTANCE hInst;

// Modifiers for tracking key masks.
static unsigned short int current_modifiers = 0x0000;

// Click count globals.
static unsigned short click_count = 0;
static DWORD click_time = 0;
static unsigned short int click_button = MOUSE_NOBUTTON;
static POINT last_click;

// Static event memory.
static uiohook_event event;

// Event dispatch callback.
static dispatcher_t dispatcher = NULL;

UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatcher = dispatch_proc;
}

// Send out an event if a dispatcher was set.
static inline void dispatch_event(uiohook_event *const event) {
    if (dispatcher != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, event->type);

        dispatcher(event);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

static void initialize_modifiers();

// Set the native modifier mask for future events.
static inline void set_modifier_mask(unsigned short int mask) {
    current_modifiers |= mask;
}

// Unset the native modifier mask for future events.
static inline void unset_modifier_mask(unsigned short int mask) {
    current_modifiers &= ~mask;
}

// Get the current native modifier mask state.
static inline unsigned short int get_modifiers() {
    if (is_blocked_by_uipi) {
        initialize_modifiers();
        is_blocked_by_uipi = false;
    }
    return current_modifiers;
}

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers() {
    current_modifiers = 0x0000;

    // NOTE We are checking the high order bit, so it will be < 0 for a singed short.
    if (GetAsyncKeyState(VK_LSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_L);     }
    if (GetAsyncKeyState(VK_RSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_R);     }
    if (GetAsyncKeyState(VK_LCONTROL) < 0) { set_modifier_mask(MASK_CTRL_L);      }
    if (GetAsyncKeyState(VK_RCONTROL) < 0) { set_modifier_mask(MASK_CTRL_R);      }
    if (GetAsyncKeyState(VK_LMENU)    < 0) { set_modifier_mask(MASK_ALT_L);       }
    if (GetAsyncKeyState(VK_RMENU)    < 0) { set_modifier_mask(MASK_ALT_R);       }
    if (GetAsyncKeyState(VK_LWIN)     < 0) { set_modifier_mask(MASK_META_L);      }
    if (GetAsyncKeyState(VK_RWIN)     < 0) { set_modifier_mask(MASK_META_R);      }

    if (GetAsyncKeyState(VK_LBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON1);     }
    if (GetAsyncKeyState(VK_RBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON2);     }
    if (GetAsyncKeyState(VK_MBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON3);     }
    if (GetAsyncKeyState(VK_XBUTTON1) < 0) { set_modifier_mask(MASK_BUTTON4);     }
    if (GetAsyncKeyState(VK_XBUTTON2) < 0) { set_modifier_mask(MASK_BUTTON5);     }

    if (GetAsyncKeyState(VK_NUMLOCK)  < 0) { set_modifier_mask(MASK_NUM_LOCK);    }
    if (GetAsyncKeyState(VK_CAPITAL)  < 0) { set_modifier_mask(MASK_CAPS_LOCK);   }
    if (GetAsyncKeyState(VK_SCROLL)   < 0) { set_modifier_mask(MASK_SCROLL_LOCK); }
}

void check_and_update_uipi_state(HWND hwnd) {
    SetLastError(ERROR_SUCCESS);
    PostMessage(hwnd, WM_UIOHOOK_UIPI_TEST, 0, 0);
    if (GetLastError() == ERROR_ACCESS_DENIED) {
        is_blocked_by_uipi = true;
    }
}

void unregister_running_hooks() {
    // Stop the event hook and any timer still running.
    if (win_foreground_hhook != NULL) {
        UnhookWinEvent(win_foreground_hhook);
        win_foreground_hhook = NULL;
    }

    if (win_minimizeend_hhook != NULL) {
        UnhookWinEvent(win_minimizeend_hhook);
        win_minimizeend_hhook = NULL;
    }

    if (foreground_timer != 0) {
        KillTimer(NULL, foreground_timer);
        foreground_timer = 0;
    }

    // Destroy the native hooks.
    if (keyboard_event_hhook != NULL) {
        UnhookWindowsHookEx(keyboard_event_hhook);
        keyboard_event_hhook = NULL;
    }

    if (mouse_event_hhook != NULL) {
        UnhookWindowsHookEx(mouse_event_hhook);
        mouse_event_hhook = NULL;
    }
}

void hook_start_proc() {
    // Initialize native input helper functions.
    load_input_helper();

    // Get the local system time in UNIX epoch form.
    uint64_t timestamp = GetMessageTime();

    // Populate the hook start event.
    event.time = timestamp;
    event.reserved = 0x00;

    event.type = EVENT_HOOK_ENABLED;
    event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&event);
}

void hook_stop_proc() {
    // Get the local system time in UNIX epoch form.
    uint64_t timestamp = GetMessageTime();

    // Populate the hook stop event.
    event.time = timestamp;
    event.reserved = 0x00;

    event.type = EVENT_HOOK_DISABLED;
    event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&event);

    // Deinitialize native input helper functions.
    unload_input_helper();
}

// Process a key pressed event.
static void process_key_pressed(KBDLLHOOKSTRUCT *kbhook) {
    // Check and setup modifiers.
    if      (kbhook->vkCode == VK_LSHIFT)   { set_modifier_mask(MASK_SHIFT_L);    }
    else if (kbhook->vkCode == VK_RSHIFT)   { set_modifier_mask(MASK_SHIFT_R);    }
    else if (kbhook->vkCode == VK_LCONTROL) { set_modifier_mask(MASK_CTRL_L);     }
    else if (kbhook->vkCode == VK_RCONTROL) { set_modifier_mask(MASK_CTRL_R);     }
    else if (kbhook->vkCode == VK_LMENU)    { set_modifier_mask(MASK_ALT_L);      }
    else if (kbhook->vkCode == VK_RMENU)    { set_modifier_mask(MASK_ALT_R);      }
    else if (kbhook->vkCode == VK_LWIN)     { set_modifier_mask(MASK_META_L);     }
    else if (kbhook->vkCode == VK_RWIN)     { set_modifier_mask(MASK_META_R);     }
    
    // Track number of clicks.
    if ((long) kbhook->time <= (long) click_time + GetDoubleClickTime()) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: %u milliseconds elapsed between last press.\n",
                __FUNCTION__, __LINE__, (unsigned int) (kbhook->time - click_time));
    }
    else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Resetting click count.\n",
                __FUNCTION__, __LINE__);
    }

    // Get the virtual key code.
    unsigned short int vk_code = (unsigned short int) kbhook->vkCode;
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%u)\n",
            __FUNCTION__, __LINE__, vk_code, click_count);

    // Set the event time.
    event.time = kbhook->time;
    
    // Do some checking if this event is a special or extended key.
    // Handling for malformed capslock and reserved keys that function like capslock.
    bool is_reserved = (kbhook->vkCode == VK_ICO_HELP || kbhook->vkCode == VK_ICO_00 || 
                (kbhook->vkCode >= VK_ICO_CLEAR && kbhook->vkCode <= VK_ICO_F17) || 
                (kbhook->vkCode >= VK_KANJI && kbhook->vkCode <= VK_DBE_KATAKANA));
    if (kbhook->vkCode == VK_CAPITAL || is_reserved) {
        if (kbhook->vkCode == VK_NUMLOCK) {
            set_modifier_mask(MASK_NUM_LOCK);
        }
        else if (kbhook->vkCode == VK_CAPITAL) {
            set_modifier_mask(MASK_CAPS_LOCK);
        }
        else if (kbhook->vkCode == VK_SCROLL) {
            set_modifier_mask(MASK_SCROLL_LOCK);
        }

        event.reserved = 0x00;
    }
    else {
        event.reserved = 0x01;
    }

    // Populate event structure.
    event.type = EVENT_KEY_PRESSED;
    event.mask = get_modifiers();
    
    // Converts the XKB keycode to an X11 keycode using the lookup table keycode_to_scancode.
    event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode, kbhook->flags);
    event.data.keyboard.rawcode = kbhook->vkCode;
    event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_INFO,   "%s [%u]: Key %#X was pressed. (%#X)\n",
            __FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);

    // Fire key pressed event.
    dispatch_event(&event);

    if ((event.mask & (MASK_CTRL)) &&
        (event.mask & (MASK_ALT)) &&
        (event.data.keyboard.keycode == VC_DELETE)) {
        current_modifiers = 0;
    }

    // If the pressed event was not consumed...
    if (event.reserved ^ 0x01) {
    }
}

// Process a key released event.
static void process_key_released(KBDLLHOOKSTRUCT *kbhook) {
    // Check and setup modifiers.
    if      (kbhook->vkCode == VK_LSHIFT)   { unset_modifier_mask(MASK_SHIFT_L);    }
    else if (kbhook->vkCode == VK_RSHIFT)   { unset_modifier_mask(MASK_SHIFT_R);    }
    else if (kbhook->vkCode == VK_LCONTROL) { unset_modifier_mask(MASK_CTRL_L);     }
    else if (kbhook->vkCode == VK_RCONTROL) { unset_modifier_mask(MASK_CTRL_R);     }
    else if (kbhook->vkCode == VK_LMENU)    { unset_modifier_mask(MASK_ALT_L);      }
    else if (kbhook->vkCode == VK_RMENU)    { unset_modifier_mask(MASK_ALT_R);      }
    else if (kbhook->vkCode == VK_LWIN)     { unset_modifier_mask(MASK_META_L);     }
    else if (kbhook->vkCode == VK_RWIN)     { unset_modifier_mask(MASK_META_R);     }

    // Get the virtual key code.
    unsigned short int vk_code = (unsigned short int) kbhook->vkCode;
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%u)\n",
            __FUNCTION__, __LINE__, vk_code, click_count);

    // Set the event time.
    event.time = kbhook->time;

    // Set this to mark the event as a released event.
    event.reserved = 0x00;

    // Populate event structure.
    event.type = EVENT_KEY_RELEASED;
    event.mask = get_modifiers();

    // Converts the XKB keycode to an X11 keycode using the lookup table keycode_to_scancode.
    event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode, kbhook->flags);
    event.data.keyboard.rawcode = kbhook->vkCode;
    event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_INFO, "%s [%u]: Key %#X was released. (%#X)\n",
            __FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&event);

    // Make sure to reset toggle behavior after key release.
    if (kbhook->vkCode == VK_NUMLOCK) {
        if (GetKeyState(VK_NUMLOCK) & 0x01) {
            set_modifier_mask(MASK_NUM_LOCK);
        } else {
            unset_modifier_mask(MASK_NUM_LOCK);
        }
    }
    else if (kbhook->vkCode == VK_CAPITAL) {
        if (GetKeyState(VK_CAPITAL) & 0x01) {
            set_modifier_mask(MASK_CAPS_LOCK);
        } else {
            unset_modifier_mask(MASK_CAPS_LOCK);
        }
    }
    else if (kbhook->vkCode == VK_SCROLL) {
        if (GetKeyState(VK_SCROLL) & 0x01) {
            set_modifier_mask(MASK_SCROLL_LOCK);
        } else {
            unset_modifier_mask(MASK_SCROLL_LOCK);
        }
    }
}

// Callback function that handles events.
void CALLBACK win_hook_event_proc(HWINEVENTHOOK hook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
    foreground_window = hWnd;
    check_and_update_uipi_state(hWnd);
}

static VOID CALLBACK foreground_timer_proc(HWND _hwnd, UINT msg, UINT_PTR timerId, DWORD dwmsEventTime)
{
    HWND system_foreground = GetForegroundWindow();

    if (foreground_window != system_foreground) {
        foreground_window = system_foreground;
        check_and_update_uipi_state(system_foreground);
    }
}

UIOHOOK_API int hook_run() {
    int status = UIOHOOK_FAILURE;

    // Set the thread id we want to signal later.
    hook_thread_id = GetCurrentThreadId();

    // Create the native hooks.
    keyboard_event_hhook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_event_proc, hInst, 0);
    mouse_event_hhook = SetWindowsHookEx(WH_MOUSE_LL, mouse_hook_event_proc, hInst, 0);

    win_foreground_hhook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL, win_hook_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    win_minimizeend_hhook = SetWinEventHook(
            EVENT_SYSTEM_MINIMIZEEND, EVENT_SYSTEM_MINIMIZEEND,
            NULL, win_hook_event_proc, 0, 0, WINEVENT_OUTOFCONTEXT);
    foreground_timer = SetTimer(NULL, 0, FOREGROUND_TIMER_MS, foreground_timer_proc);

    WM_UIOHOOK_UIPI_TEST = RegisterWindowMessage("UIOHOOK_UIPI_TEST");

    foreground_window = GetForegroundWindow();
    is_blocked_by_uipi = true; // init modifiers

    // If we did not encounter a problem, start processing events.
    if (keyboard_event_hhook != NULL && mouse_event_hhook != NULL) {
        if (win_foreground_hhook == NULL || win_minimizeend_hhook == NULL) {
            logger(LOG_LEVEL_WARN, "%s [%u]: SetWinEventHook() failed!\n",
                    __FUNCTION__, __LINE__);
        }

        logger(LOG_LEVEL_DEBUG, "%s [%u]: SetWindowsHookEx() successful.\n",
                __FUNCTION__, __LINE__);

        // Set the exit status.
        status = UIOHOOK_SUCCESS;

        // Windows does not have a hook start event or callback so we need to
        // manually fake it.
        hook_start_proc();

        MSG message;
        while (GetMessage(&message, NULL, 0, 0)) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        
        // Windows hook_event_proc has a special case for re-enable.
        // NOTE We still execute the cleanup even if the thread does not terminate!
        // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms644986.aspx
        unregister_running_hooks();
        
        // Windows does not have a hook stop event or callback so we need to
        // manually fake it.
        hook_stop_proc();
    }
    else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SetWindowsHookEx() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());

        status = UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX;
    }
    
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);
    
    return status;
}

UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    // Try to exit the thread naturally.
    if (PostThreadMessage(hook_thread_id, WM_QUIT, (WPARAM) NULL, (LPARAM) NULL)) {
        status = UIOHOOK_SUCCESS;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
