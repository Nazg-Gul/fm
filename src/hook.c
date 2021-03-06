/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Hooks implementation
 *
 * Copyright 2008 Sergey I. Sharybin <g.ulairi@gmail.com>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#include "hook.h"
#include "types.h"
#include "deque.h"
#include "dynstruct.h"

#include <malloc.h>
#include <wchar.h>

typedef struct
{
  hook_callback_proc callback;
  unsigned long priority;
} hook_action_t;
#define HOOK_ACTION(x) ((hook_action_t *)x)

typedef struct
{
  wchar_t *name;
  deque_t *actions_list;
} hook_t;
#define HOOK(x) ((hook_t *) x)

static deque_t *hooks;

/**
 * Comparison function of the hooks's names
 *
 * @param h0 is a hook
 * @param h2 is a wide char string
 * @return look man wcscmp(3)
 */
static int
hook_name_cmp (const void *h0, const void *h2)
{
  return wcscmp (HOOK (h0)->name, (wchar_t *) h2);
}

/**
 * Comparison function of the hooks's actions priorities
 *
 * @param ha0 is the first hook's action
 * @param ha2 is the second hook's action
 * @return zero if h0 priority and h2 priority are equal,
 * greater than zero if h0 priority greater than h2 priority,
 * less than zero if h0 priority less than h2 priority
 */
static int
hook_action_cmp (const void *ha0, const void *ha2)
{
  return HOOK_ACTION (ha0)->priority - HOOK_ACTION (ha2)->priority;
}

/**
 * Comparison function of the callbacks addresses
 *
 * @param hc0 is a iterator of hook's actions
 * @param hc2 is a callback's address
 * @return zero if hc0 address and hc2 address are equal,
 * greater than zero if hc0 address greater than hc2 address,
 * less than zero if hc0 address less than hc2 address
 */
static int
hook_callback_cmp (const void *hc0, const void *hc2)
{
  return memcmp (HOOK_ACTION (hc0)->callback, hc2, sizeof (hc2));
}

/**
 * Hook destructor handler
 *
 * @param h is a hook for destroying
 */
void
hook_destroy (void *__h)
{
  hook_t *hook = HOOK (__h);

  deque_destroy (hook->actions_list, NULL);
  SAFE_FREE (hook->name);
}

/**
 * Call a hook
 *
 * @param __name is a hook's name
 * @param __data is a internal data, the parameters of handles
 * @return HOOK_SUCCESS if successful, HOOK_BREAK if hook callback has stopped
 * call process, and HOOK_FAILURE otherwise
 */
int
hook_call (const wchar_t *__name, dynstruct_t *__data)
{
  iterator_t *hook;
  if (!__name)
    {
      return HOOK_FAILURE;
    }

  hook = deque_find (hooks, __name, hook_name_cmp);
  if (hook != NULL)
    {
      hook_t *h = HOOK (hook->data);
      hook_action_t *ha;

      deque_foreach (h->actions_list, ha);
        if (ha->callback (__data) != 0)
          {
            return HOOK_BREAK;
          }
      deque_foreach_done;
    }
  else
    {
      return HOOK_FAILURE;
    }

  return HOOK_SUCCESS;
}

/**
 * Add a new hook, or a new handler if the hook already exists
 *
 * @param __name is a hook's name
 * @param __callback is the function-handler
 * @param __priority is the priority of the function-handler
 * @return HOOK_SUCCESS if successful, otherwise HOOK_FAILURE
 */
int
hook_register (const wchar_t *__name, hook_callback_proc __callback,
               int __priority)
{
  iterator_t *iter;

  if (hooks == NULL)
    {
      hooks = deque_create ();
    }

  if (__name == NULL)
    {
      return HOOK_FAILURE;
    }

  hook_action_t *new_action =
          (hook_action_t *) malloc (sizeof (hook_action_t));

  new_action->priority = __priority;
  new_action->callback = __callback;

  iter = deque_find (hooks, __name, hook_name_cmp);
  if (iter)
    {
      deque_sorted_insert (HOOK (iter->data)->actions_list,
                           new_action, hook_action_cmp);
    }
  else
    {
      hook_t *new_hook = (hook_t *) malloc (sizeof (hook_t));

      new_hook->name = wcsdup (__name);
      new_hook->actions_list = deque_create ();
      deque_push_back (new_hook->actions_list, new_action);
      deque_push_back (hooks, new_hook);
    }

  return HOOK_SUCCESS;
}

/**
 * Remove handler of the hook
 *
 * @param __name is a hook's name
 * @param __callback is the function-handler
 * @return HOOK_SUCCESS if successful, otherwise HOOK_FAILURE
 */
int
hook_unhook (const wchar_t *__name, hook_callback_proc __callback)
{
  iterator_t *iter;
  if (__name == NULL|| __callback == NULL)
    {
      return HOOK_FAILURE;
    }

  iter = deque_find (hooks, __name, hook_name_cmp);
  if (iter)
    {
      iterator_t *iter2 = deque_find (HOOK (iter->data)->actions_list,
                                      __callback, hook_callback_cmp);
      if (iter)
        {
          deque_remove (HOOK (iter->data)->actions_list, iter2, NULL);
        }
      else
        {
          return HOOK_FAILURE;
        }

    }
  else
    {
      return HOOK_FAILURE;
    }

  return HOOK_SUCCESS;
}

/**
 * Remove the hook
 *
 * @param __name is a hook's name
 * @return HOOK_SUCCESS if successful, otherwise HOOK_FAILURE
 */
int
hook_unregister (const wchar_t *__name)
{
  iterator_t *iter;
  if (__name == NULL)
    {
      return HOOK_FAILURE;
    }

  iter = deque_find (hooks, __name, hook_name_cmp);
  if (iter)
    {
      deque_remove (hooks, iter, hook_destroy);
    }
  else
    {
      return HOOK_FAILURE;
    }

  return HOOK_SUCCESS;
}

/**
 * Remove all the hooks
 *
 * @return HOOK_SUCCESS if successful, otherwise HOOK_FAILURE
 */
int
hooks_destroy (void)
{
  if (hooks == NULL)
    {
      return HOOK_FAILURE;
    }

  deque_destroy (hooks, hook_destroy);
  return HOOK_SUCCESS;
}
