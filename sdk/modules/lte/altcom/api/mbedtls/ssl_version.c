/****************************************************************************
 * modules/lte/altcom/api/mbedtls/ssl_version.c
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
#include "apicmd_ssl_version.h"
#include "apiutil.h"
#include "mbedtls/ssl.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SSL_VERSION_REQ_DATALEN (sizeof(struct apicmd_ssl_version_s))
#define SSL_VERSION_RES_DATALEN (sizeof(struct apicmd_ssl_versionres_s))

#define SSL_VERSION_SUCCESS 0
#define SSL_VERSION_FAILURE -1

#define SSL_V_3_0    "SSLv3.0"
#define TLS_V_1_0    "TLSv1.0"
#define TLS_V_1_1    "TLSv1.1"
#define TLS_V_1_2    "TLSv1.2"
#define TLS_UNKNOWN  "unknown"

#define DTLS_V_1_0   "DTLSv1.0"
#define DTLS_V_1_2   "DTLSv1.2"
#define DTLS_UNKNOWN "unknown (DTLS)"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct ssl_version_req_s
{
  uint32_t id;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char* ssl_tls_ver_tbl[]
={
  SSL_V_3_0,
  TLS_V_1_0,
  TLS_V_1_1,
  TLS_V_1_2,
  DTLS_V_1_0,
  DTLS_V_1_2,
  TLS_UNKNOWN,
  DTLS_UNKNOWN,
  NULL
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const char *get_ssl_tls_version(const int8_t *ver_name)
{
  int i = 0;

  for (i = 0; ssl_tls_ver_tbl[i] != NULL; i++)
    {
      if (strcmp((const char*) ver_name, ssl_tls_ver_tbl[i]) == 0)
        {
          return ssl_tls_ver_tbl[i];
        }
    }

  return TLS_UNKNOWN;
}

static int32_t ssl_version_request(FAR struct ssl_version_req_s *req,
                                   const char** res_version)
{
  int32_t                            ret;
  uint16_t                           reslen = 0;
  FAR struct apicmd_ssl_version_s    *cmd = NULL;
  FAR struct apicmd_ssl_versionres_s *res = NULL;

  /* Allocate send and response command buffer */

  if (!altcom_mbedtls_alloc_cmdandresbuff(
    (FAR void **)&cmd, APICMDID_TLS_SSL_VERSION, SSL_VERSION_REQ_DATALEN,
    (FAR void **)&res, SSL_VERSION_RES_DATALEN))
    {
      return SSL_VERSION_FAILURE;
    }

  /* Fill the data */

  cmd->ssl = htonl(req->id);

  DBGIF_LOG1_DEBUG("[ssl_version]ctx id: %d\n", req->id);

  /* Send command and block until receive a response */

  ret = apicmdgw_send((FAR uint8_t *)cmd, (FAR uint8_t *)res,
                      SSL_VERSION_RES_DATALEN, &reslen,
                      SYS_TIMEO_FEVR);

  if (ret < 0)
    {
      DBGIF_LOG1_ERROR("apicmdgw_send error: %d\n", ret);
      goto errout_with_cmdfree;
    }

  if (reslen != SSL_VERSION_RES_DATALEN)
    {
      DBGIF_LOG1_ERROR("Unexpected response data length: %d\n", reslen);
      goto errout_with_cmdfree;
    }

  ret = ntohl(res->ret_code);

  DBGIF_LOG1_DEBUG("[ssl_version res]ret: %d\n", ret);
  DBGIF_LOG1_DEBUG("[ssl_version res]version: %s\n", res->version);

  res->version[APICMD_SSL_VERSION_LEN-1] = '\0';
  *res_version = get_ssl_tls_version(res->version);

  altcom_mbedtls_free_cmdandresbuff(cmd, res);

  return SSL_VERSION_SUCCESS;

errout_with_cmdfree:
  altcom_mbedtls_free_cmdandresbuff(cmd, res);
  return SSL_VERSION_FAILURE;
}



/****************************************************************************
 * Public Functions
 ****************************************************************************/

const char *mbedtls_ssl_get_version(const mbedtls_ssl_context *ssl)
{
  const char*              res_version = TLS_UNKNOWN;
  struct ssl_version_req_s req;

  if (!altcom_isinit())
    {
      DBGIF_LOG_ERROR("Not intialized\n");
      altcom_seterrno(ALTCOM_ENETDOWN);
      return TLS_UNKNOWN;
    }

  req.id = ssl->id;

  ssl_version_request(&req, &res_version);

  return res_version;
}

