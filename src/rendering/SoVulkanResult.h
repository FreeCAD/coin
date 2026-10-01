// src/rendering/SoVulkanResult.h
//
// Lightweight status type for Coin's Vulkan internals.  Not public API.  Replaces
// bare-SbBool checks that logged the reason as a side effect: Result carries a
// status + message so a failure reaches a decision point (e.g. drop to raster)
// without log parsing; the public SoRenderBackend keeps SbBool for ABI stability.

#ifndef COIN_SOVULKANRESULT_H
#define COIN_SOVULKANRESULT_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include <cstdint>
#include <string>
#include <utility>

namespace SoVulkan {

enum class Status : uint8_t {
  Ok = 0,
  Error,            //!< generic failure (allocation, upload, recording)
  InvalidArgument,  //!< caller misuse (null target, bad params)
};

/*!
  \brief A status plus an optional reason.

  Default-constructed is Ok.  Only failures allocate a message; the Ok path is
  a single enum compare.  Copyable and movable.
*/
class Result {
public:
  Result() = default;

  static Result ok()
  {
    return Result {};
  }
  static Result error(std::string message)
  {
    return Result(Status::Error, std::move(message));
  }
  static Result invalidArgument(std::string message)
  {
    return Result(Status::InvalidArgument, std::move(message));
  }

  bool isOk() const { return this->status_ == Status::Ok; }
  explicit operator bool() const { return this->isOk(); }
  Status status() const { return this->status_; }
  const std::string & message() const { return this->message_; }

private:
  Result(Status status, std::string message)
    : status_(status), message_(std::move(message))
  {
  }

  Status status_ = Status::Ok;
  std::string message_;
};

/*!
  \brief Propagate a failed Result out of the current function.

  Usage: `COIN_VULKAN_TRY(uploadTexture(...));` where the enclosing function
  returns SoVulkan::Result.  A no-op on success.
*/
#define COIN_VULKAN_TRY(expr)                                                  \
  do {                                                                         \
    ::SoVulkan::Result coin_vulkan_try_result = (expr);                        \
    if (!coin_vulkan_try_result.isOk()) {                                      \
      return coin_vulkan_try_result;                                           \
    }                                                                          \
  } while (false)

} // namespace SoVulkan

#endif // COIN_SOVULKANRESULT_H
