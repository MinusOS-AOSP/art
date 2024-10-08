/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "emulated_stack_frame-inl.h"

#include "array-alloc-inl.h"
#include "array-inl.h"
#include "class-alloc-inl.h"
#include "class_root-inl.h"
#include "handle.h"
#include "jvalue-inl.h"
#include "method_handles-inl.h"
#include "method_handles.h"
#include "method_type-inl.h"
#include "object_array-alloc-inl.h"
#include "object_array-inl.h"
#include "reflection-inl.h"

namespace art HIDDEN {
namespace mirror {

// Calculates the size of a stack frame based on the size of its argument
// types and return types.
static void CalculateFrameAndReferencesSize(ObjPtr<mirror::ObjectArray<mirror::Class>> p_types,
                                            ObjPtr<mirror::Class> r_type,
                                            size_t* frame_size_out,
                                            size_t* references_size_out)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const size_t length = p_types->GetLength();
  size_t frame_size = 0;
  size_t references_size = 0;
  for (size_t i = 0; i < length; ++i) {
    ObjPtr<mirror::Class> type = p_types->GetWithoutChecks(i);
    const Primitive::Type primitive_type = type->GetPrimitiveType();
    if (primitive_type == Primitive::kPrimNot) {
      references_size++;
    } else if (Primitive::Is64BitType(primitive_type)) {
      frame_size += 8;
    } else {
      frame_size += 4;
    }
  }

  const Primitive::Type return_type = r_type->GetPrimitiveType();
  if (return_type == Primitive::kPrimNot) {
    references_size++;
  } else if (Primitive::Is64BitType(return_type)) {
    frame_size += 8;
  } else {
    frame_size += 4;
  }

  (*frame_size_out) = frame_size;
  (*references_size_out) = references_size;
}

// Allows for read or write access to an emulated stack frame. Each
// accessor index has an associated index into the references / stack frame
// arrays which is incremented on every read or write to the frame.
//
// This class is used in conjunction with PerformConversions, either as a setter
// or as a getter.
class EmulatedStackFrameAccessor {
 public:
  EmulatedStackFrameAccessor(Handle<mirror::ObjectArray<mirror::Object>> references,
                             Handle<mirror::ByteArray> stack_frame,
                             size_t stack_frame_size) :
    references_(references),
    stack_frame_(stack_frame),
    stack_frame_size_(stack_frame_size),
    reference_idx_(0u),
    stack_frame_idx_(0u) {
  }

  ALWAYS_INLINE void SetReference(ObjPtr<mirror::Object> reference)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    references_->Set(reference_idx_++, reference);
  }

  ALWAYS_INLINE void Set(const uint32_t value) REQUIRES_SHARED(Locks::mutator_lock_) {
    int8_t* array = stack_frame_->GetData();

    CHECK_LE((stack_frame_idx_ + 4u), stack_frame_size_);
    memcpy(array + stack_frame_idx_, &value, sizeof(uint32_t));
    stack_frame_idx_ += 4u;
  }

  ALWAYS_INLINE void SetLong(const int64_t value) REQUIRES_SHARED(Locks::mutator_lock_) {
    int8_t* array = stack_frame_->GetData();

    CHECK_LE((stack_frame_idx_ + 8u), stack_frame_size_);
    memcpy(array + stack_frame_idx_, &value, sizeof(int64_t));
    stack_frame_idx_ += 8u;
  }

  ALWAYS_INLINE ObjPtr<mirror::Object> GetReference() REQUIRES_SHARED(Locks::mutator_lock_) {
    return references_->Get(reference_idx_++);
  }

  ALWAYS_INLINE uint32_t Get() REQUIRES_SHARED(Locks::mutator_lock_) {
    const int8_t* array = stack_frame_->GetData();

    CHECK_LE((stack_frame_idx_ + 4u), stack_frame_size_);
    uint32_t val = 0;

    memcpy(&val, array + stack_frame_idx_, sizeof(uint32_t));
    stack_frame_idx_ += 4u;
    return val;
  }

  ALWAYS_INLINE int64_t GetLong() REQUIRES_SHARED(Locks::mutator_lock_) {
    const int8_t* array = stack_frame_->GetData();

    CHECK_LE((stack_frame_idx_ + 8u), stack_frame_size_);
    int64_t val = 0;

    memcpy(&val, array + stack_frame_idx_, sizeof(int64_t));
    stack_frame_idx_ += 8u;
    return val;
  }

 private:
  Handle<mirror::ObjectArray<mirror::Object>> references_;
  Handle<mirror::ByteArray> stack_frame_;
  const size_t stack_frame_size_;

  size_t reference_idx_;
  size_t stack_frame_idx_;

  DISALLOW_COPY_AND_ASSIGN(EmulatedStackFrameAccessor);
};

ObjPtr<mirror::EmulatedStackFrame> EmulatedStackFrame::CreateFromShadowFrameAndArgs(
    Thread* self,
    Handle<mirror::MethodType> caller_type,
    Handle<mirror::MethodType> callee_type,
    const ShadowFrame& caller_frame,
    const InstructionOperands* const operands) {
  StackHandleScope<6> hs(self);

  DCHECK(callee_type->IsExactMatch(caller_type.Get()));
  Handle<mirror::ObjectArray<mirror::Class>> p_types(hs.NewHandle(callee_type->GetPTypes()));

  // Step 2: Calculate the size of the reference / byte arrays in the emulated
  // stack frame.
  size_t frame_size = 0;
  size_t refs_size = 0;
  Handle<mirror::Class> r_type(hs.NewHandle(callee_type->GetRType()));
  CalculateFrameAndReferencesSize(p_types.Get(), r_type.Get(), &frame_size, &refs_size);

  // Step 3 : Allocate the arrays.
  ObjPtr<mirror::Class> array_class(GetClassRoot<mirror::ObjectArray<mirror::Object>>());

  Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(
      mirror::ObjectArray<mirror::Object>::Alloc(self, array_class, refs_size)));
  if (references == nullptr) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  Handle<ByteArray> stack_frame(hs.NewHandle(ByteArray::Alloc(self, frame_size)));
  if (stack_frame == nullptr) {
    DCHECK(self->IsExceptionPending());
    return nullptr;
  }

  // Step 4 : Copy arguments.
  ShadowFrameGetter getter(caller_frame, operands);
  EmulatedStackFrameAccessor setter(references, stack_frame, stack_frame->GetLength());
  CopyArguments<ShadowFrameGetter, EmulatedStackFrameAccessor>(self, caller_type, &getter, &setter);

  // Step 5: Construct the EmulatedStackFrame object.
  Handle<EmulatedStackFrame> sf(hs.NewHandle(
      ObjPtr<EmulatedStackFrame>::DownCast(GetClassRoot<EmulatedStackFrame>()->AllocObject(self))));
  sf->SetFieldObject<false>(TypeOffset(), callee_type.Get());
  sf->SetFieldObject<false>(ReferencesOffset(), references.Get());
  sf->SetFieldObject<false>(StackFrameOffset(), stack_frame.Get());

  return sf.Get();
}

void EmulatedStackFrame::WriteToShadowFrame(Thread* self,
                                            Handle<mirror::MethodType> callee_type,
                                            const uint32_t first_dest_reg,
                                            ShadowFrame* callee_frame) {
  DCHECK(callee_type->IsExactMatch(GetType()));

  StackHandleScope<3> hs(self);
  Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(GetReferences()));
  Handle<ByteArray> stack_frame(hs.NewHandle(GetStackFrame()));

  EmulatedStackFrameAccessor getter(references, stack_frame, stack_frame->GetLength());
  ShadowFrameSetter setter(callee_frame, first_dest_reg);

  CopyArguments<EmulatedStackFrameAccessor, ShadowFrameSetter>(self, callee_type, &getter, &setter);
}

void EmulatedStackFrame::GetReturnValue(Thread* self, JValue* value) {
  StackHandleScope<2> hs(self);
  Handle<mirror::Class> r_type(hs.NewHandle(GetType()->GetRType()));

  const Primitive::Type type = r_type->GetPrimitiveType();
  if (type == Primitive::kPrimNot) {
    Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(GetReferences()));
    value->SetL(references->GetWithoutChecks(references->GetLength() - 1));
  } else {
    Handle<ByteArray> stack_frame(hs.NewHandle(GetStackFrame()));
    const int8_t* array = stack_frame->GetData();
    const size_t length = stack_frame->GetLength();
    if (Primitive::Is64BitType(type)) {
      int64_t primitive = 0;
      memcpy(&primitive, array + length - sizeof(int64_t), sizeof(int64_t));
      value->SetJ(primitive);
    } else {
      uint32_t primitive = 0;
      memcpy(&primitive, array + length - sizeof(uint32_t), sizeof(uint32_t));
      value->SetI(primitive);
    }
  }
}

void EmulatedStackFrame::SetReturnValue(Thread* self, const JValue& value) {
  StackHandleScope<2> hs(self);
  Handle<mirror::Class> r_type(hs.NewHandle(GetType()->GetRType()));

  const Primitive::Type type = r_type->GetPrimitiveType();
  if (type == Primitive::kPrimNot) {
    Handle<mirror::ObjectArray<mirror::Object>> references(hs.NewHandle(GetReferences()));
    references->SetWithoutChecks<false>(references->GetLength() - 1, value.GetL());
  } else {
    Handle<ByteArray> stack_frame(hs.NewHandle(GetStackFrame()));
    int8_t* array = stack_frame->GetData();
    const size_t length = stack_frame->GetLength();
    if (Primitive::Is64BitType(type)) {
      const int64_t primitive = value.GetJ();
      memcpy(array + length - sizeof(int64_t), &primitive, sizeof(int64_t));
    } else {
      const uint32_t primitive = value.GetI();
      memcpy(array + length - sizeof(uint32_t), &primitive, sizeof(uint32_t));
    }
  }
}

}  // namespace mirror
}  // namespace art
