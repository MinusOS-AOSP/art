/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_
#define ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_

#include <stdint.h>
#include <string_view>
#include <vector>

#include "base/casts.h"
#include "base/macros.h"
#include "base/scoped_arena_containers.h"
#include "dex/primitive.h"
#include "gc_root.h"
#include "handle_scope.h"

namespace art HIDDEN {

namespace mirror {
class Class;
class ClassLoader;
}  // namespace mirror

class ClassLinker;
class ScopedArenaAllocator;

namespace verifier {

class BooleanType;
class ByteType;
class CharType;
class ConflictType;
class ConstantType;
class DoubleHiType;
class DoubleLoType;
class FloatType;
class ImpreciseConstType;
class IntegerType;
class LongHiType;
class LongLoType;
class MethodVerifier;
class NullType;
class PreciseConstType;
class PreciseReferenceType;
class RegType;
class ShortType;
class UndefinedType;
class UninitializedType;

// Use 8 bytes since that is the default arena allocator alignment.
static constexpr size_t kDefaultArenaBitVectorBytes = 8;

class RegTypeCache {
 public:
  RegTypeCache(Thread* self,
               ClassLinker* class_linker,
               bool can_load_classes,
               ScopedArenaAllocator& allocator,
               bool can_suspend = true);
  const art::verifier::RegType& GetFromId(uint16_t id) const;
  // Find a RegType, returns null if not found.
  const RegType* FindClass(ObjPtr<mirror::Class> klass, bool precise) const
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Insert a new class with a specified descriptor, must not already be in the cache.
  const RegType* InsertClass(const std::string_view& descriptor,
                             ObjPtr<mirror::Class> klass,
                             bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Get or insert a reg type for a description, klass, and precision.
  const RegType& FromClass(const char* descriptor, ObjPtr<mirror::Class> klass, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const ConstantType& FromCat1Const(int32_t value, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const ConstantType& FromCat2ConstLo(int32_t value, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const ConstantType& FromCat2ConstHi(int32_t value, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& FromDescriptor(Handle<mirror::ClassLoader> loader, const char* descriptor)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& FromUnresolvedMerge(const RegType& left,
                                     const RegType& right,
                                     MethodVerifier* verifier)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& FromUnresolvedSuperClass(const RegType& child)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Note: this should not be used outside of RegType::ClassJoin!
  const RegType& MakeUnresolvedReference() REQUIRES_SHARED(Locks::mutator_lock_);

  const ConstantType& Zero() REQUIRES_SHARED(Locks::mutator_lock_) {
    return FromCat1Const(0, true);
  }
  const ConstantType& One() REQUIRES_SHARED(Locks::mutator_lock_) {
    return FromCat1Const(1, true);
  }
  size_t GetCacheSize() {
    return entries_.size();
  }
  const BooleanType& Boolean() REQUIRES_SHARED(Locks::mutator_lock_);
  const ByteType& Byte() REQUIRES_SHARED(Locks::mutator_lock_);
  const CharType& Char() REQUIRES_SHARED(Locks::mutator_lock_);
  const ShortType& Short() REQUIRES_SHARED(Locks::mutator_lock_);
  const IntegerType& Integer() REQUIRES_SHARED(Locks::mutator_lock_);
  const FloatType& Float() REQUIRES_SHARED(Locks::mutator_lock_);
  const LongLoType& LongLo() REQUIRES_SHARED(Locks::mutator_lock_);
  const LongHiType& LongHi() REQUIRES_SHARED(Locks::mutator_lock_);
  const DoubleLoType& DoubleLo() REQUIRES_SHARED(Locks::mutator_lock_);
  const DoubleHiType& DoubleHi() REQUIRES_SHARED(Locks::mutator_lock_);
  const UndefinedType& Undefined() REQUIRES_SHARED(Locks::mutator_lock_);
  const ConflictType& Conflict();
  const NullType& Null();

  const PreciseReferenceType& JavaLangClass() REQUIRES_SHARED(Locks::mutator_lock_);
  const PreciseReferenceType& JavaLangString() REQUIRES_SHARED(Locks::mutator_lock_);
  const PreciseReferenceType& JavaLangInvokeMethodHandle() REQUIRES_SHARED(Locks::mutator_lock_);
  const PreciseReferenceType& JavaLangInvokeMethodType() REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& JavaLangThrowable() REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& JavaLangObject(bool precise) REQUIRES_SHARED(Locks::mutator_lock_);

  const UninitializedType& Uninitialized(const RegType& type, uint32_t allocation_pc)
      REQUIRES_SHARED(Locks::mutator_lock_);
  // Create an uninitialized 'this' argument for the given type.
  const UninitializedType& UninitializedThisArgument(const RegType& type)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& FromUninitialized(const RegType& uninit_type)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& ByteConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& CharConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& ShortConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& IntConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& PosByteConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const ImpreciseConstType& PosShortConstant() REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& GetComponentType(const RegType& array, Handle<mirror::ClassLoader> loader)
      REQUIRES_SHARED(Locks::mutator_lock_);
  void Dump(std::ostream& os) REQUIRES_SHARED(Locks::mutator_lock_);
  const RegType& RegTypeFromPrimitiveType(Primitive::Type) const;

  ClassLinker* GetClassLinker() const {
    return class_linker_;
  }

  Handle<mirror::Class> GetNullHandle() const {
    return null_handle_;
  }

  static constexpr int32_t kMinSmallConstant = -1;
  static constexpr int32_t kMaxSmallConstant = 4;
  static constexpr int32_t kNumSmallConstants = kMaxSmallConstant - kMinSmallConstant + 1;
  static constexpr size_t kNumPrimitivesAndSmallConstants = 13 + kNumSmallConstants;
  static constexpr int32_t kBooleanCacheId = kNumSmallConstants;
  static constexpr int32_t kByteCacheId = kNumSmallConstants + 1;
  static constexpr int32_t kShortCacheId = kNumSmallConstants + 2;
  static constexpr int32_t kCharCacheId = kNumSmallConstants + 3;
  static constexpr int32_t kIntCacheId = kNumSmallConstants + 4;
  static constexpr int32_t kLongLoCacheId = kNumSmallConstants + 5;
  static constexpr int32_t kLongHiCacheId = kNumSmallConstants + 6;
  static constexpr int32_t kFloatCacheId = kNumSmallConstants + 7;
  static constexpr int32_t kDoubleLoCacheId = kNumSmallConstants + 8;
  static constexpr int32_t kDoubleHiCacheId = kNumSmallConstants + 9;
  static constexpr int32_t kUndefinedCacheId = kNumSmallConstants + 10;
  static constexpr int32_t kConflictCacheId = kNumSmallConstants + 11;
  static constexpr int32_t kNullCacheId = kNumSmallConstants + 12;

 private:
  void FillPrimitiveAndSmallConstantTypes() REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<mirror::Class> ResolveClass(const char* descriptor, Handle<mirror::ClassLoader> loader)
      REQUIRES_SHARED(Locks::mutator_lock_);
  bool MatchDescriptor(size_t idx, const std::string_view& descriptor, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);
  const ConstantType& FromCat1NonSmallConstant(int32_t value, bool precise)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const RegType& From(Handle<mirror::ClassLoader> loader, const char* descriptor)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the pass in RegType.
  template <class RegTypeType>
  RegTypeType& AddEntry(RegTypeType* new_entry) REQUIRES_SHARED(Locks::mutator_lock_);

  // Add a string to the arena allocator so that it stays live for the lifetime of the
  // verifier and return a string view.
  std::string_view AddString(const std::string_view& str);

  // The actual storage for the RegTypes.
  ScopedArenaVector<const RegType*> entries_;

  // Fast lookup for quickly finding entries that have a matching class.
  ScopedArenaVector<std::pair<Handle<mirror::Class>, const RegType*>> klass_entries_;

  // Arena allocator.
  ScopedArenaAllocator& allocator_;

  // Handle scope containing classes.
  VariableSizedHandleScope handles_;
  ScopedNullHandle<mirror::Class> null_handle_;

  ClassLinker* class_linker_;

  // Whether or not we're allowed to load classes.
  const bool can_load_classes_;

  DISALLOW_COPY_AND_ASSIGN(RegTypeCache);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_REG_TYPE_CACHE_H_
