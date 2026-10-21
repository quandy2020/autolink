/******************************************************************************
 * Copyright 2025 The Openbot Authors (duyongquan)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "autolink/transport/shm/segment.hpp"

#include <algorithm>
#include <thread>

#include "autolink/common/log.hpp"
#include "autolink/common/util.hpp"
#include "autolink/transport/shm/shm_conf.hpp"

namespace autolink {
namespace transport {

Segment::Segment(uint64_t channel_id)
    : init_(false),
      conf_(),
      channel_id_(channel_id),
      state_(nullptr),
      blocks_(nullptr),
      arena_blocks_(nullptr),
      managed_shm_(nullptr),
      block_buf_lock_(),
      arena_block_buf_lock_(),
      block_buf_addrs_(),
      arena_block_buf_addrs_() {}

bool Segment::AcquireBlockToWrite(std::size_t msg_size,
                                  WritableBlock* writable_block) {
    RETURN_VAL_IF_NULL(writable_block, false);
    if (!init_ && !OpenOrCreate()) {
        AERROR << "create shm failed, can't write now.";
        return false;
    }

    bool result = true;
    if (state_->need_remap()) {
        result = Remap();
    }

    if (msg_size > conf_.ceiling_msg_size()) {
        // Ceiling tiers top out at MESSAGE_SIZE_MORE. Recreate(msg) when
        // msg_size is larger still yields the same ceiling — Remove()+Open
        // then writes past the block (or spins) and starves every peer on
        // that channel (trajectory / cloud_map "Write failed").
        const uint64_t max_ceiling = ShmConf::MaxCeilingMessageSize();
        if (msg_size > max_ceiling) {
            AERROR << "msg_size: " << msg_size
                   << " exceeds SHM hard cap " << max_ceiling
                   << " (ceiling=" << conf_.ceiling_msg_size()
                   << "); drop write instead of recreate.";
            return false;
        }
        ShmConf next_conf(msg_size);
        if (next_conf.ceiling_msg_size() <= conf_.ceiling_msg_size()) {
            AERROR << "msg_size: " << msg_size
                   << " needs larger SHM but ceiling already "
                   << conf_.ceiling_msg_size() << "; drop write.";
            return false;
        }
        AINFO << "msg_size: " << msg_size
              << " larger than current shm_buffer_size: "
              << conf_.ceiling_msg_size() << " , need recreate.";
        result = Recreate(msg_size);
    }

    if (!result) {
        AERROR << "segment update failed.";
        return false;
    }

    const int32_t index = GetNextWritableBlockIndex();
    if (index < 0) {
        AERROR << "acquire writable shm block failed (all blocks busy).";
        return false;
    }
    writable_block->index = static_cast<uint32_t>(index);
    writable_block->block = &blocks_[writable_block->index];
    writable_block->buf = block_buf_addrs_[writable_block->index];
    return true;
}

bool Segment::AcquireArenaBlockToWrite(std::size_t msg_size,
                                       WritableBlock* writable_block) {
    RETURN_VAL_IF_NULL(writable_block, false);
    if (!init_ && !OpenOrCreate()) {
        AERROR << "create shm failed, can't write now.";
        return false;
    }

    if (state_->need_remap()) {
        Remap();
    }

    const int32_t index = GetNextArenaWritableBlockIndex();
    if (index < 0) {
        AERROR << "acquire writable arena shm block failed (all blocks busy).";
        return false;
    }
    writable_block->index = static_cast<uint32_t>(index);
    writable_block->block = &arena_blocks_[writable_block->index];
    writable_block->buf = arena_block_buf_addrs_[writable_block->index];
    return true;
}

void Segment::ReleaseWrittenBlock(const WritableBlock& writable_block) {
    auto index = writable_block.index;
    if (index >= conf_.block_num()) {
        return;
    }
    blocks_[index].ReleaseWriteLock();
}

void Segment::ReleaseArenaWrittenBlock(const WritableBlock& writable_block) {
    auto index = writable_block.index;
    if (index >= ShmConf::ARENA_BLOCK_NUM) {
        return;
    }
    arena_blocks_[index].ReleaseWriteLock();
}

bool Segment::AcquireBlockToRead(ReadableBlock* readable_block) {
    RETURN_VAL_IF_NULL(readable_block, false);
    if (!init_ && !OpenOnly()) {
        AERROR << "failed to open shared memory, can't read now.";
        return false;
    }

    auto index = readable_block->index;
    if (index >= conf_.block_num()) {
        AERROR << "invalid block_index[" << index << "].";
        return false;
    }

    bool result = true;
    if (state_->need_remap()) {
        result = Remap();
    }

    if (!result) {
        AERROR << "segment update failed.";
        return false;
    }

    if (!blocks_[index].TryLockForRead()) {
        return false;
    }
    readable_block->block = blocks_ + index;
    readable_block->buf = block_buf_addrs_[index];
    return true;
}

bool Segment::AcquireArenaBlockToRead(ReadableBlock* readable_block) {
    RETURN_VAL_IF_NULL(readable_block, false);
    if (!init_ && !OpenOnly()) {
        AERROR << "failed to open shared memory, can't read now.";
        return false;
    }

    auto index = readable_block->index;
    if (index >= ShmConf::ARENA_BLOCK_NUM) {
        AERROR << "invalid arena block_index[" << index << "].";
        return false;
    }

    bool result = true;
    if (state_->need_remap()) {
        result = Remap();
    }

    if (!result) {
        AERROR << "segment update failed.";
        return false;
    }

    if (!arena_blocks_[index].TryLockForRead()) {
        return false;
    }
    readable_block->block = arena_blocks_ + index;
    readable_block->buf = arena_block_buf_addrs_[index];
    return true;
}

void Segment::ReleaseArenaReadBlock(const ReadableBlock& readable_block) {
    auto index = readable_block.index;
    if (index >= ShmConf::ARENA_BLOCK_NUM) {
        return;
    }
    arena_blocks_[index].ReleaseReadLock();
}

void Segment::ReleaseReadBlock(const ReadableBlock& readable_block) {
    auto index = readable_block.index;
    if (index >= conf_.block_num()) {
        return;
    }
    blocks_[index].ReleaseReadLock();
}

bool Segment::InitOnly(uint64_t message_size) {
    if (init_) {
        return true;
    }
    conf_.Update(message_size);
    if (!OpenOrCreate()) {
        return false;
    }
    return true;
}

void* Segment::GetManagedShm() {
    return managed_shm_;
}

bool Segment::LockBlockForWriteByIndex(uint64_t block_index) {
    if (block_index >= conf_.block_num()) {
        return false;
    }
    return blocks_[block_index].TryLockForWrite();
}

bool Segment::ReleaseBlockForWriteByIndex(uint64_t block_index) {
    if (block_index >= conf_.block_num()) {
        return false;
    }
    blocks_[block_index].ReleaseWriteLock();
    return true;
}

bool Segment::LockBlockForReadByIndex(uint64_t block_index) {
    if (block_index >= conf_.block_num()) {
        return false;
    }
    return blocks_[block_index].TryLockForRead();
}

bool Segment::ReleaseBlockForReadByIndex(uint64_t block_index) {
    if (block_index >= conf_.block_num()) {
        return false;
    }
    blocks_[block_index].ReleaseReadLock();
    return true;
}

bool Segment::LockArenaBlockForWriteByIndex(uint64_t block_index) {
    if (block_index >= ShmConf::ARENA_BLOCK_NUM) {
        return false;
    }
    return arena_blocks_[block_index].TryLockForWrite();
}

bool Segment::ReleaseArenaBlockForWriteByIndex(uint64_t block_index) {
    if (block_index >= ShmConf::ARENA_BLOCK_NUM) {
        return false;
    }
    arena_blocks_[block_index].ReleaseWriteLock();
    return true;
}

bool Segment::LockArenaBlockForReadByIndex(uint64_t block_index) {
    if (block_index >= ShmConf::ARENA_BLOCK_NUM) {
        return false;
    }
    return arena_blocks_[block_index].TryLockForRead();
}

bool Segment::ReleaseArenaBlockForReadByIndex(uint64_t block_index) {
    if (block_index >= ShmConf::ARENA_BLOCK_NUM) {
        return false;
    }
    arena_blocks_[block_index].ReleaseReadLock();
    return true;
}

bool Segment::Destroy() {
    if (!init_) {
        return true;
    }
    init_ = false;

    try {
        state_->DecreaseReferenceCounts();
        uint32_t reference_counts = state_->reference_counts();
        if (reference_counts == 0) {
            return Remove();
        }
    } catch (...) {
        AERROR << "exception.";
        return false;
    }
    ADEBUG << "destroy.";
    return true;
}

bool Segment::Remap() {
    init_ = false;
    ADEBUG << "before reset.";
    Reset();
    ADEBUG << "after reset.";
    return OpenOnly();
}

bool Segment::Recreate(const uint64_t& msg_size) {
    init_ = false;
    state_->set_need_remap(true);
    Reset();
    Remove();
    conf_.Update(msg_size);
    return OpenOrCreate();
}

int32_t Segment::GetNextWritableBlockIndex() {
    const auto block_num = conf_.block_num();
    // Cap retries: all blocks held by slow readers used to spin forever here
    // (Argus publishing large PointCloud2 + Autoviz reading → 100% CPU, no logs).
    const uint32_t max_tries = std::max<uint32_t>(block_num * 64u, 256u);
    for (uint32_t n = 0; n < max_tries; ++n) {
        uint32_t try_idx = state_->FetchAddSeq(1) % block_num;
        if (blocks_[try_idx].TryLockForWrite()) {
            return static_cast<int32_t>(try_idx);
        }
        if ((n & 0x3fu) == 0x3fu) {
            std::this_thread::yield();
        }
    }
    AERROR << "no writable shm block after " << max_tries
           << " tries (block_num=" << block_num << ")";
    return -1;
}

int32_t Segment::GetNextArenaWritableBlockIndex() {
    const auto block_num = ShmConf::ARENA_BLOCK_NUM;
    const uint32_t max_tries = std::max<uint32_t>(block_num * 64u, 256u);
    for (uint32_t n = 0; n < max_tries; ++n) {
        uint32_t try_idx = state_->FetchAddArenaSeq(1) % block_num;
        if (arena_blocks_[try_idx].TryLockForWrite()) {
            return static_cast<int32_t>(try_idx);
        }
        if ((n & 0x3fu) == 0x3fu) {
            std::this_thread::yield();
        }
    }
    AERROR << "no writable arena shm block after " << max_tries << " tries";
    return -1;
}

}  // namespace transport
}  // namespace autolink
