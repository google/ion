/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#ifndef ION_GFX_BUFFEROBJECT_H_
#define ION_GFX_BUFFEROBJECT_H_

#include "ion/base/datacontainer.h"
#include "ion/base/invalid.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/external/gtest/gunit_prod.h"  // For FRIEND_TEST().
#include "ion/gfx/resourceholder.h"
#include "ion/math/range.h"

namespace ion {
namespace gfx {
class BufferObject;
// Convenience typedef for shared pointer to a BufferObject.
using BufferObjectPtr = base::SharedPtr<BufferObject>;

// A BufferObject describes a generic array of data used, for example, to
// describe the vertices in a Shape or data retrieved from a framebuffer. It is
// a wrapper around a structure, but makes no assumptions about the structure
// itself, nor any assumptions about the input data (e.g. it assumes data is
// already in the correct format to send to the graphics card, and performs no
// normalization, clamping, etc.).
//
// For the purposes of BufferObject a data structure is composed of elements
// defined by a Spec (e.g., int, vec2f, mat4), each Spec has an SpecType (e.g.,
// int, float, float), and a certain number of components (e.g., 1, 2, 16). A
// spec may correspond to a shader attribute, a uniform, or some kind of pixel
// format, depending on how the buffer is used.
//
// There are three possible usage modes that give OpenGL hints as to how the
// data should be stored (from the OpenGL ES documentation):
//  - kDynamicDraw: The data store contents will be modified repeatedly and used
//                  many times.
//  - kStaticDraw: The data store contents will be modified once and used many
//                 times.
//  - kStreamDraw: The data store contents will be modified once and used at
//                 most a few times.
//
// A BufferObject can be bound to a number of targets, but only two targets
// are used for draw calls: kElementBuffer and kArrayBuffer. kElementBuffer
// means the data will be used as an element array defining indices, while
// kArrayBuffer means the data will be used for array data, such as vertices.
// BufferObjects are by default kArrayBuffers; see the IndexBuffer class for
// creating types of kElementBuffer to be used as index arrays.
//
// After a buffer's data has been set through SetData(), callers can modify
// sub-ranges of data through SetSubData(), or update the entire buffer's data
// with another call to SetData().
class ION_API BufferObject : public ResourceHolder {
 public:
  // Changes that affect the resource.
  enum Changes {
    kDataChanged = kNumBaseChanges,
    kSubDataChanged,
    kNumChanges
  };

  // The type of the components of a spec.
  enum ComponentType {
    kInvalid,  // An invalid spec.
    kByte,
    kUnsignedByte,
    kShort,
    kUnsignedShort,
    kInt,
    kUnsignedInt,
    kFloat,
    // These are necessary since each column of a matrix must be sent to OpenGL
    // separately and we must know the number of components in each column.
    kFloatMatrixColumn2,
    kFloatMatrixColumn3,
    kFloatMatrixColumn4
  };

  enum Target {
    kArrayBuffer,
    kElementBuffer,
    kCopyReadBuffer,
    kCopyWriteBuffer,
    kTransformFeedbackBuffer,
    kNumTargets
  };

  enum IndexedTarget {
    kIndexedTransformFeedbackBuffer,
    kNumIndexedTargets
  };

  enum UsageMode {
    kDynamicDraw,
    kStaticDraw,
    kStreamDraw,
  };

  // Specifies a destination byte range, read byte offset, and source
  // BufferObject or DataContainer for BufferSubData and CopyBufferSubData.
  struct BufferSubData {
    BufferSubData() : read_offset(0) {}

    BufferSubData(const math::Range1ui& range_in,
                  const base::DataContainerPtr& data_in)
        : range(range_in), data(data_in), read_offset(0) {
      DCHECK(data_in.Get());
    }
    BufferSubData(const BufferObjectPtr& src_in,
                  const math::Range1ui& range_in,
                  uint32 read_offset_in)
        : range(range_in), read_offset(read_offset_in), src(src_in) {}
    // See comment in .cc file.
    ~BufferSubData();

    // Destination byte range of copy.
    math::Range1ui range;
    // Source data for copy. If NULL source data is taken from src.
    base::DataContainerPtr data;
    // Read offset in bytes into data or src.
    uint32 read_offset;
    // Source BufferObject for CopySubData, NULL is interpreted as this
    // BufferObject.
    BufferObjectPtr src;
  };

  struct Spec {
    // Default constructor for STL.
    Spec()
        : component_count(0U),
          byte_offset(0U),
          type(kInvalid) {}

    Spec(const ComponentType type_in, const size_t count_in,
         const size_t offset_in)
        : component_count(count_in),
          byte_offset(offset_in),
          type(type_in) {}

    bool operator==(const Spec& other) const {
      return component_count == other.component_count &&
             byte_offset == other.byte_offset &&
             type == other.type;
    }

    // The number of components.
    size_t component_count;
    // The offset of the element defined by this Spec in the data type.
    size_t byte_offset;
    // The type of each component.
    ComponentType type;
  };

  // Creates a BufferObject.
  BufferObject();

  // Gets the buffer's initial bind target.
  Target GetInitialTarget() const { return initial_target_; }

  // Describes an element of an arbitrary datatype to the BufferObject. An
  // element is defined by its byte offset into the struct, its type, and the
  // number of components it contains.
  //
  // AddSpec() Returns a static index into the array of elements for this
  // BufferObject. The returned value is a unique (for this BufferObject),
  // static index into an array of Elements. The returned index is required by
  // the Get() functions, below. If count is > 4, then returns an invalid index
  // (base::kInvalidIndex). Since Specs are unique; attempting to add the same
  // Spec twice has no effect and the original index of the identical Spec is
  // returned.
  size_t AddSpec(const ComponentType type, const size_t component_count,
                 const size_t byte_offset);

  // Gets the Spec at index spec_index. If spec_index is invalid,
  // returns an InvalidReference.
  const Spec& GetSpec(const size_t element_index) const;

  // Gets the number of Specs in the BufferObject.
  size_t GetSpecCount() const { return specs_.size(); }

  // Sets data container, the size of the structure in bytes, and the number of
  // structures. The DataContainer will only be destroyed when the last client
  // ReferentPtr to the DataContainer goes away _and_ the BufferObject is
  // destroyed or a new DataContainer is set with SetData(). data may be NULL
  // in which case the BufferObject is sized to struct_size * count, but its
  // contents are undefined and it is expected that the BufferObject will
  // be populated later with SetSubData().
  void SetData(const base::DataContainerPtr& data, const size_t struct_size,
               const size_t count, UsageMode usage) {
    if (base::DataContainer* old_data = GetData().Get())
      old_data->RemoveReceiver(this);
    if (base::DataContainer* new_data = data.Get())
      new_data->AddReceiver(this);
    data_.Set(BufferData(data, struct_size, count, usage));
  }

  // Gets the data container.
  const base::DataContainerPtr& GetData() const { return data_.Get().data; }

  // Marks that the specified byte range of the BufferObject's data should be
  // updated with the passed data. This function is only meaningful if SetData()
  // has already been used, and may be called multiple times to update multiple
  // byte ranges.
  void SetSubData(const math::Range1ui& byte_range,
                  const base::DataContainerPtr& data) {
    if (!byte_range.IsEmpty() && data.Get() && data->GetData()) {
      sub_data_.push_back(BufferSubData(byte_range, data));
      // Set twice so that the bit can be flipped again on the next call.
      sub_data_changed_.Set(true);
      sub_data_changed_.Set(false);
    }
  }
  // Adds a byte range of data that should be copied from src to this
  // BufferObject.  read_offset specifies the byte offset within the src
  // BufferObject data from which to copy the data. dst_byte_range specifies the
  // destination range. The source and destination ranges should not overlap
  // if src == this. Note that all subdatas in src are applied to to src before
  // the copy to this BufferObject.
  void CopySubData(const BufferObjectPtr& src,
                   const math::Range1ui& dst_byte_range,
                   uint32 read_offset) {
    if (src.Get() && !dst_byte_range.IsEmpty()) {
      sub_data_.push_back(BufferSubData(
          // Don't keep ref to this in sub_data_ to avoid Ptr cycle.
          src.Get() == this ? BufferObjectPtr() : src,
          dst_byte_range, read_offset));
      // Set twice so that the bit can be flipped again on the next call.
      sub_data_changed_.Set(true);
      sub_data_changed_.Set(false);
    }
  }
  // Clears the vector of sub-data.
  void ClearSubData() const {
    sub_data_.clear();
  }
  // Returns all sub-data ranges; may be an empty vector.
  const base::AllocVector<BufferSubData>& GetSubData() const {
    return sub_data_;
  }

  // Returns the mapped data pointer of the buffer, which will be NULL if the
  // buffer has not been mapped with Renderer::MapBufferObjectData(Range)().
  void* GetMappedPointer() const {
    return mapped_data_.pointer;
  }

  // Gets the size of one structure, in bytes.
  size_t GetStructSize() const { return data_.Get().struct_size; }
  // Gets the number of structs in the buffer.
  size_t GetCount() const { return data_.Get().count; }
  // Gets the usage mode of the data.
  UsageMode GetUsageMode() const { return data_.Get().usage; }

 protected:
  // Creates a BufferObject with a particular target type. This constructor is
  // protected so that only derived classes can set the target.
  explicit BufferObject(Target target);

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~BufferObject() override;

 private:
  // Helper struct for storing arbitrary data.
  struct BufferData {
    // Default constructor for containers.
    BufferData()
        : data(nullptr),
          struct_size(0U),
          count(0U),
          usage(kStaticDraw) {}

    BufferData(const base::DataContainerPtr& data_in,
               const size_t struct_size_in,
               const size_t count_in, const UsageMode usage_in)
        : data(data_in),
          struct_size(struct_size_in),
          count(count_in),
          usage(usage_in) {}

    // Needed for Field::Set(). We always return true here because if the
    // client calls Set on the data field with the same pointer, size, and
    // count, we can't tell the difference. Instead, we assume that if the
    // client calls Set() on the data that it must have changed.
    inline bool operator !=(const BufferData& other) const {
      return true;
    }

    // The actual data stored as a strong reference.
    base::DataContainerPtr data;
    // The size of a single struct.
    size_t struct_size;
    // The number of structs.
    size_t count;
    // Data usage mode.
    UsageMode usage;
  };

  // Wrapper for mapped buffer data.
  struct MappedBufferData {
    enum DataSource {
      kGpuMapped,  // Data is mapped by GPU.
      kAllocated,  // Data is allocated from Allocator, needs free.
      kDataContainer  // Data comes from BufferObject's DataContainer, no free.
    };
    MappedBufferData()
        : range(math::Range1ui()),
          pointer(nullptr),
          data_source(base::InvalidEnumValue<
                      BufferObject::MappedBufferData::DataSource>()),
          read_only(true) {}
    // The range of data mapped.
    math::Range1ui range;
    // A pointer that is either client allocated or GPU mapped.
    void* pointer;
    // Indicates the source of data returned by MapBuffer.
    DataSource data_source;
    // Don't need to upload if read_only is true.
    bool read_only;
  };

  // Called when the DataContainer this depends on changes.
  void OnNotify(const base::Notifier* notifier) override;

  // Called by a Renderer to set mapped data.
  void SetMappedData(const math::Range1ui& range, void* pointer,
                     MappedBufferData::DataSource data_source, bool read_only) {
    mapped_data_.range = range;
    mapped_data_.pointer = pointer;
    mapped_data_.data_source = data_source;
    mapped_data_.read_only = read_only;
  }

  // Returns the mapped data struct. Called by a Renderer.
  const MappedBufferData& GetMappedData() const {
    return mapped_data_;
  }

  // Element storage.
  base::AllocVector<Spec> specs_;

  // The data.
  Field<BufferData> data_;

  // Initial bind target. Note that the buffer may be bound to other targets
  // depending on how it is used.
  const Target initial_target_;

  // Ranges of the BufferObject's data container that have been modified. It is
  // mutable so that it can be cleared even in const instances.
  mutable base::AllocVector<BufferSubData> sub_data_;
  // Whether any sub data has been added to the BufferObject. This, as opposed
  // to the above vector, is a Field so that clearing the sub-data does not
  // trigger a change bit.
  Field<bool> sub_data_changed_;

  // Buffer data that has been mapped from the graphics hardware or a
  // client-side pointer if the platform does not support mapped buffers. The
  // range is empty if the entire buffer is mapped.
  MappedBufferData mapped_data_;

  // Allow the Renderer to map and unmap buffer data.
  friend class Renderer;

  // Allows tests to set mapped data and get the mapped range.
  FRIEND_TEST(BufferObjectTest, MappedData);
  FRIEND_TEST(RendererTest, MappedBuffer);
};

// Structure for clients to use to encapsulate Elements.  This is passed to
// Attribute to link a BufferObjectElement with a shader attribute.
struct BufferObjectElement {
  // Default constructor for templates.
  BufferObjectElement()
      : buffer_object(nullptr),
        spec_index(0U) {}

  BufferObjectElement(const BufferObjectPtr& buffer_in,
                      const size_t spec_index_in)
      : buffer_object(buffer_in),
        spec_index(spec_index_in) {}

  bool operator==(const BufferObjectElement& other) const {
    return buffer_object == other.buffer_object &&
        spec_index == other.spec_index;
  }

  const BufferObjectPtr buffer_object;
  const size_t spec_index;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_BUFFEROBJECT_H_
