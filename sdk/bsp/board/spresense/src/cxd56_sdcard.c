/****************************************************************************
 * bsp/board/spresense/src/cxd56_sdcard.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <nuttx/wqueue.h>

#include "chip.h"
#include "up_arch.h"

#include <arch/board/board.h>
#include <arch/chip/pin.h>
#include <arch/chip/pm.h>
#include "cxd56_gpio.h"
#include "cxd56_pinconfig.h"
#include "cxd56_sdhci.h"
#include "chip/cxd5602_topreg.h"

#ifdef CONFIG_MMCSD_HAVECARDDETECT
#  include "cxd56_gpioint.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* TXS02612RTWR: SDIO port expander with voltage level translation */

#define SDCARD_TXS02612_SEL PIN_AP_CLK

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct cxd56_sdhci_state_s
{
  struct sdio_dev_s *sdhci;   /* R/W device handle */
  bool initialized;           /* TRUE: SDHCI block driver is initialized */
  bool inserted;              /* TRUE: card is inserted */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct cxd56_sdhci_state_s g_sdhci;
#ifdef CONFIG_MMCSD_HAVECARDDETECT
static struct work_s g_sdcard_work;
#endif

static struct pm_cpu_freqlock_s g_hv_lock =
  PM_CPUFREQLOCK_INIT(PM_CPUFREQLOCK_TAG('S','D',0), PM_CPUFREQLOCK_FLAG_HV);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_sdcard_enable
 *
 * Description:
 *   Enable SD Card on the board.
 *
 ****************************************************************************/

static void board_sdcard_enable(FAR void *arg)
{
  struct stat stat_sdio;
  int ret = OK;

  /* Acquire frequency lock */

  up_pm_acquire_freqlock(&g_hv_lock);

  if(!g_sdhci.initialized)
    {
      /* Mount the SDHC-based MMC/SD block driver */
      /* This should be used with 3.3V */
      /* First, get an instance of the SDHC interface */

      finfo("Initializing SDHC slot 0\n");

      g_sdhci.sdhci = cxd56_sdhci_initialize(0);
      if (!g_sdhci.sdhci)
        {
          _err("ERROR: Failed to initialize SDHC slot 0\n");
          goto release_frequency_lock;
        }

      /* If not initialize SD slot */

      if (!stat("/dev/mmcsd0", &stat_sdio) == 0)
        {
          /* Now bind the SDHC interface to the MMC/SD driver */

          finfo("Bind SDHC to the MMC/SD driver, minor=0\n");

          ret = mmcsd_slotinitialize(0, g_sdhci.sdhci);
          if (ret != OK)
            {
              _err("ERROR: Failed to bind SDHC to the MMC/SD driver: %d\n", ret);
              goto release_frequency_lock;
            }

          finfo("Successfully bound SDHC to the MMC/SD driver\n");
        }

      /* Handle the initial card state */

      cxd56_sdhci_mediachange(g_sdhci.sdhci);

      if (stat("/dev/mmcsd0", &stat_sdio) == 0)
        {
          if (S_ISBLK(stat_sdio.st_mode))
            {
              ret = mount("/dev/mmcsd0", "/mnt/sd0", "vfat", 0, NULL);
              if (ret == 0)
                {
                  finfo("Successfully mount a SDCARD via the MMC/SD driver\n");
                }
              else
                {
                  _err("ERROR: Failed to mount the SDCARD. %d\n", errno);
                }
            }
        }

      g_sdhci.initialized = true;
    }

release_frequency_lock:
    /* Release frequency lock */

    up_pm_release_freqlock(&g_hv_lock);
}

/****************************************************************************
 * Name: board_sdcard_disable
 *
 * Description:
 *   Disable SD Card on the board.
 *
 ****************************************************************************/

static void board_sdcard_disable(FAR void *arg)
{
  int ret;

  if(g_sdhci.initialized)
    {
      /* un-mount */

      ret = umount("/mnt/sd0");
      if (ret < 0)
        {
          ferr("ERROR: Failed to unmount the SD Card: %d\n", errno);
        }

      /* Report the new state to the SDIO driver */

      cxd56_sdhci_mediachange(g_sdhci.sdhci);

      cxd56_sdhci_finalize(0);

      g_sdhci.initialized = false;
    }
}

#ifdef CONFIG_MMCSD_HAVECARDDETECT
/****************************************************************************
 * Name: board_sdcard_inserted
 *
 * Description:
 *   Check if a card is inserted into the selected SDHCI slot
 *
 ****************************************************************************/

static bool board_sdcard_inserted(int slotno)
{
  bool removed;

  /* Get the state of the GPIO pin */

  removed = cxd56_gpio_read(PIN_SDIO_CD);
  finfo("Slot %d inserted: %s\n", slotno, removed ? "NO" : "YES");

  return !removed;
}

/****************************************************************************
 * Name: board_sdcard_detect_int
 *
 * Description:
 *   Card detect interrupt handler
 *
 * TODO: Any way to automatically moun/unmount filesystem based on card
 * detect status?  Yes... send a message or signal to an application.
 *
 ****************************************************************************/

static int board_sdcard_detect_int(int irq, FAR void *context, FAR void *arg)
{
  bool inserted;

  /* Get the state of the GPIO pin */

  inserted = board_sdcard_inserted(0);

  /* Has the card detect state changed? */

  if (inserted != g_sdhci.inserted)
    {
      /* Yes... remember that new state and inform the SDHCI driver */

      g_sdhci.inserted = inserted;

      if (inserted)
        {
          /* Card Detect = Present, Write Protect = disable */

          putreg32(0, CXD56_TOPREG_IOFIX_APP);
        }
      else
        {
          /* Card Detect = Not present, Write Protect = disable */

          putreg32(1, CXD56_TOPREG_IOFIX_APP);
        }

      /* Check context */

      if (up_interrupt_context())
        {
          DEBUGASSERT(work_available(&g_sdcard_work));
          if (inserted)
            {
              work_queue(HPWORK, &g_sdcard_work, board_sdcard_enable, NULL, 0);
            }
          else
            {
              work_queue(HPWORK, &g_sdcard_work, board_sdcard_disable, NULL, 0);
            }
        }
      else
        {
          if (inserted)
            {
              board_sdcard_enable(NULL);
            }
          else
            {
              board_sdcard_disable(NULL);
            }
        }

      /* Re-configure Interrupt pin */

      cxd56_gpioint_config(PIN_SDIO_CD,
                           inserted ?
                           GPIOINT_PSEUDO_EDGE_RISE :
                           GPIOINT_PSEUDO_EDGE_FALL,
                           board_sdcard_detect_int);
    }

   return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_sdcard_initialize
 *
 * Description:
 *   Initialize SD Card on the board.
 *
 ****************************************************************************/

int board_sdcard_initialize(void)
{
  int ret = OK;

  cxd56_gpio_config(SDCARD_TXS02612_SEL, false);

#ifdef CONFIG_SDCARD_TXS02612_PORT0
  /* Select port0 for SD-Card */

  cxd56_gpio_write(SDCARD_TXS02612_SEL, false);
#else
  /* Select port1 for SDIO other than SD-Card */

  cxd56_gpio_write(SDCARD_TXS02612_SEL, true);
#endif

#ifdef CONFIG_MMCSD_HAVECARDDETECT
  /* Initialize Card insert status */

  g_sdhci.inserted = false;

  /* Configure Interrupt pin with internal pull-up */

  cxd56_pin_config(PINCONF_SDIO_CD_GPIO);
  ret = cxd56_gpioint_config(PIN_SDIO_CD,
                             GPIOINT_PSEUDO_EDGE_FALL,
                             board_sdcard_detect_int);
  if (ret < 0)
    {
      _err("ERROR: Failed to configure GPIO int. \n");
    }

  /* Enabling Interrupt */

  cxd56_gpioint_enable(PIN_SDIO_CD);
#else
  /* Initialize Card insert status */

  g_sdhci.inserted = true;

  /* Enable SDC */

  board_sdcard_enable(NULL);
#endif

  return ret;
}

/****************************************************************************
 * Name: board_sdcard_finalize
 *
 * Description:
 *   Finalize SD Card on the board.
 *
 ****************************************************************************/

int board_sdcard_finalize(void)
{
  int ret = OK;

  /* At first, Disable interrupt of the card detection */

  if (g_sdhci.inserted)
    {
      board_sdcard_disable(NULL);
    }

  g_sdhci.inserted = false;

#ifdef CONFIG_MMCSD_HAVECARDDETECT
  /* Disabling Interrupt */

  cxd56_gpioint_disable(PIN_SDIO_CD);
#endif

  /* Disable SDIO pin configuration */

  CXD56_PIN_CONFIGS(PINCONFS_SDIOA_GPIO);

  /* Set GPIO pin to initial state */

  cxd56_gpio_write(PIN_SDIO_CLK, false);
  cxd56_gpio_write(PIN_SDIO_CMD, false);
  cxd56_gpio_write(PIN_SDIO_DATA0, false);
  cxd56_gpio_write(PIN_SDIO_DATA1, false);
  cxd56_gpio_write(PIN_SDIO_DATA2, false);
  cxd56_gpio_write(PIN_SDIO_DATA3, false);
  cxd56_gpio_write_hiz(SDCARD_TXS02612_SEL);

  return ret;
}

/****************************************************************************
 * Name: board_sdcard_pin_initialize
 *
 * Description:
 *   Initialize SD Card pins on the board.
 *
 ****************************************************************************/

void board_sdcard_pin_initialize(void)
{
}

/****************************************************************************
 * Name: board_sdcard_pin_finalize
 *
 * Description:
 *   Finalize SD Card pins on the board.
 *
 ****************************************************************************/

void board_sdcard_pin_finalize(void)
{
}

/****************************************************************************
 * Name: board_sdcard_pin_configuraton
 *
 * Description:
 *   Configure SD Card pins on the board.
 *   This is called when SDHCI is used.
 *
 ****************************************************************************/

void board_sdcard_pin_configuraton(void)
{
  /* SDIO configuration */

  modifyreg32(CXD56_SDHCI_USERDEF1CTL, SDHCI_UDEF1_SDCLKI_SEL,
              SDHCI_UDEF1_SDCLKI_SEL_INT);
  modifyreg32(CXD56_SDHCI_USERDEF2CTL, SDHCI_UDEF2_CMD_SEL,
              SDHCI_UDEF2_CMD_SEL_INT);

  /* Disable GPIO output */

  cxd56_gpio_write_hiz(PIN_SDIO_CLK);
  cxd56_gpio_write_hiz(PIN_SDIO_CMD);
  cxd56_gpio_write_hiz(PIN_SDIO_DATA0);
  cxd56_gpio_write_hiz(PIN_SDIO_DATA1);
  cxd56_gpio_write_hiz(PIN_SDIO_DATA2);
  cxd56_gpio_write_hiz(PIN_SDIO_DATA3);

  /* SDIO pin configuration */

  CXD56_PIN_CONFIGS(PINCONFS_SDIOA_SDIO);
}

/****************************************************************************
 * Name: board_sdcard_pin_enable
 *
 * Description:
 *   Enable SD Card on the board.
 *
 ****************************************************************************/

void board_sdcard_pin_enable(void)
{
}

/****************************************************************************
 * Name: board_sdcard_pin_disable
 *
 * Description:
 *   Disable SD Card pins on the board.
 *
 ****************************************************************************/

void board_sdcard_pin_disable(void)
{
}

/****************************************************************************
 * Name: board_sdcard_set_high_voltage
 *
 * Description:
 *   Set SD Card IO voltage to 3.3V
 *
 ****************************************************************************/

void board_sdcard_set_high_voltage(void)
{
}

/****************************************************************************
 * Name: board_sdcard_set_low_voltage
 *
 * Description:
 *   Set SD Card IO voltage to 1.8V
 *
 ****************************************************************************/

void board_sdcard_set_low_voltage(void)
{
}
