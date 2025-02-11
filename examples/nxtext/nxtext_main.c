/****************************************************************************
 * examples/nxtext/nxtext_main.c
 *
 *   Copyright (C) 2011-2012, 2015-2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <sys/types.h>
#include <sys/boardctl.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>

#ifdef CONFIG_NX_LCDDRIVER
#  include <nuttx/lcd/lcd.h>
#else
#  include <nuttx/video/fb.h>
#  ifdef CONFIG_VNCSERVER
#    include <nuttx/video/vnc.h>
#  endif
#endif

#include <nuttx/nx/nx.h>
#include <nuttx/nx/nxglib.h>
#include <nuttx/nx/nxfonts.h>

#include "nxtext_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/* If not specified, assume that the hardware supports one video plane */

#ifndef CONFIG_EXAMPLES_NXTEXT_VPLANE
#  define CONFIG_EXAMPLES_NXTEXT_VPLANE 0
#endif

/* If not specified, assume that the hardware supports one LCD device */

#ifndef CONFIG_EXAMPLES_NXTEXT_DEVNO
#  define CONFIG_EXAMPLES_NXTEXT_DEVNO 0
#endif

#define BGMSG_LINES 24

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_NX_KBD
static const uint8_t g_pumsg[] = "Pop-Up!";
#endif

static const char *g_bgmsg[BGMSG_LINES] =
{
  "\nJULIET\n",                           /* Line 1 */
  "Wilt thou be gone?\n",                 /* Line 2 */
  "  It is not yet near day:\n",          /* Line 3 */
  "It was the nightingale,\n",            /* Line 4 */
  "  and not the lark,\n",                /* Line 5 */
  "That pierced the fearful hollow\n",    /* Line 6 */
  "  of thine ear;\n",                    /* Line 7 */
  "Nightly she sings\n",                  /* Line 8 */
  "  on yon pomegranate-tree:\n",         /* Line 9 */
  "Believe me, love,\n",                  /* Line 10 */
  "  it was the nightingale.\n",          /* Line 11 */
  "\nROMEO\n",                            /* Line 12 */
  "It was the lark,\n",                   /* Line 13 */
  "  the herald of the morn,\n",          /* Line 14 */
  "No nightingale:\n",                    /* Line 15 */
  "  look, love, what envious streaks\n", /* Line 16 */
  "Do lace the severing clouds\n",        /* Line 17 */
  "  in yonder east:\n",                  /* Line 18 */
  "Night's candles are burnt out,\n",     /* Line 19 */
  "  and jocund day\n",                   /* Line 20 */
  "Stands tiptoe\n",                      /* Line 21 */
  "  on the misty mountain tops.\n",      /* Line 22 */
  "I must be gone and live,\n",           /* Line 23 */
  "  or stay and die.\n"                  /* Line 24 */
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* The connecton handler */

NXHANDLE g_hnx = NULL;

/* The font handles */

NXHANDLE g_bghfont = NULL;
NXHANDLE g_puhfont = NULL;

/* The screen resolution */

nxgl_coord_t g_xres;
nxgl_coord_t g_yres;

bool b_haveresolution = false;
#ifdef CONFIG_NX_MULTIUSER
bool g_connected = false;
#endif
sem_t g_semevent = {0};

int g_exitcode = NXEXIT_SUCCESS;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxtext_suinitialize
 ****************************************************************************/

#ifndef CONFIG_NX_MULTIUSER
static inline int nxtext_suinitialize(void)
{
  FAR NX_DRIVERTYPE *dev;
  int ret;

#if defined(CONFIG_EXAMPLES_NXTEXT_EXTERNINIT)
  struct boardioc_graphics_s devinfo;

  /* Use external graphics driver initialization */

  printf("nxtext_initialize: Initializing external graphics device\n");

  devinfo.devno = CONFIG_EXAMPLES_NXTEXT_DEVNO;
  devinfo.dev = NULL;

  ret = boardctl(BOARDIOC_GRAPHICS_SETUP, (uintptr_t)&devinfo);
  if (ret < 0)
    {
      printf("nxtext_initialize: boardctl failed, devno=%d: %d\n",
             CONFIG_EXAMPLES_NXTEXT_DEVNO, errno);
      g_exitcode = NXEXIT_EXTINITIALIZE;
      return ERROR;
    }

  dev = devinfo.dev;

#elif defined(CONFIG_NX_LCDDRIVER)
  /* Initialize the LCD device */

  printf("nxtext_initialize: Initializing LCD\n");
  ret = board_lcd_initialize();
  if (ret < 0)
    {
      printf("nxtext_initialize: board_lcd_initialize failed: %d\n", -ret);
      g_exitcode = NXEXIT_LCDINITIALIZE;
      return ERROR;
    }

  /* Get the device instance */

  dev = board_lcd_getdev(CONFIG_EXAMPLES_NXTEXT_DEVNO);
  if (!dev)
    {
      printf("nxtext_initialize: board_lcd_getdev failed, devno=%d\n",
             CONFIG_EXAMPLES_NXTEXT_DEVNO);
      g_exitcode = NXEXIT_LCDGETDEV;
      return ERROR;
    }

  /* Turn the LCD on at 75% power */

  (void)dev->setpower(dev, ((3*CONFIG_LCD_MAXPOWER + 3)/4));

#else
  /* Initialize the frame buffer device */

  printf("nxtext_initialize: Initializing framebuffer\n");

  ret = up_fbinitialize(0);
  if (ret < 0)
    {
      printf("nxtext_initialize: up_fbinitialize failed: %d\n", -ret);

      g_exitcode = NXEXIT_FBINITIALIZE;
      return ERROR;
    }

  dev = up_fbgetvplane(0, CONFIG_EXAMPLES_NXTEXT_VPLANE);
  if (!dev)
    {
      printf("nxtext_initialize: up_fbgetvplane failed, vplane=%d\n",
             CONFIG_EXAMPLES_NXTEXT_VPLANE);

      g_exitcode = NXEXIT_FBGETVPLANE;
      return ERROR;
    }
#endif

  /* Then open NX */

  printf("nxtext_initialize: Open NX\n");

  g_hnx = nx_open(dev);
  if (!g_hnx)
    {
      printf("nxtext_initialize: nx_open failed: %d\n", errno);

      g_exitcode = NXEXIT_NXOPEN;
      return ERROR;
    }

#ifdef CONFIG_VNCSERVER
  /* Setup the VNC server to support keyboard/mouse inputs */

  ret = vnc_default_fbinitialize(0, g_hnx);
  if (ret < 0)
    {
      printf("vnc_default_fbinitialize failed: %d\n", ret);

      nx_close(g_hnx);
      g_exitcode = NXEXIT_FBINITIALIZE;
      return ERROR;
    }
#endif

  return OK;
}
#endif

/****************************************************************************
 * Name: nxtext_initialize
 ****************************************************************************/

#ifdef CONFIG_NX_MULTIUSER
static inline int nxtext_muinitialize(void)
{
  struct sched_param param;
  pthread_t thread;
  int ret;

  /* Set the client task priority */

  param.sched_priority = CONFIG_EXAMPLES_NXTEXT_CLIENTPRIO;
  ret = sched_setparam(0, &param);
  if (ret < 0)
    {
      printf("nxtext_initialize: sched_setparam failed: %d\n" , ret);
      g_exitcode = NXEXIT_SCHEDSETPARAM;
      return ERROR;
    }

  /* Start the NX server kernel thread */

  printf("nxtext_initialize: Starting NX server\n");
  ret = boardctl(BOARDIOC_NX_START, 0);
  if (ret < 0)
    {
      printf("nxtext_initialize: Failed to start the NX server: %d\n", errno);
      g_exitcode = NXEXIT_SERVERSTART;
      return ERROR;
    }

  /* Connect to the server */

  g_hnx = nx_connect();
  if (g_hnx)
    {
      pthread_attr_t attr;

#ifdef CONFIG_VNCSERVER
      /* Setup the VNC server to support keyboard/mouse inputs */

      ret = vnc_default_fbinitialize(0, g_hnx);
      if (ret < 0)
        {
          printf("vnc_default_fbinitialize failed: %d\n", ret);

          g_exitcode = NXEXIT_FBINITIALIZE;
          return ERROR;
        }
#endif
      /* Start a separate thread to listen for server events.  This is probably
       * the least efficient way to do this, but it makes this example flow more
       * smoothly.
       */

      (void)pthread_attr_init(&attr);
      param.sched_priority = CONFIG_EXAMPLES_NXTEXT_LISTENERPRIO;
      (void)pthread_attr_setschedparam(&attr, &param);
      (void)pthread_attr_setstacksize(&attr, CONFIG_EXAMPLES_NXTEXT_STACKSIZE);

      ret = pthread_create(&thread, &attr, nxtext_listener, NULL);
      if (ret != 0)
        {
           printf("nxtext_initialize: pthread_create failed: %d\n", ret);

           g_exitcode = NXEXIT_PTHREADCREATE;
           return ERROR;
        }

      /* Don't return until we are connected to the server */

      while (!g_connected)
        {
          /* Wait for the listener thread to wake us up when we really
           * are connected.
           */

          (void)sem_wait(&g_semevent);
        }
    }
  else
    {
      printf("nxtext_initialize: nx_connect failed: %d\n", errno);

      g_exitcode = NXEXIT_NXCONNECT;
      return ERROR;
    }

  return OK;
}
#endif

/****************************************************************************
 * Name: nxtext_initialize
 ****************************************************************************/

static int nxtext_initialize(void)
{
#ifdef CONFIG_NX_MULTIUSER
  return nxtext_muinitialize();
#else
  return nxtext_suinitialize();
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxtext_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int nxtext_main(int argc, char **argv)
#endif
{
  FAR struct nxtext_state_s *bgstate;
  NXWINDOW hwnd = NULL;
  nxgl_mxpixel_t color;
  int popcnt;
  int bkgndx;
  int ret;

  /* Initialize NX */

  ret = nxtext_initialize();
  printf("nxtext_main: NX handle=%p\n", g_hnx);
  if (!g_hnx || ret < 0)
    {
      printf("nxtext_main: Failed to get NX handle: %d\n", errno);
      g_exitcode = NXEXIT_NXOPEN;
      goto errout;
    }

  /* Get the configured font handles */

  g_bghfont = nxf_getfonthandle(CONFIG_EXAMPLES_NXTEXT_BGFONTID);
  if (!g_bghfont)
    {
      printf("nxtext_main: Failed to get background font handle: %d\n", errno);
      g_exitcode = NXEXIT_FONTOPEN;
      goto errout;
    }

  g_puhfont = nxf_getfonthandle(CONFIG_EXAMPLES_NXTEXT_PUFONTID);
  if (!g_puhfont)
    {
      printf("nxtext_main: Failed to get pop-up font handle: %d\n", errno);
      g_exitcode = NXEXIT_FONTOPEN;
      goto errout;
    }

  /* Set the background to the configured background color */

  printf("nxtext_main: Set background color=%d\n", CONFIG_EXAMPLES_NXTEXT_BGCOLOR);
  color = CONFIG_EXAMPLES_NXTEXT_BGCOLOR;
  ret = nx_setbgcolor(g_hnx, &color);
  if (ret < 0)
    {
      printf("nxtext_main: nx_setbgcolor failed: %d\n", errno);
      g_exitcode = NXEXIT_NXSETBGCOLOR;
      goto errout_with_nx;
    }

  /* Get the background window */

  bgstate = nxbg_getstate();
  ret = nx_requestbkgd(g_hnx, &g_nxtextcb, bgstate);
  if (ret < 0)
    {
      printf("nxtext_main: nx_requestbkgd failed: %d\n", errno);
      g_exitcode = NXEXIT_NXREQUESTBKGD;
      goto errout_with_nx;
    }

  /* Wait until we have the screen resolution.  We'll have this immediately
   * unless we are dealing with the NX server.
   */

  while (!b_haveresolution)
    {
      (void)sem_wait(&g_semevent);
    }
  printf("nxtext_main: Screen resolution (%d,%d)\n", g_xres, g_yres);

  /* Now loop, adding text to the background and periodically presenting
   * a pop-up window.
   */

  popcnt = 0;
  bkgndx = 0;
  for (;;)
    {
      /* Sleep for one second */

      sleep(1);
      popcnt++;

      /* Each three seconds, create a pop-up window.  Destroy the pop-up
       * window after two more seconds.
       */

      if (popcnt == 3)
        {
          /* Create a pop-up window */

          hwnd = nxpu_open();

#ifdef CONFIG_NX_KBD
          /* Give keyboard input to the top window (which should be the pop-up) */

          printf("nxtext_main: Send keyboard input: %s\n", g_pumsg);
          ret = nx_kbdin(g_hnx, strlen((FAR const char *)g_pumsg), g_pumsg);
          if (ret < 0)
           {
             printf("nxtext_main: nx_kbdin failed: %d\n", errno);
             goto errout_with_hwnd;
           }
#endif
        }
      else if (popcnt == 5)
        {
          /* Destroy the pop-up window and restart the sequence */

          printf("nxtext_main: Close pop-up\n");
          (void)nxpu_close(hwnd);
          popcnt = 0;
        }

      /* Give another line of text to the background window.  Force this
       * text to go the background by calling the background window interfaces
       * directly.
       */

      nxbg_write(g_bgwnd, (FAR const uint8_t *)g_bgmsg[bkgndx], strlen(g_bgmsg[bkgndx]));
      if (++bkgndx >= BGMSG_LINES)
        {
          bkgndx = 0;
        }
    }

  /* Close the pop-up window */

#ifdef CONFIG_NX_KBD
errout_with_hwnd:
#endif
  if (popcnt >= 3)
    {
      printf("nxtext_main: Close pop-up\n");
     (void)nxpu_close(hwnd);
    }

//errout_with_bkgd:
  (void)nx_releasebkgd(g_bgwnd);

errout_with_nx:
#ifdef CONFIG_NX_MULTIUSER
  /* Disconnect from the server */

  printf("nxtext_main: Disconnect from the server\n");
  nx_disconnect(g_hnx);
#else
  /* Close the server */

  printf("nxtext_main: Close NX\n");
  nx_close(g_hnx);
#endif
errout:
  return g_exitcode;
}
