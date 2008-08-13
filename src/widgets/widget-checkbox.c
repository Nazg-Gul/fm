/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Implementation of widget checkbox
 *
 * Copyright 2008 Sergey I. Sharybin <nazgul@school9.perm.ru>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#include "widget.h"
#include "screen.h"
#include "util.h"

#include <wchar.h>

/**
 * Destroys a checkbox widget
 *
 * @param __checkbox is a checkbox to be destroyed
 * @return 0 if successful, otherwise -1
 */
static int
checkbox_destructor(w_checkbox_t *__checkbox)
{
  if (!__checkbox)
    {
      return -1;
    }

  SAFE_FREE(__checkbox->caption);

  if (WIDGET_LAYOUT(__checkbox))
    scr_destroy_window(WIDGET_LAYOUT(__checkbox));

  return 0;
}

/**
 * Draw a checkbox
 *
 * @param __checkbox is a checkbox to be drawn
 * @return 0 if successful, otherwise -1
 */
static int
checkbox_drawer(w_checkbox_t *__checkbox)
{
  scr_window_t layout = WIDGET_LAYOUT(__checkbox);

  if (!__checkbox || !WIDGET_VISIBLE(__checkbox) || !layout)
    {
      return -1;
    }

  scr_wnd_attr_backup(layout);
  scr_wnd_move_caret(layout, __checkbox->position.x, __checkbox->position.y);

  if (__checkbox->caption)
    {
      scr_wnd_font(layout, __checkbox->focused ? *__checkbox->font_focus
          : *__checkbox->font);

      scr_wnd_printf(layout, "[%s] ", __checkbox->ischeck ? "x" : " ");

      widget_shortcut_print(layout, __checkbox->caption,
          __checkbox->focused ? *__checkbox->font_focus : *__checkbox->font,
          __checkbox->focused ? *__checkbox->hotkey_focus
              : *__checkbox->hotkey_font);
    }

  scr_wnd_attr_restore(layout);

  return 0;
}

/**
 * Handles a keydown callback
 *
 * @param __checkbox is a checkbox received a callback
 * @param __ch is a received character
 * @return FALSE if callback hasn't handled received character
 */
static int
checkbox_keydown(w_checkbox_t *__checkbox, wint_t __ch)
{
  _WIDGET_CALL_USER_CALLBACK(__checkbox, keydown, __checkbox, __ch);

  switch (__ch)
    {
  case KEY_SPACE:
    __checkbox->ischeck = !__checkbox->ischeck;
    widget_redraw(WIDGET (__checkbox));
    return TRUE;
    }

  return FALSE;
}

/**
 * Handles a shortcut callback
 *
 * @param __checkbox is a checkbox for which this callback sent
 * @return zero if callback hasn't handled received character,
 *   non-zero otherwise
 */
static int
checkbox_shortcut(w_checkbox_t *__checkbox)
{
  _WIDGET_CALL_USER_CALLBACK (__checkbox, shortcut, __checkbox);
  widget_set_focus(WIDGET(__checkbox));

  return checkbox_keydown(__checkbox, KEY_SPACE);
}

/**
 * Create new checkbox
 *
 * @param __parent is a parent of the checkbox. Should be container.
 * @param __caption is a caption on checkbox
 * @param __x, __y is a coordinates  of checkbox
 * @param __check is a check state of checkbox
 * @return a pointer to checkbox object if successful, otherwise NULL
 */
w_checkbox_t *
widget_create_checkbox(w_container_t *__parent, const wchar_t *__caption,
    int __x, int __y, BOOL __check)
{
  w_checkbox_t *widget;

  if (!__parent || !__caption)
    {
      return NULL;
    }

  WIDGET_INIT (widget, w_checkbox_t, WT_CHECKBOX, __parent, WF_NOLAYOUT,
      checkbox_destructor, checkbox_drawer,
      __x, __y, 1, wcslen(__caption) + 4, 1);

  WIDGET_CALLBACK(widget, keydown) = (widget_keydown_proc) checkbox_keydown;
  WIDGET_CALLBACK(widget, shortcut) = (widget_action) checkbox_shortcut;

  WIDGET_SHORTCUT (widget) = widget_shortcut_key(__caption);

  widget->caption = wcsdup(__caption);

  widget->font = &FONT(CID_BLACK, CID_WHITE);
  widget->font_focus = &FONT(CID_BLACK, CID_CYAN);
  widget->hotkey_font = &FONT(CID_YELLOW, CID_WHITE);
  widget->hotkey_focus = &FONT(CID_YELLOW, CID_CYAN);

  widget->ischeck = __check;

  WIDGET_POST_INIT(widget);

  return widget;
}

/**
 * Get check state from the checkbox
 *
 * @param __checkbox is a checkbox
 * @return TRUE if checked, otherwise FALSE
 */
BOOL
w_checkbox_get(const w_checkbox_t *__checkbox)
{
  return __checkbox->ischeck;
}