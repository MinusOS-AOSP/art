/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_
#define ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_

#include "base/macros.h"
#include "base/pointer_size.h"
#include "jni/quick/calling_convention.h"

namespace art HIDDEN {
namespace x86 {

class X86ManagedRuntimeCallingConvention final : public ManagedRuntimeCallingConvention {
 public:
  X86ManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, std::string_view shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k32),
        gpr_arg_count_(1u) {}  // Skip EAX for ArtMethod*
  ~X86ManagedRuntimeCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() const override;
  void ResetIterator(FrameOffset displacement) override;
  // Managed runtime calling convention
  ManagedRegister MethodRegister() override;
  ManagedRegister ArgumentRegisterForMethodExitHook() override;
  void Next() override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;

 private:
  int gpr_arg_count_;
  DISALLOW_COPY_AND_ASSIGN(X86ManagedRuntimeCallingConvention);
};

// Implements the x86 cdecl calling convention.
class X86JniCallingConvention final : public JniCallingConvention {
 public:
  X86JniCallingConvention(bool is_static,
                          bool is_synchronized,
                          bool is_fast_native,
                          bool is_critical_native,
                          std::string_view shorty);
  ~X86JniCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() const override;
  ManagedRegister IntReturnRegister() const override;
  // JNI calling convention
  size_t FrameSize() const override;
  size_t OutFrameSize() const override;
  ArrayRef<const ManagedRegister> CalleeSaveRegisters() const override;
  ArrayRef<const ManagedRegister> CalleeSaveScratchRegisters() const override;
  ArrayRef<const ManagedRegister> ArgumentScratchRegisters() const override;
  uint32_t CoreSpillMask() const override;
  uint32_t FpSpillMask() const override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;

  // x86 needs to extend small return types.
  bool RequiresSmallResultTypeExtension() const override {
    return HasSmallReturnType();
  }

  // Locking argument register, used to pass the synchronization object for calls
  // to `JniLockObject()` and `JniUnlockObject()`.
  ManagedRegister LockingArgumentRegister() const override;

  // Hidden argument register, used to pass the method pointer for @CriticalNative call.
  ManagedRegister HiddenArgumentRegister() const override;

  // Whether to use tail call (used only for @CriticalNative).
  bool UseTailCall() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(X86JniCallingConvention);
};

}  // namespace x86
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_X86_CALLING_CONVENTION_X86_H_
