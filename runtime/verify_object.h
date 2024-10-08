/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFY_OBJECT_H_
#define ART_RUNTIME_VERIFY_OBJECT_H_

#include <stdint.h>

#include "base/macros.h"
#include "obj_ptr.h"

namespace art HIDDEN {

namespace mirror {
class Class;
class Object;
}  // namespace mirror

// How we want to check the heap's correctness.
enum VerifyObjectMode {
  kVerifyObjectModeDisabled,  // Heap verification is disabled.
  kVerifyObjectModeFast,  // Check heap accesses quickly by using VerifyClassClass.
  kVerifyObjectModeAll  // Check heap accesses thoroughly.
};

enum EXPORT VerifyObjectFlags {
  kVerifyNone = 0x0,
  // Verify self when we are doing an operation.
  kVerifyThis = 0x1,
  // Verify reads from objects.
  kVerifyReads = 0x2,
  // Verify writes to objects.
  kVerifyWrites = 0x4,
  // Verify all things.
  kVerifyAll = kVerifyThis | kVerifyReads | kVerifyWrites,
};

static constexpr VerifyObjectFlags kDefaultVerifyFlags = kVerifyNone;
static constexpr VerifyObjectMode kVerifyObjectSupport =
    kDefaultVerifyFlags != 0 ? kVerifyObjectModeFast : kVerifyObjectModeDisabled;

// Implements the actual object checks.
void VerifyObjectImpl(ObjPtr<mirror::Object> obj) NO_THREAD_SAFETY_ANALYSIS;

// Is a front to optimize out any calls if no verification is enabled.
ALWAYS_INLINE
static inline void VerifyObject(ObjPtr<mirror::Object> obj) NO_THREAD_SAFETY_ANALYSIS {
  if (kVerifyObjectSupport > kVerifyObjectModeDisabled && obj != nullptr) {
    VerifyObjectImpl(obj);
  }
}

inline constexpr VerifyObjectFlags RemoveThisFlags(VerifyObjectFlags flags) {
  return static_cast<VerifyObjectFlags>(flags & ~kVerifyThis);
}

// Check that c.getClass() == c.getClass().getClass().
ALWAYS_INLINE bool VerifyClassClass(ObjPtr<mirror::Class> c) NO_THREAD_SAFETY_ANALYSIS;

}  // namespace art

#endif  // ART_RUNTIME_VERIFY_OBJECT_H_
