/****************************************************************************
 * modules/lte/altcom/api/mbedtls/cipher_update.c
 *
 *   Copyright 2018 Sony Corporation
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
 * 3. Neither the name NuttX nor Sony nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
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

#include <string.h>
#include "dbg_if.h"
#include "altcom_errno.h"
#include "altcom_seterrno.h"
#include "apicmd_cipher_update.h"
#include "apiutil.h"
#include "mbedtls/cipher.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CIPHER_UPDATE_REQ_DATALEN (sizeof(struct apicmd_cipher_update_s))
#define CIPHER_UPDATE_RES_DATALEN (sizeof(struct apicmd_cipher_updateres_s))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct cipher_update_req_s
{
  uint32_t            id;
  const unsigned char *input;
  size_t              ilen;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int32_t cipher_update_request(FAR struct cipher_update_req_s *req,
                                     unsigned char *output, uint32_t *olen)
{
  int32_t                              ret;
  uint16_t                             reslen = 0;
  uint32_t                             out_len = 0;
  FAR struct apicmd_cipher_update_s    *cmd = NULL;
  FAR struct apicmd_cipher_updateres_s *res = NULL;

  /* Allocate send and response command buffer */

  if (!altcom_mbedtls_alloc_cmdandresbuff(
    (FAR void **)&cmd, APICMDID_TLS_CIPHER_UPDATE, CIPHER_UPDATE_REQ_DATALEN,
    (FAR void **)&res, CIPHER_UPDATE_RES_DATALEN))
    {
      return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

  /* Fill the data */

  cmd->ctx = htonl(req->id);
  if (req->ilen <= APICMD_CIPHER_UPDATE_INPUT_LEN)
    {
      memset(cmd->input, 0, APICMD_CIPHER_UPDATE_INPUT_LEN);
      memcpy(cmd->input, req->input, req->ilen);
    }
  else
    {
      goto errout_with_cmdfree;
    }

  cmd->ilen = htonl(req->ilen);

  DBGIF_LOG1_DEBUG("[cipher_update]ctx id: %d\n", req->id);
  DBGIF_LOG1_DEBUG("[cipher_update]ilen: %d\n", req->ilen);

  /* Send command and block until receive a response */

  ret = apicmdgw_send((FAR uint8_t *)cmd, (FAR uint8_t *)res,
                      CIPHER_UPDATE_RES_DATALEN, &reslen,
                      SYS_TIMEO_FEVR);

  if (ret < 0)
    {
      DBGIF_LOG1_ERROR("apicmdgw_send error: %d\n", ret);
      goto errout_with_cmdfree;
    }

  if (reslen != CIPHER_UPDATE_RES_DATALEN)
    {
      DBGIF_LOG1_ERROR("Unexpected response data length: %d\n", reslen);
      goto errout_with_cmdfree;
    }

  ret = ntohl(res->ret_code);
  out_len = ntohl(res->olen);
  memcpy(output, res->output, out_len);
  *olen = out_len;

  DBGIF_LOG1_DEBUG("[cipher_update res]ret: %d\n", ret);

  altcom_mbedtls_free_cmdandresbuff(cmd, res);

  return ret;

errout_with_cmdfree:
  altcom_mbedtls_free_cmdandresbuff(cmd, res);
  return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
}



/****************************************************************************
 * Public Functions
 ****************************************************************************/

int mbedtls_cipher_update(mbedtls_cipher_context_t *ctx, const unsigned char *input,
                          size_t ilen, unsigned char *output, size_t *olen)
{
  int32_t                    result;
  uint32_t                   out_len = 0;
  struct cipher_update_req_s req;

  if(ctx == NULL || input == NULL || output == NULL || olen == NULL)
    {
      return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

  if (!altcom_isinit())
    {
      DBGIF_LOG_ERROR("Not intialized\n");
      altcom_seterrno(ALTCOM_ENETDOWN);
      return MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA;
    }

  req.id = ctx->id;
  req.input = input;
  req.ilen = ilen;

  result = cipher_update_request(&req, output, &out_len);
  *olen = (size_t) out_len;

  return result;
}

