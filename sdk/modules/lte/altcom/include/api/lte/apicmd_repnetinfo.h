/****************************************************************************
 * modules/lte/altcom/include/api/lte/apicmd_rep_netinfo.h
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

#ifndef __MODULES_LTE_ALTCOM_INCLUDE_API_LTE_APICMD_REPNETINFO_H
#define __MODULES_LTE_ALTCOM_INCLUDE_API_LTE_APICMD_REPNETINFO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "apicmd.h"
#include "apicmd_pdn.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define APICMD_REPNETINFO_RES_OK                 (0)
#define APICMD_REPNETINFO_RES_ERR                (1)

#define APICMD_REPNETINFO_REPORT_ENABLE          (0)
#define APICMD_REPNETINFO_REPORT_DISABLE         (1)

#define APICMD_REPNETINFO_PDNCOUNT_MAX           (5)

#define APICMD_REPNETINFO_NWSTAT_ATCH            (0)
#define APICMD_REPNETINFO_NWSTAT_DTCH            (1)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* This structure discribes the data structure of the API command */

begin_packed_struct struct apicmd_cmddat_set_repnetinfo_s
{
  uint8_t report;
} end_packed_struct;

begin_packed_struct struct apicmd_cmddat_set_repnetinfores_s
{
  uint8_t result;
} end_packed_struct;

begin_packed_struct struct apicmd_cmddat_rep_netinfo_s
{
  uint8_t nw_stat;
  uint8_t pdn_count;
  struct apicmd_pdnset_s pdn[APICMD_REPNETINFO_PDNCOUNT_MAX];
} end_packed_struct;

#endif /* __MODULES_LTE_ALTCOM_INCLUDE_API_LTE_APICMD_REPNETINFO_H */
