/****************************************************************************
 * bsp/src/cxd56_powermgr_procfs.c
 *
 *   Copyright 2018 Sony Semiconductor Solutions Corporation
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
 * 3. Neither the name of Sony Semiconductor Solutions Corporation nor
 *    the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <sdk/config.h>
#include <nuttx/fs/procfs.h>
#include <nuttx/fs/dirent.h>
#include <nuttx/kmalloc.h>

#include <sys/stat.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <debug.h>
#include <errno.h>

#include "cxd56_clock.h"
#include "cxd56_powermgr.h"
#include "up_arch.h"
#include "chip/cxd56_crg.h"
#include "chip/cxd5602_topreg.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_CXD56_PM_DEBUG
#  define pmerr(format, ...)  _err(format, ##__VA_ARGS__)
#  define pmwarn(format, ...) _warn(format, ##__VA_ARGS__)
#  define pminfo(format, ...) _info(format, ##__VA_ARGS__)
#else
#  define pmerr(x...)
#  define pmwarn(x...)
#  define pminfo(x...)
#endif

#define PWD_STAT(val, shift) ((val >> shift) & 0x1)
#define DSP_NUM (6)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct cxd56_powermgr_procfs_file_s
{
  struct procfs_file_s base;         /* Base open file structure */
  int fileno;
  int readcnt;
};

struct cxd56_powermgr_procfs_dir_s
{
  struct procfs_dir_priv_s  base;    /* Base directory private data */
  int index;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int cxd56_powermgr_procfs_open(FAR struct file *filep,
                            FAR const char *relpath, int oflags, mode_t mode);
static int cxd56_powermgr_procfs_close(FAR struct file *filep);
static ssize_t cxd56_powermgr_procfs_read(FAR struct file *filep,
                                             FAR char *buffer, size_t buflen);
static int cxd56_powermgr_procfs_dup(FAR const struct file *oldp,
                                                       FAR struct file *newp);
static int cxd56_powermgr_procfs_opendir(FAR const char *relpath,
                                                 FAR struct fs_dirent_s *dir);
static int cxd56_powermgr_procfs_closedir(FAR struct fs_dirent_s *dir);
static int cxd56_powermgr_procfs_readdir(struct fs_dirent_s *dir);
static int cxd56_powermgr_procfs_rewinddir(struct fs_dirent_s *dir);
static int cxd56_powermgr_procfs_stat(FAR const char *relpath,
                                                        FAR struct stat *buf);

/****************************************************************************
 * Private Data
 ****************************************************************************/

const struct procfs_operations cxd56_powermgr_procfs_operations =
{
  cxd56_powermgr_procfs_open,      /* open */
  cxd56_powermgr_procfs_close,     /* close */
  cxd56_powermgr_procfs_read,      /* read */
  NULL,                            /* write */
  cxd56_powermgr_procfs_dup,       /* dup */
  cxd56_powermgr_procfs_opendir,   /* opendir */
  cxd56_powermgr_procfs_closedir,  /* closedir */
  cxd56_powermgr_procfs_readdir,   /* readdir */
  cxd56_powermgr_procfs_rewinddir, /* rewinddir */
  cxd56_powermgr_procfs_stat       /* stat */
};


static const struct procfs_entry_s g_powermgr_procfs1 =
  {"pm**", &cxd56_powermgr_procfs_operations};
static const struct procfs_entry_s g_powermgr_procfs2 =
  {"pm/" , &cxd56_powermgr_procfs_operations};

static FAR char *g_powermg_procfs_buffer;
static size_t g_powermg_procfs_size;
static size_t g_powermg_procfs_len;

static const char* g_powermg_procfs_clock_source_name[]
                   = {"RCOSC", "SYSPLL", "XOSC", "RTC"};
static const char* g_powermg_procfs_power_state[]
                   = {"-", "o"};
static const char* g_powermg_procfs_dir[]
                   = {"clock", "power"};

/****************************************************************************
 * Private Function
 ****************************************************************************/

/****************************************************************************
 * Name: cxd56_powermgr_procfs_step_buffer
 *
 * Description:
 *   step buffer position
 *
 ****************************************************************************/

static void cxd56_powermgr_procfs_step_buffer(size_t len)
{
  DEBUGASSERT(g_powermg_procfs_size > (g_powermg_procfs_len + len));
  g_powermg_procfs_len = g_powermg_procfs_len + len;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_clock_base
 *
 * Description:
 *   collect clock base value
 *
 ****************************************************************************/

static void cxd56_powermgr_procfs_clock_base(void)
{
  uint32_t sys_clk_sel;
  uint32_t app_clk_sel;
  size_t   len;

  /* Collect clock base value */

  sys_clk_sel = getreg32(CXD56_TOPREG_CKSEL_ROOT) >> 22 & 0x3;
  app_clk_sel = getreg32(CXD56_TOPREG_APP_CKSEL) >> 8 & 0x3;

  /* Store data in buffer */

  len = snprintf(g_powermg_procfs_buffer + g_powermg_procfs_len,
                 g_powermg_procfs_size - g_powermg_procfs_len,
                  "Clock Source\n"
                  " SYS:%s\n"
                  " APP:%s\n\n",
                 g_powermg_procfs_clock_source_name[sys_clk_sel],
                 g_powermg_procfs_clock_source_name[app_clk_sel]);

  /* Step buffer position */

  cxd56_powermgr_procfs_step_buffer(len);
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_clock
 *
 * Description:
 *   collect clock value
 *
 ****************************************************************************/

static void cxd56_powermgr_procfs_clock(void)
{
  size_t len;
  uint32_t scu, hpadc, lpadc, gps, gpsahb, val, emmc;
  int dsp;
  uint32_t dsptabl[DSP_NUM];

  /* collect clock value
   * and store data in buffer
   */

  /* Check SCU clock status */

  scu   = cxd56_get_scu_baseclock();
  hpadc = cxd56_get_hpadc_baseclock();
  lpadc = cxd56_get_lpadc_baseclock();
  val   = getreg32(CXD56_TOPREG_SYSIOP_CKEN);
  if ((val & CKEN_BRG_SCU) != CKEN_BRG_SCU)
    {
      scu   = 0;
      hpadc = 0;
      lpadc = 0;
    }

  /* Check GPS clock status */

  gps    = cxd56_get_gps_cpu_baseclock();
  gpsahb = cxd56_get_gps_ahb_baseclock();
  val    = getreg32(CXD56_TOPREG_GNSDSP_CKEN);
  if (((val & GNSDSP_CKEN_P1) != GNSDSP_CKEN_P1)
        && ((val & GNSDSP_CKEN_COP) != GNSDSP_CKEN_COP))
    {
          gps    = 0;
          gpsahb = 0;
    }

  /* Check DSP clock status */

  val = getreg32(CXD56_CRG_CK_GATE_AHB);
  val = (val >> 16) & 0x3f;
  for (dsp=0; dsp<DSP_NUM; dsp++)
    {
      if ((val & (1 << dsp)) == (1 << dsp))
        {
          dsptabl[dsp] = cxd56_get_cpu_baseclk();
        }
      else
        {
          dsptabl[dsp] = 0;
        }
    }

  val = getreg32(CXD56_CRG_CKEN_EMMC);
  val = val & 0x7;
  if (val == 0x7)
    {
      emmc = cxd56_get_cpu_baseclk();
    }
  else
    {
      emmc = 0;
    }

  len = snprintf(g_powermg_procfs_buffer + g_powermg_procfs_len,
                 g_powermg_procfs_size - g_powermg_procfs_len,
                  "Clock Status [Hz]\n"
                  " |-RTC        : %9d""   |-APP        : %9d\n"
                  " |-RCOSC      : %9d""   ||-DSP0      : %9d\n"
                  " |-XOSC       : %9d""   ||-DSP1      : %9d\n"
                  " |-SYSPLL     : %9d""   ||-DSP2      : %9d\n"
                  " |-M0P        : %9d""   ||-DSP3      : %9d\n"
                  " ||-AHB       : %9d""   ||-DSP4      : %9d\n"
                  " | |-APB      : %9d""   ||-DSP5      : %9d\n"
                  " |-UART1      : %9d""   ||-UART2     : %9d\n"
                  " |-SFC        : %9d""   ||-SPI4      : %9d\n"
                  " |-SCU        : %9d""   ||-SPI5      : %9d\n"
                  " ||-LPADC     : %9d""   ||-USB       : %9d\n"
                  " ||-HPADC     : %9d""   ||-EMMC      : %9d\n"
                  " |-I2C4       : %9d""   ||-SDIO      : %9d\n"
                  " |-GPS        : %9d""   ||-VSYNC     : %9d\n"
                  " ||-AHB       : %9d\n",
                 cxd56_get_rtc_clock(), cxd56_get_appsmp_baseclock(),
                 cxd56_get_rcosc_clock(), dsptabl[0],
                 cxd56_get_xosc_clock(), dsptabl[1],
                 cxd56_get_syspll_clock(), dsptabl[2],
                 cxd56_get_sys_baseclock(), dsptabl[3],
                 cxd56_get_sys_ahb_baseclock(), dsptabl[4],
                 cxd56_get_sys_apb_baseclock(), dsptabl[5],
                 cxd56_get_com_baseclock(), cxd56_get_img_uart_baseclock(),
                 cxd56_get_sys_sfc_baseclock(), cxd56_get_img_spi_baseclock(),
                 scu, cxd56_get_img_wspi_baseclock(),
                 lpadc, cxd56_get_usb_baseclock(),
                 hpadc, emmc,
                 cxd56_get_pmui2c_baseclock(), cxd56_get_sdio_baseclock(),
                 gps, cxd56_get_img_vsync_baseclock(),
                 gpsahb);

  /* Step buffer position */

  cxd56_powermgr_procfs_step_buffer(len);
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_power_state
 *
 * Description:
 *   collect clock power state
 *
 ****************************************************************************/

static void cxd56_powermgr_procfs_power_state(void)
{
  uint32_t reg_pwd_stat, reg_ana_pw_stat, rf;
  size_t len;

  /* Collect power status */

  reg_pwd_stat    = getreg32(CXD56_TOPREG_PWD_STAT);
  reg_ana_pw_stat = getreg32(CXD56_TOPREG_ANA_PW_STAT);

  /* Check RF_xxx power status */

  rf = reg_ana_pw_stat & 0x3F0;
  if (rf != 0)
    {
      rf = 1;
    }

  /* Store data in buffer */

  len = snprintf(g_powermg_procfs_buffer + g_powermg_procfs_len,
                 g_powermg_procfs_size - g_powermg_procfs_len,
                  "Power Status\n"
                  "|-RCOSC        : %s\n"
                  "|-XOSC         : %s\n"
                  "|-SYSPLL       : %s\n"
                  "|-SCU          : %s\n"
                  "||-LPADC       : %s\n"
                  "||-HPADC       : %s\n"
                  "|-CORE         : %s\n"
                  "||-GNSS_ITP    : %s\n"
                  "||-SYSIOP      : %s\n"
                  "| |-SYSIOP_SUB : %s\n"
                  "| |-GNSS       : %s\n"
                  "| ||-RF        : %s\n"
                  "| |-APP        : %s\n"
                  "| ||-APP_DSP   : %s\n"
                  "| ||-APP_SUB   : %s\n"
                  "| ||-APP_AUD   : %s\n",
                 g_powermg_procfs_power_state[PWD_STAT(reg_ana_pw_stat, 0)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_ana_pw_stat, 1)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_ana_pw_stat, 2)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 0)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_ana_pw_stat, 13)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_ana_pw_stat, 12)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 4)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 12)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 5)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 6)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 13)],
                 g_powermg_procfs_power_state[rf],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 8)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 9)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 10)],
                 g_powermg_procfs_power_state[PWD_STAT(reg_pwd_stat, 14)]);

  /* step buffer position */
  cxd56_powermgr_procfs_step_buffer(len);
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_check_path
 *
 * Description:
 *   Check path power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_check_dir(char *relpath,
                                        mode_t *mode, int *level, int *fileno)
{
  char *temp;
  int ret = OK;

  *level = 0;
  *mode = S_IFDIR;
  *fileno = 0;
  temp = strtok(relpath, "/");
  if (strncmp(temp, "pm", 3) == 0)
    {
      while((temp = strtok(NULL, "/")) != NULL)
        {
          *level = *level + 1;
          if (*level == 1)
            {
              if (strncmp(temp, g_powermg_procfs_dir[0],
                    strlen(g_powermg_procfs_dir[0])+1) == 0)
                {
                  *mode = S_IFREG;
                  *fileno = 1;
                }
              else if (strncmp(temp, g_powermg_procfs_dir[1],
                         strlen(g_powermg_procfs_dir[1])+1) == 0)
                {
                  *mode = S_IFREG;
                  *fileno = 2;
                }
              else
                {
                  ret = -ENOENT;
                  break;
                }
            }
          else
            {
              ret = -ENOENT;
              break;
            }
        }
    }
  else
    {
      ret = -ENOENT;
    }

  return ret;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_open
 *
 * Description:
 *   Request Open power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_open(FAR struct file *filep,
                             FAR const char *relpath, int oflags, mode_t mode)
{
  FAR struct cxd56_powermgr_procfs_file_s *priv;
  int ret;
  mode_t getmode;
  int level;
  int fileno;
  char temp[16];

  pminfo("Open '%s'\n", relpath);

  /* PROCFS is read-only.  Any attempt to open with any kind of write
   * access is not permitted.
   *
   * REVISIT:  Write-able proc files could be quite useful.
   */

  if (((oflags & O_WRONLY) != 0 || (oflags & O_RDONLY) == 0))
    {
      pmerr("ERROR: Only O_RDONLY supported\n");
      return -EACCES;
    }

  memset(temp, 0, sizeof(temp));
  snprintf(temp, sizeof(temp)-1, "%s", relpath);
  ret = cxd56_powermgr_procfs_check_dir(temp, &getmode, &level, &fileno);

  if (ret != OK)
    {
      return ret;
    }

  /* Allocate the open file structure */

  priv = (FAR struct cxd56_powermgr_procfs_file_s *)
            kmm_zalloc(sizeof(struct cxd56_powermgr_procfs_file_s));
  if (!priv)
    {
      pmerr("ERROR: Failed to allocate file attributes\n");
      return -ENOMEM;
    }

  priv->fileno  = fileno;
  priv->readcnt = 0;
  filep->f_priv = (void *)priv;

  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_close
 *
 * Description:
 *   Request Close power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_close(FAR struct file *filep)
{
  pminfo("Close\n");

  DEBUGASSERT(filep->f_priv);
  kmm_free(filep->f_priv);
  filep->f_priv = NULL;

  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_read
 *
 * Description:
 *   Request Read power manager procfs
 *
 ****************************************************************************/

static ssize_t cxd56_powermgr_procfs_read(FAR struct file *filep,
                                          FAR char *buffer, size_t buflen)
{
  size_t    len;
  FAR struct cxd56_powermgr_procfs_file_s *priv;

  pminfo("READ buffer=%p buflen=%lu len=%lu\n", buffer,
         (unsigned long)buflen, g_powermg_procfs_len);

  DEBUGASSERT(filep && filep->f_priv);

  priv = (FAR struct cxd56_powermgr_procfs_file_s *)filep->f_priv;

  if (priv->fileno == 0)
    {
      /* pm directory */

      return 0;
    }

  if (priv->readcnt == 0)
    {
      /* Collect data and store data in buffer */

      g_powermg_procfs_buffer = buffer;
      g_powermg_procfs_size   = buflen;
      g_powermg_procfs_len    = 0;

      if (priv->fileno == 1)
        {
          cxd56_powermgr_procfs_clock_base();
          cxd56_powermgr_procfs_clock();
        }
      else
        {
          cxd56_powermgr_procfs_power_state();
        }

      len = g_powermg_procfs_len;
      priv->readcnt++;
    }
  else
    {
      /* Indicate already provided all data */

      len = 0;
      priv->readcnt = 0;
    }

  return len;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_dup
 *
 * Description:
 *   Request Dup power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_dup(FAR const struct file *oldp,
                                                        FAR struct file *newp)
{
  void *oldpriv;
  void *newpriv;

  pminfo("Dup %p->%p\n", oldp, newp);

  /* Recover our private data from the old struct file instance */

  oldpriv = oldp->f_priv;
  DEBUGASSERT(oldpriv);

  /* Allocate a new container to hold the task and attribute selection */

  newpriv = kmm_malloc(sizeof(struct cxd56_powermgr_procfs_file_s));
  if (!newpriv)
    {
      pmerr("ERROR: Failed to allocate file attributes\n");
      return -ENOMEM;
    }

  /* The copy the file attributes from the old attributes to the new */

  memcpy(newpriv, oldpriv, sizeof(struct cxd56_powermgr_procfs_file_s));

  /* Save the new attributes in the new file structure */

  newp->f_priv = newpriv;

  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_stat
 *
 * Description:
 *   Request Stat power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_stat(FAR const char *relpath,
                                                         FAR struct stat *buf)
{
  int ret;
  mode_t mode;
  int level;
  int fileno;
  char temp[16];

  memset(temp, 0, sizeof(temp));
  snprintf(temp, sizeof(temp)-1, "%s", relpath);
  ret = cxd56_powermgr_procfs_check_dir(temp, &mode, &level, &fileno);

  pminfo("Stat %s %d %d %d\n", relpath, mode, level, ret);

  buf->st_mode    = mode | S_IROTH | S_IRGRP | S_IRUSR;
  buf->st_size    = 0;
  buf->st_blksize = 0;
  buf->st_blocks  = 0;

  return ret;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_opendir
 *
 * Description:
 *   Request Opendir power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_opendir(FAR const char *relpath,
                                                 FAR struct fs_dirent_s *dir)
{
  FAR struct cxd56_powermgr_procfs_dir_s *procfs;
  int ret;
  mode_t mode;
  int level;
  int fileno;
  char temp[16];

  memset(temp, 0, sizeof(temp));
  snprintf(temp, sizeof(temp)-1, "%s", relpath);
  ret = cxd56_powermgr_procfs_check_dir(temp, &mode, &level, &fileno);

  pminfo("Opendir '%s' %d %d\n", relpath, ret, level);

  if (ret != OK)
    {
      return ret;
    }
  if (level > 0)
    {
      return -ENOENT;
    }

  procfs = (FAR struct cxd56_powermgr_procfs_dir_s *)
     kmm_malloc(sizeof(struct cxd56_powermgr_procfs_dir_s));
  if (!procfs)
    {
      pmerr("ERROR: Failed to allocate dir attributes\n");
      return -ENOMEM;
    }

  procfs->index = 0;
  dir->u.procfs = (FAR void *)procfs;

  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_closedir
 *
 * Description:
 *   Request Closedir power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_closedir(FAR struct fs_dirent_s *dir)
{
  FAR struct smartfs_level1_s *priv;

  pminfo("Closedir\n");

  DEBUGASSERT(dir && dir->u.procfs);
  priv = dir->u.procfs;

  if (priv)
    {
      kmm_free(priv);
    }

  dir->u.procfs = NULL;
  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_readdir
 *
 * Description:
 *   Request Readdir power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_readdir(struct fs_dirent_s *dir)
{
  FAR struct cxd56_powermgr_procfs_dir_s *procfs;

  DEBUGASSERT(dir && dir->u.procfs);

  procfs = (FAR struct cxd56_powermgr_procfs_dir_s *)dir->u.procfs;

  pminfo("Readdir %d\n", procfs->index);

  if (procfs->index > ((sizeof(g_powermg_procfs_dir)
                        / sizeof(g_powermg_procfs_dir[0])) - 1))
    {
      return -ENOENT;
    }

  dir->fd_dir.d_type = DTYPE_FILE;
  strncpy(dir->fd_dir.d_name, g_powermg_procfs_dir[procfs->index],
            strlen(g_powermg_procfs_dir[procfs->index])+1);
  procfs->index++;

  return OK;
}

/****************************************************************************
 * Name: cxd56_powermgr_procfs_rewinddir
 *
 * Description:
 *   Request Rewind power manager procfs
 *
 ****************************************************************************/

static int cxd56_powermgr_procfs_rewinddir(struct fs_dirent_s *dir)
{
  FAR struct cxd56_powermgr_procfs_dir_s *procfs;

  DEBUGASSERT(dir && dir->u.procfs);

  procfs = (FAR struct cxd56_powermgr_procfs_dir_s *)dir->u.procfs;
  pminfo("Rewind %d\n", procfs->index);
  procfs->index = 0;

  return OK;
}

/****************************************************************************
 * Public Function
 ****************************************************************************/

/****************************************************************************
 * Name: cxd56_pm_initialize_procfs
 *
 * Description:
 *   Initialize power manager procfs
 *
 ****************************************************************************/

void cxd56_pm_initialize_procfs(void)
{
  procfs_register(&g_powermgr_procfs1);
  procfs_register(&g_powermgr_procfs2);
}
