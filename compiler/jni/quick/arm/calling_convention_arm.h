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

#ifndef ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_
#define ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_

#include "base/macros.h"
#include "base/pointer_size.h"
#include "jni/quick/calling_convention.h"

namespace art HIDDEN {
namespace arm {

class ArmManagedRuntimeCallingConvention final : public ManagedRuntimeCallingConvention {
 public:
  ArmManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, std::string_view shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k32),
        gpr_index_(1u),  // Skip r0 for ArtMethod*
        float_index_(0u),
        double_index_(0u) {}
  ~ArmManagedRuntimeCallingConvention() override {}
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
  size_t gpr_index_;
  size_t float_index_;
  size_t double_index_;
  DISALLOW_COPY_AND_ASSIGN(ArmManagedRuntimeCallingConvention);
};

class ArmJniCallingConvention final : public JniCallingConvention {
 public:
  ArmJniCallingConvention(bool is_static,
                          bool is_synchronized,
                          bool is_fast_native,
                          bool is_critical_native,
                          std::string_view shorty);
  ~ArmJniCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() const override;
  ManagedRegister IntReturnRegister() const override;
  // JNI calling convention
  void Next() override;  // Override default behavior for AAPCS
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

  // AAPCS mandates return values are extended.
  bool RequiresSmallResultTypeExtension() const override {
    return false;
  }

  // Locking argument register, used to pass the synchronization object for calls
  // to `JniLockObject()` and `JniUnlockObject()`.
  ManagedRegister LockingArgumentRegister() const override;

  // Hidden argument register, used to pass the method pointer for @CriticalNative call.
  ManagedRegister HiddenArgumentRegister() const override;

  // Whether to use tail call (used only for @CriticalNative).
  bool UseTailCall() const override;

 private:
  // Padding to ensure longs and doubles are not split in AAPCS
  size_t padding_;

  DISALLOW_COPY_AND_ASSIGN(ArmJniCallingConvention);
};

}  // namespace arm
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_ARM_CALLING_CONVENTION_ARM_H_
