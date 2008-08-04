/**
 * ${project-name} - a GNU/Linux console-based file manager
 *
 * Implementation of action 'copy'
 *
 * Copyright 2008 Sergey I. Sharybin <nazgul@school9.perm.ru>
 * Copyright 2008 Alex A. Smirnov <sceptic13@gmail.com>
 *
 * This program can be distributed under the terms of the GNU GPL.
 * See the file COPYING.
 */

#include "actions.h"
#include "messages.h"
#include "i18n.h"
#include "dir.h"
#include "widget.h"
#include "widget.h"

#include <vfs/vfs.h>

#include <fcntl.h>
#include <time.h>
#include <errno.h>

/********
 * Constants and other definitions
 */

#define BUF_SIZE 4096

#define BUF_LEN(_buf) \
  sizeof (_buf)/sizeof (wchar_t)

/**
 * Closes file descriptors in copy_file()
 */
#define CLOSE_FD() \
  { \
    vfs_close (fd_src); \
    vfs_close (fd_dst); \
  }

/**
 * Closes file descriptors and return a value from copy_file()
 */
#define COPY_RETERR(_code) \
  CLOSE_FD (); \
  /* Check is file retrieved completely */ \
  if (_code && fd_dst) \
    { \
      if (message_box (L"Question", \
        _(L"Incomplete file was retrieved. Keep it?"), MB_YESNO)==MR_NO) \
        { \
          vfs_unlink (__dst); \
        } \
    } \
  if (_code==MR_CANCEL) \
    _code=MR_ABORT; \
  return _code;

/**
 * Sets current file name in copy_file()
 */
#define COPY_SET_FN(_fn, _dst, _text) \
  { \
    fit_filename (_fn, \
      __proc_wnd->window->position.width+1-wcslen (_(_text L": %ls")), fn); \
    swprintf (msg, BUF_LEN (msg), _(_text L": %ls"), fn); \
    w_text_set (__proc_wnd->_dst, msg); \
  }

/**
 * Makes action and if it failed asks to retry this action
 */
#define REP(_act, _error_proc, _error_act, _error, _error_args...) \
  do { \
    _act; \
    /* Error while making action */ \
    if (res) \
      { \
        res=_error_proc (_error, ##_error_args); \
        /* Review user's decision */ \
        if (res==MR_IGNORE) \
          { \
            /* User ignored error */ \
            break; \
          } \
        if (res!=MR_RETRY) \
          { \
            /* User doen't want to continue trying */ \
            _error_act; \
          } \
      } else { \
        break; \
      } \
  } while (TRUE);

#define COPY_FILE_REP(_act, _error_proc, _error, _error_args...) \
  REP (_act, _error_proc, COPY_RETERR (res), _error, ##_error_args)

#define COPY_DIR_REP(_act, _error_proc, _error, _error_args...) \
  REP (_act, _error_proc, return res, _error, ##_error_args)

/**
 * Opens file descriptor
 * If some errors occurred, asks user what to do
 */
#define OPEN_FD(_fd, _url, _flags, _res, _error, _error_args...) \
  REP (_fd=vfs_open (_url, _flags, &_res, 0), error, _error, ##_error_args)

/**
 * Allocates memory for full file name in directory listing cycle
 */
#define ALLOC_FN(_s, _len, _src) \
  { \
    _len=wcslen (_src)+MAX_FILENAME_LEN+1; \
    _s=malloc ((_len+1)*sizeof (wchar_t)); \
  }

/**
 * Creates text on window with question about file existent
 */
#define EXIST_QUESTION_TEXT(_y, _format, _args...) \
  { \
    swprintf (buf, BUF_LEN (buf), _(_format), ##_args); \
    text=widget_create_text (WIDGET_CONTAINER (wnd), buf, 1, _y); \
    w_text_set_font (text, &FONT (CID_WHITE,  CID_RED)); \
  }

/**
 * Creates button on window with question about file existent
 */
#define EXIST_QUESTION_BUTTON(_y, _caption, _modal_result) \
  { \
    btn=widget_create_button (WIDGET_CONTAINER (wnd), _(_caption), \
      cur_left, _y, 0); \
    btn->modal_result=_modal_result; \
    cur_left+=widget_shortcut_length (_(_caption))+5; \
    w_button_set_fonts (btn, \
      &FONT (CID_WHITE,  CID_RED), \
      &FONT (CID_BLACK,  CID_WHITE), \
      &FONT (CID_YELLOW, CID_RED), \
      &FONT (CID_YELLOW, CID_WHITE)); \
  }

/**
 * Save answer about overwriting all existent targets
 */
#define SAVE_OWR_ALL_RULE(_a) \
  if (__owr_all_rule) (*__owr_all_rule)=_a;

/**
 * Modal results for overwrite answers
 */
#define MR_APPEND       (MR_CUSTOM+1)
#define MR_ALL          (MR_CUSTOM+2)
#define MR_UPDATE       (MR_CUSTOM+3)
#define MR_SIZE_DIFFERS (MR_CUSTOM+4)
#define MR_MY_NONE      (MR_CUSTOM+5)

typedef struct {
  w_window_t *window;

  w_text_t   *source;
  w_text_t   *target;

  w_progress_t *file_progress;
}  process_window_t;

/********
 * Internal stuff
 */

/**
 * Displays an error message with buttons Retry, Skip and cancel
 *
 * @param __text - text to display on message
 * @return result of message_box()
 */
static int
error                             (const wchar_t *__text, ...)
{
  wchar_t buf[4096];
  PACK_ARGS (__text, buf, BUF_LEN (buf));
  return message_box (_(L"Error"), buf,  MB_CRITICAL | MB_RETRYSKIPCANCEL);
}

/**
 * Displays an error message with buttons Retry, Ignore and cancel
 *
 * @param __text - text to display on message
 * @return result of message_box()
 */
static int
error2                            (const wchar_t *__text, ...)
{
  wchar_t buf[4096];
  PACK_ARGS (__text, buf, BUF_LEN (buf));
  return message_box (_(L"Error"), buf,  MB_CRITICAL | MB_RETRYIGNCANCEL);
}

/**
 * Returns real destination filename
 *
 * @param __src - URL of source
 * @param __dst - URL of destination
 * @return real destination filename
 */
static wchar_t*
get_real_dst                      (const wchar_t *__src, const wchar_t *__dst)
{
  wchar_t *rdst, *fn=wcfilename (__src);
  size_t len=wcslen (__dst)+wcslen (fn)+1;
  rdst=malloc ((len+1)*sizeof (wchar_t));
  swprintf (rdst, len+1, L"%ls/%ls", __dst, fn);
  free (fn);
  return rdst;
}

/**
 * Formats information of file for file_exists_question()
 *
 * @param __buf - destination buffer
 * @param __buf_size - size of buffer
 * @param __url - url of file for which information is generating
 */
static void
format_exists_file_data           (wchar_t       *__buf,
                                   size_t         __buf_size,
                                   const wchar_t *__url)
{
  wchar_t date[100];
  vfs_stat_t stat;

  vfs_stat (__url, &stat);

  format_file_time (date, 100, stat.st_mtim.tv_sec);

#ifdef __USE_FILE_OFFSET64
  swprintf (__buf, __buf_size, _(L"date %ls, size %lld bytes"),
    date, stat.st_size);
#else
  swprintf (__buf, __buf_size, _(L"date %ls, size %ld bytes"),
    date, stat.st_size);
#endif
}

/**
 * Returns width of file exists message
 *
 * @return width of message box
 */
static int
file_exists_msg_width             (void)
{
  int res=55, dummy;

  dummy=wcslen (_(L"Overwrite this target?"))+
    widget_shortcut_length (_(L"_Yes"))+
    widget_shortcut_length (_(L"_No"))+
    widget_shortcut_length (_(L"A_ppend"))+17;
  res=MAX (res, dummy);

  dummy=wcslen (_(L"Overwrite all targets?"))+
    widget_shortcut_length (_(L"A_ll"))+
    widget_shortcut_length (_(L"_Update"))+
    widget_shortcut_length (_(L"Non_e"))+17;
  res=MAX (res, dummy);

  return res;
}

/**
 * Shows question when destination file already exists
 *
 * @param __src - URL of source file
 * @param __dst - URL of destination file
 * @return modal result of question
 */
static int
file_exists_question              (const wchar_t *__src,
                                   const wchar_t *__dst)
{
  w_window_t *wnd;
  w_text_t *text;
  w_button_t *btn;
  wchar_t buf[4096], dummy[4096];
  int buttons_left, cur_left, fn_len, res;
  static int width=0;

  if (!width)
    width=file_exists_msg_width ();

  /* Create question window */
  wnd=widget_create_window (_(L"File exists"), 0, 0, width, 13, WMS_CENTERED);
  w_window_set_fonts (wnd, &FONT (CID_WHITE,  CID_RED),
    &FONT (CID_YELLOW,  CID_RED));

  fn_len=width-wcslen (_(L"Target file \"%ls\" already exists!"));
  fit_filename (__dst, fn_len, dummy);
  EXIST_QUESTION_TEXT (1, L"Target file \"%ls\" already exists!", dummy);

  format_exists_file_data (dummy, BUF_LEN (dummy), __src);
  EXIST_QUESTION_TEXT (3, L"Source: %ls", dummy);

  format_exists_file_data (dummy, BUF_LEN (dummy), __dst);
  EXIST_QUESTION_TEXT (4, L"Target: %ls", dummy);

  EXIST_QUESTION_TEXT (6, L"Overwrite this target?");
  EXIST_QUESTION_TEXT (8, L"Overwrite all targets?");

  buttons_left=MAX (wcslen (_(L"Overwrite this target?")),
    wcslen (_(L"Overwrite all targets?")))+2;

  cur_left=buttons_left;
  EXIST_QUESTION_BUTTON (6, L"_Yes",    MR_YES);
  EXIST_QUESTION_BUTTON (6, L"_No",     MR_NO);
  EXIST_QUESTION_BUTTON (6, L"A_ppend", MR_APPEND);

  cur_left=buttons_left;
  EXIST_QUESTION_BUTTON (8, L"A_ll",    MR_ALL);
  EXIST_QUESTION_BUTTON (8, L"_Update", MR_UPDATE);
  EXIST_QUESTION_BUTTON (8, L"Non_e",   MR_MY_NONE);
  cur_left=buttons_left;
  EXIST_QUESTION_BUTTON (9, L"If _size differs", MR_SIZE_DIFFERS);

  cur_left=(wnd->position.width-widget_shortcut_length (_(L"_Abort"))-4)/2;
  EXIST_QUESTION_BUTTON (11, L"_Abort", MR_ABORT);

  res=w_window_show_modal (wnd);
  
  /* Destroy any created data */
  widget_destroy (WIDGET (wnd));
  
  return res;
}

/**
 * Creates file copy process window
 *
 * @return created window
 */
static process_window_t
create_process_window             (void)
{
  process_window_t res;
  w_container_t *cnt;

  res.window=widget_create_window (_(L"Copy"), 0, 0, 60, 10, WMS_CENTERED);
  cnt=WIDGET_CONTAINER (res.window);

  res.source=widget_create_text (cnt, L"", 1, 1);
  res.target=widget_create_text (cnt, L"", 1, 2);

  res.file_progress=widget_create_progress (cnt, 100, 10, 4, 49);

  widget_create_text (cnt, _(L"File"), 1, 4);

  return res;
}

/**
 * Compares sizes of two files
 *
 * @param __src - URL of source file
 * @param __dst - URL of destination file
 * @return zero if sizes are equal, non-zero otherwise
 */
static BOOL
is_size_differs                   (const wchar_t *__src,
                                   const wchar_t *__dst)
{
  vfs_stat_t s1, s2;

  vfs_stat (__src, &s1);
  vfs_stat (__dst, &s2);

  return s1.st_size!=s2.st_size;
}

/**
 * Compares modification times of two files
 *
 * @param __src - URL of source file
 * @param __dst - URL of destination file
 * @return zero if source is older than destination, non-zero otherwise
 */
static BOOL
is_newer                          (const wchar_t *__src,
                                   const wchar_t *__dst)
{
  vfs_stat_t s1, s2;

  vfs_stat (__src, &s1);
  vfs_stat (__dst, &s2);

  if (s1.st_mtim.tv_sec>s2.st_mtim.tv_sec)
    return TRUE;

  if (s1.st_mtim.tv_sec<s2.st_mtim.tv_sec)
    return FALSE;

  return s1.st_mtim.tv_nsec>s2.st_mtim.tv_nsec;
}

/**
 * Copies a single file
 *
 * @param __src - URL of source
 * @param __dst - URL of destination
 * @param __owr_all_rule - Rule for overwriting existing files 
 * @return zero on success, non-zero otherwise
 */
static int
copy_file                         (const wchar_t    *__src,
                                   const wchar_t    *__dst,
                                   int              *__owr_all_rule,
                                   process_window_t *__proc_wnd)
{
  vfs_file_t fd_src=0, fd_dst=0;
  int res, create_flags=O_WRONLY | O_CREAT | O_TRUNC;
  vfs_stat_t stat;
  char buffer[BUF_SIZE];
  wchar_t msg[1024], fn[1024];
  vfs_size_t remain, copied, read, written;
  struct utimbuf times;

  /* Initialize current info on screen */
  w_progress_set_pos (__proc_wnd->file_progress, 0);
  COPY_SET_FN (__src, source, L"Source");
  COPY_SET_FN (__dst, target, L"Target");

  /* Check is file copying to itself */
  if (!wcscmp (__src, __dst))
    {
      wchar_t buf[4096];
      swprintf (buf, BUF_LEN (buf),
        _(L"Cannot copy \"%ls\" to itself"), __src);

      message_box (_(L"Error"), buf, MB_OK | MB_CRITICAL);
      return -1;
    }

  /* Open descriptor of source file */
  OPEN_FD (fd_src, __src, O_RDONLY, res,
    _(L"Cannot open source file \"%ls\":\n%ls"),
    __src, vfs_get_error (res));

  /* Check is file already exists */
  if (!vfs_stat (__dst, &stat))
    {
      if (__owr_all_rule && *__owr_all_rule)
        res=*__owr_all_rule; else
        res=file_exists_question (__src, __dst);

      switch (res)
        {
          case MR_NO:
            return 0;
            break;
          case MR_APPEND:
            create_flags=O_WRONLY | O_APPEND;
            break;

          case MR_ALL:
            SAVE_OWR_ALL_RULE (MR_ALL);
            break;
          case MR_UPDATE:
            SAVE_OWR_ALL_RULE (MR_UPDATE);
            if (!is_newer (__src, __dst))
              return 0;
            break;
          case MR_MY_NONE:
            SAVE_OWR_ALL_RULE (MR_MY_NONE);
            return 0;
            break;
          case MR_SIZE_DIFFERS:
            SAVE_OWR_ALL_RULE (MR_SIZE_DIFFERS);
            if (!is_size_differs (__src, __dst))
              return 0;
            break;

          case MR_CANCEL:
          case MR_ABORT:
            return MR_ABORT;
            break;
        }
    }

  /* Stat of source file */
  COPY_FILE_REP ( res=vfs_stat (__src, &stat);, error,
        _(L"Cannot stat source file \"%ls\":\n%ls"),
        __src, vfs_get_error (res));

  remain=stat.st_size;

  /* Create destination file */
  OPEN_FD (fd_dst, __dst, create_flags, res,
    _(L"Cannot create target file \"%ls\":\n%ls"),
    __dst, vfs_get_error (res));

  /* Set mode of destination file */
  COPY_FILE_REP (res=vfs_chmod (__dst, stat.st_mode), error2,
    _(L"Cannot chmod target file \"%ls\":\n%ls"),
    __dst, vfs_get_error (res));

  copied=0;
  w_progress_set_max (__proc_wnd->file_progress, remain);

  /* Copy content of file */
  while (remain>0)
    {
      /* Read buffer from source file */
      COPY_FILE_REP ( read=vfs_read (fd_src, buffer, MIN (remain, BUF_SIZE));
            res=read<0?read:0;,
            error,
            _(L"Cannot read source file \"%ls\":\n%ls"),
            __src, vfs_get_error (res));

      /* Write buffer to destination file */
      COPY_FILE_REP ( written=vfs_write (fd_dst, buffer, read);
            res=written<0?written:0;,
            error,
            _(L"Cannot write target file \"%ls\":\n%ls"),
            __dst, vfs_get_error (res));

      copied+=read;
      w_progress_set_pos (__proc_wnd->file_progress, copied);

      remain-=read;
    }

  /* Set access and modification time of new file */
  times.actime  = stat.st_atim.tv_sec;
  times.modtime = stat.st_mtim.tv_sec;
  vfs_utime (__dst, &times);

  /* Close descriptors  */
  CLOSE_FD ();

  return 0;
}

/**
 * Recursively copies directory
 *
 * @param __src - URL of source
 * @param __dst - URL of destination
 * @param __owr_all_rule - Rule for overwriting existing files 
 * @return zero on success, non-zero otherwise
 */
static int
copy_dir                          (const wchar_t    *__src,
                                   const wchar_t    *__dst,
                                   int              *__owr_all_rule,
                                   process_window_t *__proc_wnd)
{
  vfs_dirent_t **eps=NULL;
  vfs_stat_t stat;
  int count, i, res=0;
  wchar_t *full_name, *full_dst;
  size_t fn_len, dst_len;

  /* Stat source directory */
  COPY_DIR_REP ( res=vfs_stat (__src, &stat);, error,
        _(L"Cannot stat source directory \"%ls\":\n%ls"),
        __src, vfs_get_error (res));

  /* Create destination directory */
  COPY_DIR_REP ( res=vfs_mkdir (__dst, 0); if (res==-EEXIST) res=0;, error,
        _(L"Cannot create target directory \"%ls\":\n%ls"),
        __dst, vfs_get_error (res));

  /* Set mode of destination directory */
  COPY_DIR_REP ( res=vfs_chmod (__dst, stat.st_mode);, error,
        _(L"Cannot chmod target directory \"%ls\":\n%ls"),
        __dst, vfs_get_error (res));

  /* Get listing of a directory */
  COPY_DIR_REP ( count=vfs_scandir (__src, &eps, 0, vfs_alphasort);
                 res=count<0?count:0,
                 error,
                 _(L"Cannot listing source directory \"%ls\":\n%ls"),
                 __src, vfs_get_error (res));

  /* Allocate memories for new strings */
  ALLOC_FN (full_name, fn_len, __src);
  ALLOC_FN (full_dst, dst_len, __dst);

  /* Get contents of source directory */
  for (i=0; i<count; i++)
    {
      if (wcscmp (eps[i]->name, L".") && wcscmp (eps[i]->name, L".."))
        {
          /* Get full filename of current file or directory and */
          /* it's destination */
          swprintf (full_name, fn_len, L"%ls/%ls", __src, eps[i]->name);
          swprintf (full_dst, dst_len, L"%ls/%ls", __dst, eps[i]->name);

          if (!isdir (full_name))
            {
              res=copy_file (full_name, full_dst, __owr_all_rule, __proc_wnd);
            } else {
              res=copy_dir (full_name, full_dst, __owr_all_rule, __proc_wnd);
            }

          /* Copying has been aborted */
          if (res==MR_ABORT)
            {
              int j;

              /* Free allocated memory */
              for (j=i; j<count; j++)
                vfs_free_dirent (eps[i]);

              break;
            }
        }

      vfs_free_dirent (eps[i]);
    }

  /* Free used variables */
  SAFE_FREE (eps);
  free (full_name);

  return res;
}

/********
 * User's backend
 */

/**
 * Copies file or directory
 *
 * @param __src - URL of source
 * @param __dst - URL of destination
 * @return zero on success, non-zero otherwise
 */
int
action_copy                       (const wchar_t *__src,
                                   const wchar_t *__dst)
{
  process_window_t wnd;
  wnd=create_process_window ();
  w_window_show (wnd.window);

  wchar_t *rdst=NULL; /* Real destination */
  int owr_all_rule=0;

  if (isdir (__src))
    {
      vfs_stat_t stat;

      /* Get full destination directory name */
      if (vfs_stat (__dst, &stat))
        rdst=wcsdup (__dst); else
      if (isdir (__dst))
        rdst=get_real_dst (__src, __dst); else
        {
          /*
           * TODO:
           *  Add error handling here
           */
          return -1;
        }

      /* Copy directory */
      copy_dir (__src, rdst, &owr_all_rule, &wnd);
    } else
    {
      /* Get full destination filename */
      if (isdir (__dst))
        rdst=get_real_dst (__src, __dst); else
        rdst=wcsdup (__dst);

      /* Copy single file */
      copy_file (__src, rdst, &owr_all_rule, &wnd);
    }

  SAFE_FREE (rdst);

  widget_destroy (WIDGET (wnd.window));

  return 0;
}
