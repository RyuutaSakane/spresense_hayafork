/****************************************************************************
 * modules/lte/altcom/api/lte/lte_setce.c
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

#include <stdint.h>
#include <errno.h>

#include "lte/lte_api.h"
#include "buffpoolwrapper.h"
#include "dbg_if.h"
#include "osal.h"
#include "apiutil.h"
#include "apicmdgw.h"
#include "apicmd_setce.h"
#include "evthdlbs.h"
#include "apicmdhdlrbs.h"
#include "altcom_callbacks.h"
#include "altcombs.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SETCE_DATA_LEN (sizeof(struct apicmd_cmddat_setce_s))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: setce_status_chg_cb
 *
 * Description:
 *   Notification status change in processing set CE.
 *
 * Input Parameters:
 *  new_stat    Current status.
 *  old_stat    Preview status.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static int32_t setce_status_chg_cb(int32_t new_stat, int32_t old_stat)
{
  if (new_stat < ALTCOM_STATUS_POWER_ON)
    {
      DBGIF_LOG2_INFO("setce_status_chg_cb(%d -> %d)\n",
        old_stat, new_stat);
      altcomcallbacks_unreg_cb(APICMDID_SET_CE);

      return ALTCOM_STATUS_REG_CLR;
    }

  return ALTCOM_STATUS_REG_KEEP;
}

/****************************************************************************
 * Name: setce_job
 *
 * Description:
 *   This function is an API callback for set CE.
 *
 * Input Parameters:
 *  arg    Pointer to received event.
 *
 * Returned Value:
 *   None.
 *
 ****************************************************************************/

static void setce_job(FAR void *arg)
{
  int32_t                             ret;
  FAR struct apicmd_cmddat_setceres_s *data;
  set_ce_cb_t                         callback;

  data = (FAR struct apicmd_cmddat_setceres_s *)arg;

  ret = altcomcallbacks_get_unreg_cb(APICMDID_SET_CE,
    (void **)&callback);

  if ((ret == 0) && (callback))
    {
      if (APICMD_SETCE_RES_OK == data->result)
        {
          callback(LTE_RESULT_OK);
        }
      else
        {
          callback(LTE_RESULT_ERROR);
          DBGIF_ASSERT(APICMD_SETCE_RES_ERR == data->result, "result parameter error.\n");
        }
    }
  else
    {
      DBGIF_LOG_ERROR("Unexpected!! callback is NULL.\n");
    }

  /* In order to reduce the number of copies of the receive buffer,
   * bring a pointer to the receive buffer to the worker thread.
   * Therefore, the receive buffer needs to be released here. */

  altcom_free_cmd((FAR uint8_t *)arg);

  /* Unregistration status change callback. */

  altcomstatus_unreg_statchgcb(setce_status_chg_cb);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lte_set_ce
 *
 * Description:
 *   Get CE settings.
 *
 * Input Parameters:
 *   callback Callback function to notify that set CE settings is
 *            completed.
 *
 * Returned Value:
 *   On success, 0 is returned.
 *   On failure, negative value is returned.
 *
 ****************************************************************************/

int32_t lte_set_ce(lte_ce_setting_t *settings, set_ce_cb_t callback)
{
  int32_t                        ret;
  struct apicmd_cmddat_setce_s *cmddat;

  /* Return error if callback is NULL */

  if (!settings || !callback)
    {
      DBGIF_LOG_ERROR("Input argument is NULL.\n");
      return -EINVAL;
    }

  /* Check Lte library status */

  ret = altcombs_check_poweron_status();
  if (0 > ret)
    {
      return ret;
    }

  /* Register API callback */

  ret = altcomcallbacks_chk_reg_cb((void *)callback, APICMDID_SET_CE);
  if (0 > ret)
    {
      DBGIF_LOG_ERROR("Currently API is busy.\n");
      return -EINPROGRESS;
    }

  ret = altcomstatus_reg_statchgcb(setce_status_chg_cb);
  if (0 > ret)
    {
      DBGIF_LOG_ERROR("Failed to registration status change callback.\n");
      altcomcallbacks_unreg_cb(APICMDID_SET_CE);
      return ret;
    }

  /* Allocate API command buffer to send */

  cmddat = (struct apicmd_cmddat_setce_s *)apicmdgw_cmd_allocbuff(
    APICMDID_SET_CE, SETCE_DATA_LEN);
  if (!cmddat)
    {
      DBGIF_LOG_ERROR("Failed to allocate command buffer.\n");
      ret = -ENOMEM;
    }
  else
    {
      cmddat->mode_a_enable = settings->mode_a_enable ?
        APICMD_SETCE_ENABLE : APICMD_SETCE_DISABLE;
      cmddat->mode_b_enable = settings->mode_b_enable ?
        APICMD_SETCE_ENABLE : APICMD_SETCE_DISABLE;

      /* Send API command to modem */

      ret = altcom_send_and_free((FAR uint8_t *)cmddat);
    }

  /* If fail, there is no opportunity to execute the callback,
   * so clear it here. */

  if (ret < 0)
    {
      /* Clear registered callback */

      altcomcallbacks_unreg_cb(APICMDID_SET_CE);
      altcomstatus_unreg_statchgcb(setce_status_chg_cb);
    }
  else
    {
      ret = 0;
    }

  return ret;
}


/****************************************************************************
 * Name: apicmdhdlr_setce
 *
 * Description:
 *   This function is an API command handler for set CE result.
 *
 * Input Parameters:
 *  evt    Pointer to received event.
 *  evlen  Length of received event.
 *
 * Returned Value:
 *   If the API command ID matches APICMDID_SET_CE_RES,
 *   EVTHDLRC_STARTHANDLE is returned.
 *   Otherwise it returns EVTHDLRC_UNSUPPORTEDEVENT. If an internal error is
 *   detected, EVTHDLRC_INTERNALERROR is returned.
 *
 ****************************************************************************/

enum evthdlrc_e apicmdhdlr_setce(FAR uint8_t *evt, uint32_t evlen)
{
  return apicmdhdlrbs_do_runjob(evt, APICMDID_CONVERT_RES(APICMDID_SET_CE),
    setce_job);
}
