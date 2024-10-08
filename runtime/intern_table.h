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

#ifndef ART_RUNTIME_INTERN_TABLE_H_
#define ART_RUNTIME_INTERN_TABLE_H_

#include "base/dchecked_vector.h"
#include "base/gc_visited_arena_pool.h"
#include "base/hash_set.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc/weak_root_state.h"
#include "gc_root.h"

namespace art HIDDEN {

class IsMarkedVisitor;

namespace gc {
namespace space {
class ImageSpace;
}  // namespace space
}  // namespace gc

enum VisitRootFlags : uint8_t;

namespace linker {
class ImageWriter;
}  // namespace linker

namespace mirror {
class String;
}  // namespace mirror
class Transaction;

/**
 * Used to intern strings.
 *
 * There are actually two tables: one that holds strong references to its strings, and one that
 * holds weak references. The former is used for string literals, for which there is an effective
 * reference from the constant pool. The latter is used for strings interned at runtime via
 * String.intern. Some code (XML parsers being a prime example) relies on being able to intern
 * arbitrarily many strings for the duration of a parse without permanently increasing the memory
 * footprint.
 */
class InternTable {
 public:
  // Modified UTF-8-encoded string treated as UTF16.
  class Utf8String {
   public:
    Utf8String(uint32_t utf16_length, const char* utf8_data)
        : utf16_length_(utf16_length), utf8_data_(utf8_data) { }

    uint32_t GetHash() const { return Hash(utf16_length_, utf8_data_); }
    uint32_t GetUtf16Length() const { return utf16_length_; }
    const char* GetUtf8Data() const { return utf8_data_; }

    static uint32_t Hash(uint32_t utf16_length, const char* utf8_data);

   private:
    uint32_t utf16_length_;
    const char* utf8_data_;
  };

  class StringHash {
   public:
    // NO_THREAD_SAFETY_ANALYSIS: Used from unannotated `HashSet<>` functions.
    size_t operator()(const GcRoot<mirror::String>& root) const NO_THREAD_SAFETY_ANALYSIS;

    // Utf8String can be used for lookup. While we're passing the hash explicitly to all
    // `HashSet<>` functions, they `DCHECK()` the supplied hash against the hash we provide here.
    size_t operator()(const Utf8String& key) const {
      static_assert(std::is_same_v<uint32_t, decltype(key.GetHash())>);
      return key.GetHash();
    }
  };

  class StringEquals {
   public:
    // NO_THREAD_SAFETY_ANALYSIS: Used from unannotated `HashSet<>` functions.
    bool operator()(const GcRoot<mirror::String>& a, const GcRoot<mirror::String>& b) const
        NO_THREAD_SAFETY_ANALYSIS;

    // Utf8String can be used for lookup.
    // NO_THREAD_SAFETY_ANALYSIS: Used from unannotated `HashSet<>` functions.
    bool operator()(const GcRoot<mirror::String>& a, const Utf8String& b) const
        NO_THREAD_SAFETY_ANALYSIS;
  };

  class GcRootEmptyFn {
   public:
    void MakeEmpty(GcRoot<mirror::String>& item) const {
      item = GcRoot<mirror::String>();
    }
    bool IsEmpty(const GcRoot<mirror::String>& item) const {
      return item.IsNull();
    }
  };

  using UnorderedSet =
      HashSet<GcRoot<mirror::String>,
              GcRootEmptyFn,
              StringHash,
              StringEquals,
              GcRootArenaAllocator<GcRoot<mirror::String>, kAllocatorTagInternTable>>;

  EXPORT InternTable();

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  EXPORT ObjPtr<mirror::String> InternStrong(uint32_t utf16_length, const char* utf8_data)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  EXPORT ObjPtr<mirror::String> InternStrong(const char* utf8_data)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'strong' table. May cause thread suspension.
  EXPORT ObjPtr<mirror::String> InternStrong(ObjPtr<mirror::String> s)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'weak' table. May cause thread suspension.
  ObjPtr<mirror::String> InternWeak(const char* utf8_data) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  // Interns a potentially new string in the 'weak' table. May cause thread suspension.
  ObjPtr<mirror::String> InternWeak(ObjPtr<mirror::String> s) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Roles::uninterruptible_);

  void SweepInternTableWeaks(IsMarkedVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!Locks::intern_table_lock_);

  // Lookup a strong intern, returns null if not found.
  ObjPtr<mirror::String> LookupStrong(Thread* self, ObjPtr<mirror::String> s)
      REQUIRES(!Locks::intern_table_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<mirror::String> LookupStrong(Thread* self, uint32_t utf16_length, const char* utf8_data)
      REQUIRES(!Locks::intern_table_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<mirror::String> LookupStrongLocked(ObjPtr<mirror::String> s)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  // Lookup a weak intern, returns null if not found.
  ObjPtr<mirror::String> LookupWeak(Thread* self, ObjPtr<mirror::String> s)
      REQUIRES(!Locks::intern_table_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  ObjPtr<mirror::String> LookupWeakLocked(ObjPtr<mirror::String> s)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  // Total number of interned strings.
  EXPORT size_t Size() const REQUIRES(!Locks::intern_table_lock_);

  // Total number of weakly live interned strings.
  size_t StrongSize() const REQUIRES(!Locks::intern_table_lock_);

  // Total number of strongly live interned strings.
  size_t WeakSize() const REQUIRES(!Locks::intern_table_lock_);

  EXPORT void VisitRoots(RootVisitor* visitor, VisitRootFlags flags)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  // Visit all of the interns in the table.
  template <typename Visitor>
  void VisitInterns(const Visitor& visitor,
                    bool visit_boot_images,
                    bool visit_non_boot_images)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  // Count the number of intern strings in the table.
  size_t CountInterns(bool visit_boot_images, bool visit_non_boot_images) const
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  void DumpForSigQuit(std::ostream& os) const REQUIRES(!Locks::intern_table_lock_);

  void BroadcastForNewInterns();

  // Add all of the strings in the image's intern table into this intern table. This is required so
  // the intern table is correct.
  // The visitor arg type is UnorderedSet
  template <typename Visitor>
  void AddImageStringsToTable(gc::space::ImageSpace* image_space,
                              const Visitor& visitor)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  // Add a new intern table for inserting to, previous intern tables are still there but no
  // longer inserted into and ideally unmodified. This is done to prevent dirty pages.
  void AddNewTable()
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Locks::intern_table_lock_);

  // Change the weak root state. May broadcast to waiters.
  void ChangeWeakRootState(gc::WeakRootState new_state)
      REQUIRES(!Locks::intern_table_lock_);

 private:
  // Table which holds pre zygote and post zygote interned strings. There is one instance for
  // weak interns and strong interns.
  class Table {
   public:
    class InternalTable {
     public:
      InternalTable() = default;
      InternalTable(UnorderedSet&& set, bool is_boot_image)
          : set_(std::move(set)), is_boot_image_(is_boot_image) {}

      bool Empty() const {
        return set_.empty();
      }

      size_t Size() const {
        return set_.size();
      }

      bool IsBootImage() const {
        return is_boot_image_;
      }

     private:
      UnorderedSet set_;
      bool is_boot_image_ = false;

      friend class InternTable;
      friend class linker::ImageWriter;
      friend class Table;
      ART_FRIEND_TEST(InternTableTest, CrossHash);
    };

    Table();
    ObjPtr<mirror::String> Find(ObjPtr<mirror::String> s,
                                uint32_t hash,
                                size_t num_searched_frozen_tables = 0u)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    ObjPtr<mirror::String> Find(const Utf8String& string, uint32_t hash)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void Insert(ObjPtr<mirror::String> s, uint32_t hash)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void Remove(ObjPtr<mirror::String> s, uint32_t hash)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void VisitRoots(RootVisitor* visitor)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    void SweepWeaks(IsMarkedVisitor* visitor)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
    // Add a new intern table that will only be inserted into from now on.
    void AddNewTable() REQUIRES(Locks::intern_table_lock_);
    size_t Size() const REQUIRES(Locks::intern_table_lock_);
    // Read and add an intern table from ptr.
    // Tables read are inserted at the front of the table array. Only checks for conflicts in
    // debug builds. Returns how many bytes were read.
    // NO_THREAD_SAFETY_ANALYSIS for the visitor that may require locks.
    template <typename Visitor>
    size_t AddTableFromMemory(const uint8_t* ptr, const Visitor& visitor, bool is_boot_image)
        REQUIRES(!Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_);

   private:
    void SweepWeaks(UnorderedSet* set, IsMarkedVisitor* visitor)
        REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

    // Add a table to the front of the tables vector.
    void AddInternStrings(UnorderedSet&& intern_strings, bool is_boot_image)
        REQUIRES(Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_);

    // We call AddNewTable when we create the zygote to reduce private dirty pages caused by
    // modifying the zygote intern table. The back of table is modified when strings are interned.
    dchecked_vector<InternalTable> tables_;

    friend class InternTable;
    friend class linker::ImageWriter;
    ART_FRIEND_TEST(InternTableTest, CrossHash);
  };

  // Insert if non null, otherwise return null. Must be called holding the mutator lock.
  ObjPtr<mirror::String> Insert(ObjPtr<mirror::String> s,
                                uint32_t hash,
                                bool is_strong,
                                size_t num_searched_strong_frozen_tables = 0u)
      REQUIRES(!Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_);

  // Add a table from memory to the strong interns.
  template <typename Visitor>
  size_t AddTableFromMemory(const uint8_t* ptr, const Visitor& visitor, bool is_boot_image)
      REQUIRES(!Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_);

  // Note: Transaction rollback calls these helper functions directly.
  EXPORT ObjPtr<mirror::String> InsertStrong(ObjPtr<mirror::String> s, uint32_t hash)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  ObjPtr<mirror::String> InsertWeak(ObjPtr<mirror::String> s, uint32_t hash)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveStrong(ObjPtr<mirror::String> s, uint32_t hash)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);
  void RemoveWeak(ObjPtr<mirror::String> s, uint32_t hash)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(Locks::intern_table_lock_);

  // Change the weak root state. May broadcast to waiters.
  void ChangeWeakRootStateLocked(gc::WeakRootState new_state)
      REQUIRES(Locks::intern_table_lock_);

  // Wait until we can read weak roots.
  void WaitUntilAccessible(Thread* self)
      REQUIRES(Locks::intern_table_lock_) REQUIRES_SHARED(Locks::mutator_lock_);

  bool log_new_roots_ GUARDED_BY(Locks::intern_table_lock_);
  ConditionVariable weak_intern_condition_ GUARDED_BY(Locks::intern_table_lock_);
  // Since this contains (strong) roots, they need a read barrier to
  // enable concurrent intern table (strong) root scan. Do not
  // directly access the strings in it. Use functions that contain
  // read barriers.
  Table strong_interns_ GUARDED_BY(Locks::intern_table_lock_);
  dchecked_vector<GcRoot<mirror::String>> new_strong_intern_roots_
      GUARDED_BY(Locks::intern_table_lock_);
  // Since this contains (weak) roots, they need a read barrier. Do
  // not directly access the strings in it. Use functions that contain
  // read barriers.
  Table weak_interns_ GUARDED_BY(Locks::intern_table_lock_);
  // Weak root state, used for concurrent system weak processing and more.
  gc::WeakRootState weak_root_state_ GUARDED_BY(Locks::intern_table_lock_);

  friend class gc::space::ImageSpace;
  friend class linker::ImageWriter;
  friend class Transaction;
  ART_FRIEND_TEST(InternTableTest, CrossHash);
  DISALLOW_COPY_AND_ASSIGN(InternTable);
};

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_H_
