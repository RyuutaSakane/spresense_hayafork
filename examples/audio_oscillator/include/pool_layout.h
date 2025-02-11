/* This file is generated automatically. */
/****************************************************************************
 * pool_layout.h
 *
 *   Copyright 2019 Sony Semiconductor Solutions Corporation
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

#ifndef POOL_LAYOUT_H_INCLUDED
#define POOL_LAYOUT_H_INCLUDED

#include "memutils/memory_manager/MemMgrTypes.h"

namespace MemMgrLite {

MemPool*  static_pools_block[NUM_MEM_SECTIONS][NUM_MEM_POOLS];
MemPool** static_pools[NUM_MEM_SECTIONS] = {
  static_pools_block[0],
};
uint8_t layout_no[NUM_MEM_SECTIONS] = {
  BadLayoutNo,
};
uint8_t pool_num[NUM_MEM_SECTIONS] = {
  NUM_MEM_S0_POOLS,
};
extern const PoolSectionAttr MemoryPoolLayouts[NUM_MEM_SECTIONS][NUM_MEM_LAYOUTS][7] = {
  {  /* Section:0 */
    {/* Layout:0 */
     /* pool_ID                          type         seg  fence  addr        size         */
      { S0_REND_PCM_BUF_POOL           , BasicType  ,   5,  true, 0x000c0008, 0x00015f90 },  /* AUDIO_WORK_AREA */
      { S0_OSC_APU_CMD_POOL            , BasicType  ,   5,  true, 0x000d5fa0, 0x000001cc },  /* AUDIO_WORK_AREA */
      { S0_PF0_PCM_BUF_POOL            , BasicType  ,   1,  true, 0x000d6178, 0x00004650 },  /* AUDIO_WORK_AREA */
      { S0_PF1_PCM_BUF_POOL            , BasicType  ,   1,  true, 0x000da7d0, 0x00004650 },  /* AUDIO_WORK_AREA */
      { S0_PF0_APU_CMD_POOL            , BasicType  ,   3,  true, 0x000dee28, 0x00000114 },  /* AUDIO_WORK_AREA */
      { S0_PF1_APU_CMD_POOL            , BasicType  ,   3,  true, 0x000def48, 0x00000114 },  /* AUDIO_WORK_AREA */
      { S0_NULL_POOL, 0, 0, false, 0, 0 },
    },
  },
}; /* end of MemoryPoolLayouts */

}  /* end of namespace MemMgrLite */

#endif /* POOL_LAYOUT_H_INCLUDED */
