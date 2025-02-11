/****************************************************************************
 * modules/audio/objects/audio_object_common.cpp
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

#include "memutils/message/Message.h"
#include "audio/audio_object_common_api.h"
#include "audio/audio_message_types.h"
#include "wien2_internal_packet.h"
#include "wien2_common_defs.h"

__USING_WIEN2

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/*--------------------------------------------------------------------------*/
template <typename T>
bool AS_ReceiveObjectReply(MsgQueId msgq_id,
                           T *reply)
{
  return AS_ReceiveObjectReply(msgq_id, TIME_FOREVER, reply);
}

bool AS_ReceiveObjectReply(MsgQueId msgq_id,
                           AudioObjReply *reply)
{
  return AS_ReceiveObjectReply<AudioObjReply>(msgq_id, reply);
}

/*--------------------------------------------------------------------------*/
template <typename T>
bool AS_ReceiveObjectReply(MsgQueId msgq_id,
                           uint32_t ms,
                           T *reply)
{
  err_t           err_code;
  FAR MsgQueBlock *que;
  FAR MsgPacket   *msg;

  if (reply == NULL)
    {
      return false;
    }

  /* Get an instance of the specified message ID. */

  err_code = MsgLib::referMsgQueBlock(msgq_id, &que);
  if (err_code != ERR_OK)
    {
      return false;
    }

  /* Waiting to receive a message. */

  err_code = que->recv(ms, &msg);
  if (err_code != ERR_OK)
    {
      return false;
    }

  if (msg->getType() != MSG_AUD_MGR_RST)
    {
      return false;
    }

  /* Store reply information. */

  *reply = msg->moveParam<T>();

  /* Delete received data. */

  err_code = que->pop();
  if (err_code != ERR_OK)
    {
      return false;
    }

  return true;
}

/*--------------------------------------------------------------------------*/
bool AS_ReceiveObjectReply(MsgQueId msgq_id,
                           uint32_t ms,
                           AudioObjReply *reply)
{
  return AS_ReceiveObjectReply<AudioObjReply>(msgq_id, ms, reply);
}

