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

#include "ion/gfxutils/printer.h"

#include <algorithm>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>

#include "ion/base/array2.h"
#include "ion/base/enumhelper.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfx/uniformblock.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace gfxutils {

namespace {

using gfx::Attribute;
using gfx::AttributeArray;
using gfx::AttributeArrayPtr;
using gfx::CubeMapTexture;
using gfx::CubeMapTexturePtr;
using gfx::BufferObject;
using gfx::BufferObjectElement;
using gfx::BufferObjectPtr;
using gfx::Image;
using gfx::ImagePtr;
using gfx::IndexBuffer;
using gfx::IndexBufferPtr;
using gfx::Node;
using gfx::NodePtr;
using gfx::Sampler;
using gfx::SamplerPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
using gfx::ShaderProgram;
using gfx::ShaderProgramPtr;
using gfx::Shape;
using gfx::ShapePtr;
using gfx::StateTable;
using gfx::StateTablePtr;
using gfx::Texture;
using gfx::TextureBase;
using gfx::TexturePtr;
using gfx::Uniform;
using gfx::UniformBlock;
using gfx::UniformBlockPtr;

//-----------------------------------------------------------------------------
//
// Struct that allows unsigned values (masks) to be printed properly as hex.
//
//-----------------------------------------------------------------------------

struct Mask {
  explicit Mask(uint32 value_in) : value(value_in) {}
  const uint32 value;
};

std::ostream& operator<<(std::ostream& out, const Mask& mask) {
  return out << "0x" << std::hex << mask.value << std::dec;
}

//-----------------------------------------------------------------------------
//
// Struct that allows pointers to be printed, with NULL printed as "NULL".
//
//-----------------------------------------------------------------------------

struct Pointer {
  explicit Pointer(const void* pointer_in) : pointer(pointer_in) {}
  const void* pointer;
};

std::ostream& operator<<(std::ostream& out, const Pointer& p) {
  if (p.pointer)
    out << p.pointer;
  else
    out << "NULL";
  return out;
}

//-----------------------------------------------------------------------------
//
// Convenience functions.
//
//-----------------------------------------------------------------------------

// Helper that converts any type of value to a string, with a specialization
// for VectorBase (which has no << operator).
template <typename T> const std::string ValueToString(const T& value) {
  return base::ValueToString(value);
}
template <int Dimension, typename T>
const std::string ValueToString(const math::VectorBase<Dimension, T>& vec) {
  return base::ValueToString(
      static_cast<const math::Vector<Dimension, T>&>(vec));
}
// Overload for pointers.
template <typename T> const std::string ValueToString(const T* value) {
  return value ? base::ValueToString(value) : std::string("NULL");
}

// Prints data from a BufferObject. The ValueType is the type of the values in
// the buffer, while ValuePrintType is the type to print them as.  This
// distinction is necessary to keep 1-byte integers from printing as chars.
template<typename ValueType, typename ValuePrintType>
static void PrintBufferData(std::ostream& out,  // NOLINT
                            const char* data, const size_t num_components) {
  if (const ValueType* typed_data = reinterpret_cast<const ValueType*>(data)) {
    if (num_components == 1) {
        out << static_cast<ValuePrintType>(typed_data[0]);
    } else {
      out << "[";
      for (size_t i = 0; i < num_components; ++i) {
        out << static_cast<ValuePrintType>(typed_data[i]);
        if (i < num_components - 1)
          out << ", ";
      }
      out << "]";
    }
  } else {
    out << "[NULL]";
  }
}

// Prints matrix data in row-order.
template<typename T>
static void PrintMatrixBufferData(std::ostream& out,  // NOLINT
                                  const char* data,
                                  const size_t num_columns,
                                  const size_t num_components) {
  if (const T* typed_data = reinterpret_cast<const T*>(data)) {
    out << "[";
    for (size_t i = 0; i < num_components; ++i) {
      for (size_t j = 0; j < num_columns; ++j) {
        out << typed_data[i * num_columns + j];
        if (j < num_columns - 1)
          out << ", ";
      }
      if (i < num_components - 1)
        out << " | ";
    }
    out << "]";
  }
}

static void PrintBufferDataByType(std::ostream& out,  // NOLINT
                                  BufferObject::ComponentType type,
                                  const char* ptr, size_t count) {
  switch (type) {
    case BufferObject::kByte:
      PrintBufferData<int8, int32>(out, ptr, count);
      break;
    case BufferObject::kUnsignedByte:
      PrintBufferData<uint8, uint32>(out, ptr, count);
      break;
    case BufferObject::kShort:
      PrintBufferData<int16, int32>(out, ptr, count);
      break;
    case BufferObject::kUnsignedShort:
      PrintBufferData<uint16, uint32>(out, ptr, count);
      break;
    case BufferObject::kInt:
      PrintBufferData<int32, int32>(out, ptr, count);
      break;
    case BufferObject::kUnsignedInt:
      PrintBufferData<uint32, uint32>(out, ptr, count);
      break;
    case BufferObject::kFloat:
      PrintBufferData<float, float>(out, ptr, count);
      break;
    case BufferObject::kFloatMatrixColumn2:
      PrintMatrixBufferData<float>(out, ptr, 2U, count);
      break;
    case BufferObject::kFloatMatrixColumn3:
      PrintMatrixBufferData<float>(out, ptr, 3U, count);
      break;
    case BufferObject::kFloatMatrixColumn4:
      PrintMatrixBufferData<float>(out, ptr, 4U, count);
      break;
    case BufferObject::kInvalid:
    default:
#if !defined(ION_COVERAGE)  // COV_NF_START
      DCHECK(false) << "Invalid buffer component type " << type;
#endif  // COV_NF_END
      break;
  }
}

// Returns the number of vertices common to all enabled buffer attributes in an
// AttributeArray.
static size_t GetBufferAttributeVertexCount(const AttributeArray& aa) {
  size_t min_vertex_count = 0;
  bool is_first = true;
  const size_t attribute_count = aa.GetAttributeCount();
  for (size_t i = 0; i < attribute_count; ++i) {
    const Attribute& attribute = aa.GetAttribute(i);
    if (attribute.IsValid() && attribute.Is<BufferObjectElement>() &&
        aa.IsAttributeEnabled(i)) {
      const size_t vertex_count =
          attribute.GetValue<BufferObjectElement>().buffer_object->GetCount();
      if (is_first) {
        min_vertex_count = vertex_count;
        is_first = false;
      } else {
        min_vertex_count = std::min(min_vertex_count, vertex_count);
      }
    }
  }
  return min_vertex_count;
}

static const std::string GetBufferAttributeValue(const AttributeArray& aa,
                                                 size_t vertex_index) {
  std::ostringstream s;
  bool is_first = true;
  const size_t attribute_count = aa.GetAttributeCount();
  for (size_t i = 0; i < attribute_count; ++i) {
    const Attribute& a = aa.GetAttribute(i);
    if (a.IsValid() && a.Is<BufferObjectElement>() &&
        aa.IsAttributeEnabled(i)) {
      if (is_first) {
        is_first = false;
      } else {
        s << ", ";
      }

      // Get the BufferObject for this Element.
      const BufferObjectPtr& bo =
          a.GetValue<BufferObjectElement>().buffer_object;
      const size_t num_entries = bo->GetCount();
      if (vertex_index < num_entries) {
        // Get the BufferObject data. It will be a NULL pointer if it was
        // wiped.
        const size_t stride = bo->GetStructSize();
        const char* raw_data =
            static_cast<const char*>(bo->GetData()->GetData());

        const size_t spec_index = a.GetValue<BufferObjectElement>().spec_index;
        const BufferObject::Spec& spec = bo->GetSpec(spec_index);
        DCHECK(!base::IsInvalidReference(spec));
        const char* ptr =
            raw_data ? &raw_data[stride * vertex_index + spec.byte_offset]
                     : nullptr;
        PrintBufferDataByType(s, spec.type, ptr, spec.component_count);
      }
    }
  }
  return s.str();
}

//-----------------------------------------------------------------------------
//
// The Tree class holds an intermediate form of an Ion graph, making the actual
// output formatting much simpler.
//
//-----------------------------------------------------------------------------

class Tree {
 public:
  // A Table stores a multi-row value for a field (such as a Matrix). The first
  // column may be used for row labels, in which case has_label_column is true.
  class Table : public base::Array2<std::string> {
   public:
    Table(size_t num_columns, size_t num_rows, bool has_label_column)
        : base::Array2<std::string>(num_columns, num_rows),
          has_label_column_(has_label_column) {}

    bool HasLabelColumn() const { return has_label_column_; }

   private:
    bool has_label_column_;
  };

  // A StringField represents a field (name/value pair) in which the value has
  // been converted to a string; this is used for most fields.
  struct StringField {
    StringField(const std::string& name_in, const std::string& value_in)
        : name(name_in), value(value_in) {}
    std::string name;
    std::string value;
  };

  // A TableField represents a field in which the value is a Table.
  struct TableField {
    TableField(const std::string& name_in, const Table& table_in)
        : name(name_in), table(table_in) {}
    std::string name;
    Table table;
  };

  // An ObjectField represents a field in which the value is another Object;
  // the index of the Object within the Tree is specified.
  struct ObjectField {
    ObjectField(const std::string& name_in, size_t object_index_in)
        : name(name_in), object_index(object_index_in) {}
    std::string name;
    size_t object_index;  // Index of object in vector.
  };

  // An Object represents an item optionally containing fields and other
  // objects.
  struct Object {
    Object(const void* pointer_in, const std::string& type_in,
           const std::string& label_in, bool is_inside_field_in)
        : pointer(pointer_in), type(type_in),
          label(label_in), is_inside_field(is_inside_field_in),
          has_enable_field(false), is_enabled(false) {}
    const void* pointer;
    std::string type;
    std::string label;
    bool is_inside_field;
    bool has_enable_field;
    bool is_enabled;
    std::vector<StringField> string_fields;
    std::vector<TableField> table_fields;
    std::vector<ObjectField> object_fields;
    std::vector<size_t> child_object_indices;
  };

  Tree() {
    all_objects_.reserve(128);
    root_objects_.reserve(16);
    cur_objects_.reserve(16);
  }

  size_t BeginObject(const void* pointer, const std::string& type,
                     const std::string& label, bool is_inside_field) {
    const size_t index = all_objects_.size();
    all_objects_.push_back(Object(pointer, type, label, is_inside_field));

    // If the object is already inside a field, there's no need to add it to
    // another object.
    if (!is_inside_field) {
      if (cur_objects_.empty()) {
        root_objects_.push_back(index);
      } else {
        GetCurObject()->child_object_indices.push_back(index);
      }
    }

    cur_objects_.push_back(index);
    return index;
  }

  template <typename T>
  size_t BeginLabeledObject(const T* pointer, const std::string& type) {
    return BeginObject(pointer, type, pointer ? pointer->GetLabel() : "");
  }

  void EndObject() {
    DCHECK(!cur_objects_.empty());
    cur_objects_.pop_back();
  }

  // Adds a checkbox button for enabling/disabling a Node's visibility.
  void AddNodeButton(const std::string& name, const std::string& node_label,
                     const std::string& command, bool checked) {
    GetCurObject()->has_enable_field = true;
    GetCurObject()->is_enabled = checked;
  }

  // Adds a field as a StringField by first converting the given value to a
  // string; this should work for anything that ValueToString() can handle.
  template <typename T>
  void AddField(const std::string& name, const T& value) {
    AddStringField(name, ValueToString(value));
  }

  // For enum types which provide a specialization of
  // base::EnumHelper::EnumData, this method should be preferred.
  template <typename E>
  void AddEnumField(const std::string& name, E enumvalue) {
    AddStringField(name, base::EnumHelper::GetString(enumvalue));
  }

  void AddStringField(const std::string& name, const std::string& value) {
    GetCurObject()->string_fields.push_back(StringField(name, value));
  }

  void AddTableField(const std::string& name, const Table& table) {
    GetCurObject()->table_fields.push_back(TableField(name, table));
  }

  void AddObjectField(const std::string& name, size_t object_index) {
    GetCurObject()->object_fields.push_back(ObjectField(name, object_index));
  }

  const std::vector<size_t>& GetRootObjectIndices() const {
    return root_objects_;
  }

  const Object& GetObject(size_t index) const {
    DCHECK_LT(index, all_objects_.size());
    return all_objects_[index];
  }

 private:
  Object* GetCurObject() {
    DCHECK(!cur_objects_.empty());
    return &all_objects_[cur_objects_.back()];
  }

  std::vector<Object> all_objects_;   // All objects added to the Tree.
  std::vector<size_t> root_objects_;  // Indices of all root objects.
  std::vector<size_t> cur_objects_;   // Stack of indices of open objects.
};

//-----------------------------------------------------------------------------
//
// Helper class that creates a string from multiple inline fields, each of
// which is added as "name=value".
//
//-----------------------------------------------------------------------------

class MultiField {
 public:
  MultiField() : is_first_(true) {}

  // Adds a field of any serializable type.
  template <typename T>
  MultiField& Add(const std::string& name, const T& value) {
    return AddField(name, ValueToString(value));
  }

  // Special version for enums that specialize base::EnumHelper::EnumData.
  template <typename E>
  MultiField& AddEnum(const std::string& name, E enumvalue) {
    return AddField(name, base::EnumHelper::GetString(enumvalue));
  }

  // Special version for strings that should not be quoted.
  MultiField& AddString(const std::string& name, const std::string& value) {
    return AddField(name, value);
  }

  // Adds a value only if a condition is true.
  template <typename T>
  MultiField& AddIf(bool cond, const std::string& name, const T& value) {
    if (cond) {
      Add(name, value);
    }
    return *this;
  }

  // Returns the resulting string.
  const std::string Get() const { return out_.str(); }

 private:
  MultiField& AddField(const std::string& name, const std::string& value) {
    if (is_first_)
      is_first_ = false;
    else
      out_ << ", ";
    out_ << name << '=' << value;
    return *this;
  }

  std::ostringstream out_;
  bool is_first_;
};

//-----------------------------------------------------------------------------
//
// Tree helper functions.
//
//-----------------------------------------------------------------------------

// Returns a Tree::Table representing a math::Matrix.
template <int Dimension, typename T>
static const Tree::Table BuildMatrixTable(const math::Matrix<Dimension, T>& m) {
  Tree::Table table(Dimension, Dimension, false);
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col) {
      table.Set(col, row, ValueToString(m(row, col)));
    }
  }
  return table;
}

// Returns a Tree::Table representing the values in an IndexBuffer.
static const Tree::Table GetIndexBufferTable(
    const IndexBuffer& ib, const BufferObject::Spec& spec) {
  // Get the BufferObject data.
  const size_t stride = ib.GetStructSize();
  const char* raw_data = ib.GetData().Get() ?
                         static_cast<const char*>(ib.GetData()->GetData()) :
                         nullptr;
  const size_t index_count = ib.GetCount();

  // The table has this many columns of indices plus 1 for the label.
  static const size_t kNumColumns = 10U;
  const size_t num_rows = (index_count + kNumColumns - 1) / kNumColumns;
  Tree::Table table(1U + kNumColumns, num_rows, true);
  size_t cur_index = 0;
  for (size_t row = 0; row < num_rows; ++row) {
    // Label the row in the first column.
    const size_t last = std::min(cur_index + kNumColumns - 1, index_count - 1);
    table.Set(0, row,
              base::ValueToString(cur_index) + " - " + ValueToString(last));

    // Indices.
    std::ostringstream out;
    for (size_t col = 0; col < kNumColumns; ++col) {
      const char* ptr =
          raw_data ? &raw_data[stride * cur_index + spec.byte_offset] : nullptr;
      PrintBufferDataByType(out, spec.type, ptr, spec.component_count);
      table.Set(1U + col, row, out.str());
      out.str("");
      if (++cur_index >= index_count)
        break;
    }
  }
  return table;
}

//-----------------------------------------------------------------------------
//
// The TreeBuilder class builds a Tree from an Ion graph.
//
//-----------------------------------------------------------------------------

class TreeBuilder {
 public:
  TreeBuilder(bool address_printing_enabled,
              bool full_shape_printing_enabled)
      : address_printing_enabled_(address_printing_enabled),
        full_shape_printing_enabled_(full_shape_printing_enabled) {}
  ~TreeBuilder() {}

  const Tree& BuildTree(const Node& node) {
    AddNode(node);
    return tree_;
  }

 private:
  // Base helper class that begins and ends a Tree::Object within a scope.
  class ScopedObjectBase {
   public:
    ScopedObjectBase(Tree* tree, const void* object, const std::string& type,
                     const std::string& label, bool is_inside_field)
        : tree_(tree),
          object_index_(
              tree_->BeginObject(object, type, label, is_inside_field)) {}
    ~ScopedObjectBase() { tree_->EndObject(); }
    size_t GetIndex() const { return object_index_; }

   private:
    Tree* tree_;
    const size_t object_index_;
  };

  // Scoped object for any type that supports GetLabel().
  template <typename T> class ScopedLabeledObject : public ScopedObjectBase {
   public:
    ScopedLabeledObject(Tree* tree, const T* object, const std::string& type,
                        bool is_inside_field)
        : ScopedObjectBase(tree, object, type,
                           object ? object->GetLabel() : std::string(""),
                           is_inside_field) {}
  };

  // Unlabeled scoped object.
  class ScopedObject : public ScopedObjectBase {
   public:
    ScopedObject(Tree* tree, const void* object, const std::string& type)
        : ScopedObjectBase(tree, object, type, "", false) {}
  };

  // This is used for a container object that does not correspond to a real Ion
  // object.
  class ContainerObject : public ScopedObjectBase {
   public:
    // The empty type indicates that this is a container object.
    explicit ContainerObject(Tree* tree)
        : ScopedObjectBase(tree, nullptr, "", "", true) {}
  };

  // Each of these functions adds an Object, fields, or other type to the
  // currently-open Object in the Tree. A return type of size_t indicates that
  // the function returns the index of the added Object within the Tree.
  void AddNode(const Node& node);
  void AddStateTable(const StateTable& st);
  void AddImageFields(const Image& image, const char* face);
  size_t AddCubeMapTexture(const CubeMapTexture* texture);
  size_t AddTexture(const Texture* texture);
  void AddTextureBaseFields(const TextureBase* texture);
  size_t AddSampler(const Sampler* sampler);
  void AddShape(const Shape& shape);
  void AddUniform(const Uniform& uniform);
  void AddUniformBlock(const UniformBlock* block);
  void AddUniformValueField(const Uniform& uniform);
  void AddAttributeArray(const AttributeArray& aa, bool add_contents);
  void AddAttribute(const Attribute& attribute, bool is_enabled);
  void AddNonbufferAttributeValueField(const Attribute& attribute);
  void AddBufferAttributeValues(const AttributeArray& aa);

  Tree tree_;
  bool address_printing_enabled_;
  bool full_shape_printing_enabled_;
  std::set<const AttributeArray*> added_attribute_arrays_;
};

void TreeBuilder::AddNode(const Node& node) {
  ScopedLabeledObject<Node> obj(&tree_, &node, "Node", false);

  tree_.AddNodeButton("Enabled", node.GetLabel(), "Enable", node.IsEnabled());

  // Shader program.
  if (const ShaderProgram* program = node.GetShaderProgram().Get())
    tree_.AddField("Shader ID", program->GetLabel());

  // Add state table.
  if (const StateTable* st = node.GetStateTable().Get())
    AddStateTable(*st);

  // Add all uniforms.
  const base::AllocVector<Uniform>& uniforms = node.GetUniforms();
  for (size_t i = 0; i < uniforms.size(); ++i)
    AddUniform(uniforms[i]);

  // Add all uniform blocks.
  const base::AllocVector<UniformBlockPtr>& uniform_blocks =
      node.GetUniformBlocks();
  for (size_t i = 0; i < uniform_blocks.size(); ++i)
    AddUniformBlock(uniform_blocks[i].Get());

  // Add all shapes.
  const base::AllocVector<ShapePtr>& shapes = node.GetShapes();
  for (size_t i = 0; i < shapes.size(); ++i)
    AddShape(*shapes[i]);

  // Recurse on children.
  const base::AllocVector<NodePtr>& children = node.GetChildren();
  for (size_t i = 0; i < children.size(); ++i)
    AddNode(*children[i]);
}

void TreeBuilder::AddStateTable(const StateTable& st) {
  ScopedObject obj(&tree_, &st, "StateTable");

  // Capabilities.
  const int num_capabilities = StateTable::GetCapabilityCount();
  for (int i = 0; i < num_capabilities; ++i) {
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (st.IsCapabilitySet(cap))
      tree_.AddField(StateTable::GetEnumString(cap), st.IsEnabled(cap));
  }

  // Values.
  if (st.IsValueSet(StateTable::kBlendColorValue))
    tree_.AddField("Blend Color", st.GetBlendColor());
  if (st.IsValueSet(StateTable::kBlendEquationsValue)) {
    tree_.AddStringField(
        "Blend Equations",
        MultiField()
        .AddEnum("RGB", st.GetRgbBlendEquation())
        .AddEnum("Alpha", st.GetAlphaBlendEquation())
        .Get());
  }
  if (st.IsValueSet(StateTable::kBlendFunctionsValue)) {
    tree_.AddStringField(
        "Blend Functions",
        MultiField()
        .AddEnum("RGB-src", st.GetRgbBlendFunctionSourceFactor())
        .AddEnum("RGB-dest", st.GetRgbBlendFunctionDestinationFactor())
        .AddEnum("Alpha-src", st.GetAlphaBlendFunctionSourceFactor())
        .AddEnum("Alpha-dest", st.GetAlphaBlendFunctionDestinationFactor())
        .Get());
  }
  if (st.IsValueSet(StateTable::kClearColorValue))
    tree_.AddField("Clear Color", st.GetClearColor());
  if (st.IsValueSet(StateTable::kColorWriteMasksValue)) {
    tree_.AddStringField(
        "Color Write Masks",
        MultiField()
        .Add("R", st.GetRedColorWriteMask())
        .Add("G", st.GetGreenColorWriteMask())
        .Add("B", st.GetBlueColorWriteMask())
        .Add("A", st.GetAlphaColorWriteMask())
        .Get());
  }
  if (st.IsValueSet(StateTable::kCullFaceModeValue))
    tree_.AddEnumField("Cull Face Mode", st.GetCullFaceMode());
  if (st.IsValueSet(StateTable::kFrontFaceModeValue))
    tree_.AddEnumField("Front Face Mode", st.GetFrontFaceMode());
  if (st.IsValueSet(StateTable::kClearDepthValue))
    tree_.AddField("Clear Depth Value", st.GetClearDepthValue());
  if (st.IsValueSet(StateTable::kDepthFunctionValue))
    tree_.AddEnumField("Depth Function", st.GetDepthFunction());
  if (st.IsValueSet(StateTable::kDepthRangeValue))
    tree_.AddField("Depth Range", st.GetDepthRange());
  if (st.IsValueSet(StateTable::kDepthWriteMaskValue))
    tree_.AddField("Depth Write Mask", st.GetDepthWriteMask());
  if (st.IsValueSet(StateTable::kHintsValue))
    tree_.AddEnumField("Generate Mipmap Hint",
                       st.GetHint(StateTable::kGenerateMipmapHint));
  if (st.IsValueSet(StateTable::kLineWidthValue))
    tree_.AddField("Line Width", st.GetLineWidth());
  if (st.IsValueSet(StateTable::kPolygonOffsetValue)) {
    tree_.AddStringField(
        "Polygon Offset",
        MultiField()
        .Add("Factor", st.GetPolygonOffsetFactor())
        .Add("Units", st.GetPolygonOffsetUnits())
        .Get());
  }
  if (st.IsValueSet(StateTable::kSampleCoverageValue)) {
    tree_.AddStringField(
        "Sample Coverage",
        MultiField()
        .Add("Value", st.GetSampleCoverageValue())
        .Add("Inverted", st.IsSampleCoverageInverted())
        .Get());
  }
  if (st.IsValueSet(StateTable::kScissorBoxValue))
    tree_.AddField("Scissor Box", st.GetScissorBox());
  if (st.IsValueSet(StateTable::kStencilFunctionsValue)) {
    tree_.AddStringField(
        "Stencil Functions",
        MultiField()
        .AddEnum("FFunc", st.GetFrontStencilFunction())
        .Add("FRef", st.GetFrontStencilReferenceValue())
        .Add("FMask", Mask(st.GetFrontStencilMask()))
        .AddEnum("BFunc", st.GetBackStencilFunction())
        .Add("BRef", st.GetBackStencilReferenceValue())
        .Add("BMask", Mask(st.GetBackStencilMask()))
        .Get());
  }
  if (st.IsValueSet(StateTable::kStencilOperationsValue)) {
    tree_.AddStringField(
        "Stencil Operations",
        MultiField()
        .AddEnum("FFail", st.GetFrontStencilFailOperation())
        .AddEnum("FDFail", st.GetFrontStencilDepthFailOperation())
        .AddEnum("FPass", st.GetFrontStencilPassOperation())
        .AddEnum("BFail", st.GetBackStencilFailOperation())
        .AddEnum("BDFail", st.GetBackStencilDepthFailOperation())
        .AddEnum("BPass", st.GetBackStencilPassOperation())
        .Get());
  }
  if (st.IsValueSet(StateTable::kClearStencilValue))
    tree_.AddField("Clear Stencil Value", st.GetClearStencilValue());
  if (st.IsValueSet(StateTable::kStencilWriteMasksValue)) {
    tree_.AddStringField(
        "Stencil Write Masks",
        MultiField()
        .Add("F", Mask(st.GetFrontStencilWriteMask()))
        .Add("B", Mask(st.GetBackStencilWriteMask()))
        .Get());
  }
  if (st.IsValueSet(StateTable::kViewportValue))
    tree_.AddField("Viewport", st.GetViewport());
}

void TreeBuilder::AddImageFields(const Image& image, const char* face) {
  tree_.AddStringField(
      "Image",
      MultiField()
          .AddIf(address_printing_enabled_, "Address", &image)
          .AddString("Face", face)
          .AddString("Format", Image::GetFormatString(image.GetFormat()))
          .Add("Width", image.GetWidth())
          .Add("Height", image.GetHeight())
          .Add("Depth", image.GetDepth())
          .AddEnum("Type", image.GetType())
          .AddEnum("Dimensions", image.GetDimensions())
          .Get());
}

size_t TreeBuilder::AddCubeMapTexture(const CubeMapTexture* texture) {
  ScopedLabeledObject<CubeMapTexture> cmt(
      &tree_, texture, "CubeMapTexture", true);
  if (texture) {
    for (int i = 0; i < 6; ++i) {
      const CubeMapTexture::CubeFace face =
          static_cast<CubeMapTexture::CubeFace>(i);
      if (texture->HasImage(face, 0U)) {
        const Image& image = *texture->GetImage(face, 0U);
        AddImageFields(image, base::EnumHelper::GetString(face));
      }
    }
    AddTextureBaseFields(texture);
  }
  return cmt.GetIndex();
}

size_t TreeBuilder::AddTexture(const Texture* texture) {
  ScopedLabeledObject<Texture> t(&tree_, texture, "Texture", true);
  if (texture) {
    if (texture->HasImage(0U)) {
      const Image& image = *texture->GetImage(0U);
      AddImageFields(image, "None");
    }
    AddTextureBaseFields(texture);
  }
  return t.GetIndex();
}

void TreeBuilder::AddTextureBaseFields(const TextureBase* texture) {
  tree_.AddField(
      "Level range",
      math::Range1i(texture->GetBaseLevel(), texture->GetMaxLevel()));
  tree_.AddStringField(
      "Multisampling",
      MultiField()
      .Add("Samples", texture->GetMultisampleSamples())
      .Add("Fixed sample locations",
               texture->IsMultisampleFixedSampleLocations())
      .Get());
  tree_.AddStringField(
      "Swizzles",
      MultiField()
      .AddEnum("R", texture->GetSwizzleRed())
      .AddEnum("G", texture->GetSwizzleGreen())
      .AddEnum("B", texture->GetSwizzleBlue())
      .AddEnum("A", texture->GetSwizzleAlpha())
      .Get());
  tree_.AddObjectField("Sampler", AddSampler(texture->GetSampler().Get()));
}

size_t TreeBuilder::AddSampler(const Sampler* sampler) {
  ScopedLabeledObject<Sampler> s(&tree_, sampler, "Sampler", true);
  if (sampler) {
    tree_.AddField("Autogenerating mipmaps",
        sampler->IsAutogenerateMipmapsEnabled());
    tree_.AddEnumField("Texture compare mode", sampler->GetCompareMode());
    tree_.AddEnumField("Texture compare function",
        sampler->GetCompareFunction());
    tree_.AddEnumField("MinFilter mode", sampler->GetMinFilter());
    tree_.AddEnumField("MagFilter mode", sampler->GetMagFilter());
    tree_.AddField("Level-of-detail range",
        math::Range1f(sampler->GetMinLod(), sampler->GetMaxLod()));
    tree_.AddStringField(
        "Wrap modes",
        MultiField()
        .AddEnum("R", sampler->GetWrapR())
        .AddEnum("S", sampler->GetWrapS())
        .AddEnum("T", sampler->GetWrapT())
        .Get());
  }
  return s.GetIndex();
}

void TreeBuilder::AddShape(const Shape& shape) {
  ScopedLabeledObject<Shape> s(&tree_, &shape, "Shape", false);

  tree_.AddEnumField("Primitive Type", shape.GetPrimitiveType());

  if (const AttributeArray* aa = shape.GetAttributeArray().Get()) {
    const bool was_added = added_attribute_arrays_.count(aa);
    AddAttributeArray(*aa, !was_added);
    if (!was_added)
      added_attribute_arrays_.insert(aa);
  }

  if (const size_t range_count = shape.GetVertexRangeCount()) {
    tree_.AddField("# Vertex Ranges", range_count);
    for (size_t i = 0; i < range_count; ++i) {
      tree_.AddStringField(
          std::string("Range ") + base::ValueToString(i),
          MultiField()
          .Add("Enabled", shape.IsVertexRangeEnabled(i))
          .Add("Range", shape.GetVertexRange(i))
          .Get());
    }
  }

  if (IndexBuffer* ib = shape.GetIndexBuffer().Get()) {
    ScopedLabeledObject<IndexBuffer> si(&tree_, ib, "IndexBuffer", false);
    if (full_shape_printing_enabled_) {
      // IndexBuffers must have exactly one spec.
      DCHECK_EQ(ib->GetSpecCount(), 1U);
      const BufferObject::Spec& spec = ib->GetSpec(0);
      DCHECK(!base::IsInvalidReference(spec));
      tree_.AddEnumField("Type", spec.type);
      tree_.AddEnumField("Target", ib->GetInitialTarget());
      tree_.AddTableField("Indices", GetIndexBufferTable(*ib, spec));
    }
  }
}

void TreeBuilder::AddUniform(const Uniform& uniform) {
  DCHECK(uniform.IsValid());
  ScopedObject u(&tree_, &uniform, "Uniform");

  const ShaderInputRegistry::Spec<Uniform>& spec =
      *uniform.GetRegistry().GetSpec(uniform);
  tree_.AddField("Name", spec.name);
  tree_.AddStringField("Type", Uniform::GetValueTypeName(spec.value_type));
  AddUniformValueField(uniform);
}

void TreeBuilder::AddUniformBlock(const UniformBlock* block) {
  ScopedLabeledObject<UniformBlock> u(&tree_, block, "UniformBlock", false);
  if (block) {
    const base::AllocVector<Uniform>& uniforms = block->GetUniforms();
    tree_.AddField("Enabled", block->IsEnabled());
    for (size_t i = 0; i < uniforms.size(); ++i)
      AddUniform(uniforms[i]);
  }
}

void TreeBuilder::AddUniformValueField(const Uniform& uniform) {
  DCHECK(uniform.IsValid());
  const std::string& name("Value");
  switch (uniform.GetType()) {
    case gfx::kFloatUniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<float>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<float>());
      }
      break;
    case gfx::kIntUniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<int>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<int>());
      }
      break;
    case gfx::kUnsignedIntUniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<uint32>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<uint32>());
      }
      break;
    case gfx::kCubeMapTextureUniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i) {
          tree_.AddObjectField(
              name + " " + base::ValueToString(i),
              AddCubeMapTexture(uniform.GetValueAt<CubeMapTexturePtr>(i)
                                    .Get()));
        }
      } else {
        tree_.AddObjectField(
            name,
            AddCubeMapTexture(uniform.GetValue<CubeMapTexturePtr>().Get()));
      }
      break;
    case gfx::kTextureUniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i) {
          tree_.AddObjectField(
              name + " " + base::ValueToString(i),
              AddTexture(uniform.GetValueAt<TexturePtr>(i).Get()));
        }
      } else {
        tree_.AddObjectField(name,
                             AddTexture(uniform.GetValue<TexturePtr>().Get()));
      }
      break;
    case gfx::kFloatVector2Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase2f>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase2f>());
      }
      break;
    case gfx::kFloatVector3Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase3f>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase3f>());
      }
      break;
    case gfx::kFloatVector4Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase4f>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase4f>());
      }
      break;
    case gfx::kIntVector2Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase2i>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase2i>());
      }
      break;
    case gfx::kIntVector3Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase3i>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase3i>());
      }
      break;
    case gfx::kIntVector4Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase4i>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase4i>());
      }
      break;
    case gfx::kUnsignedIntVector2Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase2ui>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase2ui>());
      }
      break;
    case gfx::kUnsignedIntVector3Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase3ui>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase3ui>());
      }
      break;
    case gfx::kUnsignedIntVector4Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddField(name + " " + base::ValueToString(i),
                         uniform.GetValueAt<math::VectorBase4ui>(i));
      } else {
        tree_.AddField(name, uniform.GetValue<math::VectorBase4ui>());
      }
      break;
    case gfx::kMatrix2x2Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddTableField(
              name + " " + base::ValueToString(i),
              BuildMatrixTable(uniform.GetValueAt<math::Matrix2f>(i)));
      } else {
        tree_.AddTableField(
            name, BuildMatrixTable(uniform.GetValue<math::Matrix2f>()));
      }
      break;
    case gfx::kMatrix3x3Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddTableField(
              name + " " + base::ValueToString(i),
              BuildMatrixTable(uniform.GetValueAt<math::Matrix3f>(i)));
      } else {
        tree_.AddTableField(
            name, BuildMatrixTable(uniform.GetValue<math::Matrix3f>()));
      }
      break;
    case gfx::kMatrix4x4Uniform:
      if (uniform.GetCount()) {
        for (size_t i = 0; i < uniform.GetCount(); ++i)
          tree_.AddTableField(
              name + " " + base::ValueToString(i),
              BuildMatrixTable(uniform.GetValueAt<math::Matrix4f>(i)));
      } else {
        tree_.AddTableField(
            name, BuildMatrixTable(uniform.GetValue<math::Matrix4f>()));
      }
      break;
#if !defined(ION_COVERAGE)  // COV_NF_START
    default:
      DCHECK(false) << "Invalid uniform type " << uniform.GetType();
      break;
#endif  // COV_NF_END
  }
}

void TreeBuilder::AddAttributeArray(const AttributeArray& aa,
                                    bool add_contents) {
  ScopedLabeledObject<AttributeArray> a(&tree_, &aa, "AttributeArray", false);
  const size_t attribute_count = aa.GetAttributeCount();
  if (add_contents && attribute_count) {
    // Add all non-buffer attributes, including their values.
    for (size_t i = 0; i < attribute_count; ++i) {
      const Attribute& attribute = aa.GetAttribute(i);
      if (attribute.IsValid() && !attribute.Is<BufferObjectElement>())
        AddAttribute(attribute, aa.IsAttributeEnabled(i));
    }

    // Add buffer attributes without their values.
    for (size_t i = 0; i < attribute_count; ++i) {
      const Attribute& attribute = aa.GetAttribute(i);
      if (attribute.IsValid() && attribute.Is<BufferObjectElement>())
        AddAttribute(attribute, aa.IsAttributeEnabled(i));
    }

    // Now add buffer attribute values if requested.
    if (full_shape_printing_enabled_)
      AddBufferAttributeValues(aa);
  }
}

void TreeBuilder::AddAttribute(const Attribute& attribute, bool is_enabled) {
  DCHECK(attribute.IsValid());
  const bool is_buffer_attribute = attribute.Is<BufferObjectElement>();
  const std::string type_name(is_buffer_attribute ? "(Buffer)" : "(Nonbuffer)");

  ScopedObject a(&tree_, &attribute, std::string("Attribute ") + type_name);

  const ShaderInputRegistry::Spec<Attribute>& spec =
      *attribute.GetRegistry().GetSpec(attribute);
  tree_.AddField("Name", spec.name);
  tree_.AddField("Enabled", is_enabled);
  if (is_buffer_attribute) {
    tree_.AddField("Normalized", attribute.IsFixedPointNormalized());
    if (is_enabled) {
      // Add the name of the buffer if it has one.
      const BufferObjectPtr& vb =
          attribute.GetValue<BufferObjectElement>().buffer_object;
      const std::string& label = vb->GetLabel();
      if (!label.empty())
        tree_.AddField("Buffer", label);
    }
    // Buffer attribute values are added elsewhere.
  } else {
    // Always add values for enabled nonbuffer attributes.
    AddNonbufferAttributeValueField(attribute);
  }
}

void TreeBuilder::AddNonbufferAttributeValueField(const Attribute& attribute) {
  DCHECK(attribute.IsValid());
  const std::string& name("Value");
  switch (attribute.GetType()) {
    case gfx::kFloatAttribute:
      tree_.AddField(name, attribute.GetValue<float>());
      break;
    case gfx::kFloatVector2Attribute:
      tree_.AddField(name, attribute.GetValue<math::VectorBase2f>());
      break;
    case gfx::kFloatVector3Attribute:
      tree_.AddField(name, attribute.GetValue<math::VectorBase3f>());
      break;
    case gfx::kFloatVector4Attribute:
      tree_.AddField(name, attribute.GetValue<math::VectorBase4f>());
      break;
    case gfx::kFloatMatrix2x2Attribute:
      tree_.AddTableField(
          name, BuildMatrixTable(attribute.GetValue<math::Matrix2f>()));
      break;
    case gfx::kFloatMatrix3x3Attribute:
      tree_.AddTableField(
          name, BuildMatrixTable(attribute.GetValue<math::Matrix3f>()));
      break;
    case gfx::kFloatMatrix4x4Attribute:
      tree_.AddTableField(
          name, BuildMatrixTable(attribute.GetValue<math::Matrix4f>()));
      break;
    default:
#if !defined(ION_COVERAGE)  // COV_NF_START
      DCHECK(false) << "Invalid nonbuffer attribute type "
                    << attribute.GetType();
#endif  // COV_NF_END
      break;
  }
}

void TreeBuilder::AddBufferAttributeValues(const AttributeArray& aa) {
  if (const size_t vertex_count = GetBufferAttributeVertexCount(aa)) {
    size_t object_index;
    {
      ContainerObject co(&tree_);
      for (size_t i = 0; i < vertex_count; ++i) {
        tree_.AddStringField(std::string("v ") + base::ValueToString(i),
                             GetBufferAttributeValue(aa, i));
      }
      object_index = co.GetIndex();
    }
    tree_.AddObjectField("Buffer Values", object_index);
  }
}

//-----------------------------------------------------------------------------
//
// The TextTreePrinter class prints a Tree in text format.
//
//-----------------------------------------------------------------------------

class TextTreePrinter {
 public:
  TextTreePrinter(std::ostream& out, const Tree& tree,  // NOLINT
                  bool address_printing_enabled)
      : out_(out),
        tree_(tree),
        address_printing_enabled_(address_printing_enabled),
        indent_level_(0) {}
  void Print();

 private:
  void PrintObject(size_t object_index);
  void PrintObjectHeader(const Tree::Object& object);
  void PrintObjectField(const Tree::ObjectField& field);
  void PrintStringField(const Tree::StringField& field);
  void PrintTableField(const Tree::TableField& field);
  void PrintLabeledTable(const Tree::Table& table, size_t extra_indent);
  void PrintUnlabeledTable(const Tree::Table& table, size_t extra_indent);

  // Prints the proper indentation for the current line.
  void Indent() { IndentExtra(0); }

  // Same as Indent(), but adds extra indentation.
  void IndentExtra(size_t extra) {
    const size_t num_spaces = 2 * indent_level_ + extra;
    for (size_t i = 0; i < num_spaces; ++i)
      out_ << ' ';
  }

  std::ostream& out_;
  const Tree& tree_;
  bool address_printing_enabled_;
  size_t indent_level_;
};

void TextTreePrinter::Print() {
  const std::vector<size_t>& roots = tree_.GetRootObjectIndices();
  for (size_t i = 0; i < roots.size(); ++i)
    PrintObject(roots[i]);
}

void TextTreePrinter::PrintObject(size_t object_index) {
  const Tree::Object& object = tree_.GetObject(object_index);

  // Object header.
  PrintObjectHeader(object);
  ++indent_level_;

  // Fields.
  if (object.has_enable_field) {
    Tree::StringField enabled_state("Enabled",
                                    object.is_enabled ? "true" : "false");
    PrintStringField(enabled_state);
  }
  for (size_t i = 0; i < object.string_fields.size(); ++i)
    PrintStringField(object.string_fields[i]);
  for (size_t i = 0; i < object.table_fields.size(); ++i)
    PrintTableField(object.table_fields[i]);
  for (size_t i = 0; i < object.object_fields.size(); ++i)
    PrintObjectField(object.object_fields[i]);

  // Child objects.
  for (size_t i = 0; i < object.child_object_indices.size(); ++i)
    PrintObject(object.child_object_indices[i]);

  // Object footer.
  --indent_level_;
  Indent();
  out_ << "}\n";
}

void TextTreePrinter::PrintObjectHeader(const Tree::Object& object) {
  if (!object.is_inside_field)
    Indent();

  // If the type is empty, this is just a container, so no need to print
  // anything but the open brace.
  if (!object.type.empty()) {
    out_ << "ION " << object.type << ' ';
    if (!object.label.empty())
      out_ << "\"" << object.label << "\" ";
    if (address_printing_enabled_)
      out_ << '[' << Pointer(object.pointer) << "] ";
  }
  out_ << "{\n";
}

void TextTreePrinter::PrintObjectField(const Tree::ObjectField& field) {
  Indent();
  out_ << field.name << ": ";
  PrintObject(field.object_index);
}

void TextTreePrinter::PrintStringField(const Tree::StringField& field) {
  Indent();
  out_ << field.name << ": " << field.value << "\n";
}

void TextTreePrinter::PrintTableField(const Tree::TableField& field) {
  Indent();
  out_ << field.name << ": ";
  const size_t extra_indent = field.name.size() + 3U;  // For ": [".

  const Tree::Table& table = field.table;
  out_ << '[';
  if (table.GetSize()) {
    if (table.HasLabelColumn())
      PrintLabeledTable(table, extra_indent);
    else
      PrintUnlabeledTable(table, extra_indent);
  }
  out_ << "]\n";
}

void TextTreePrinter::PrintLabeledTable(const Tree::Table& table,
                                        size_t extra_indent) {
  const size_t num_rows = table.GetHeight();
  const size_t num_columns = table.GetWidth();
  for (size_t row = 0; row < num_rows; ++row) {
    if (row > 0) {
      out_ << ",\n";
      IndentExtra(extra_indent);
    }
    for (size_t col = 0; col < num_columns; ++col) {
      const std::string& cell = table.Get(col, row);
      if (col == 0) {  // Label column.
        out_ << cell << ": ";
      } else if (!cell.empty()) {
        if (col > 1)
          out_ << ", ";
        out_ << cell;
      }
    }
  }
}

void TextTreePrinter::PrintUnlabeledTable(const Tree::Table& table,
                                          size_t extra_indent) {
  const size_t num_rows = table.GetHeight();
  const size_t num_columns = table.GetWidth();
  for (size_t row = 0; row < num_rows; ++row) {
    if (row == 0) {
      out_ << '[';
    } else {
      out_ << '\n';
      IndentExtra(extra_indent);
      out_ << '[';
    }
    for (size_t col = 0; col < num_columns; ++col) {
      if (col > 0)
        out_ << ", ";
      out_ << table.Get(col, row);
    }
    out_ << ']';
  }
}

//-----------------------------------------------------------------------------
//
// The HtmlTreePrinter class prints a Tree in HTML format.
//
//-----------------------------------------------------------------------------

class HtmlTreePrinter {
 public:
  HtmlTreePrinter(std::ostream& out, const Tree& tree,  // NOLINT
                  bool address_printing_enabled)
      : out_(out),
        tree_(tree),
        address_printing_enabled_(address_printing_enabled) {}
  void Print();

  // Resets the static object counter.
  static void ResetObjectCounter() { object_counter_ = 0; }

 private:
  void PrintObject(size_t object_index);
  void PrintObjectHeader(const Tree::Object& object);
  void PrintFieldHeader();
  void PrintFieldFooter();
  void PrintFieldStart(const std::string& name);
  void PrintFieldEnd();
  void PrintObjectField(const Tree::ObjectField& field);
  void PrintStringField(const Tree::StringField& field);
  void PrintTableField(const Tree::TableField& field);

  std::ostream& out_;
  const Tree& tree_;
  bool address_printing_enabled_;
  // This counter is static so that each list ID is unique even when printing
  // multiple trees with different HtmlTreePrinter instances.
  static size_t object_counter_;
};

size_t HtmlTreePrinter::object_counter_ = 0;

void HtmlTreePrinter::Print() {
  const std::vector<size_t>& roots = tree_.GetRootObjectIndices();
  for (size_t i = 0; i < roots.size(); ++i)
    PrintObject(roots[i]);
}

void HtmlTreePrinter::PrintObject(size_t object_index) {
  const Tree::Object& object = tree_.GetObject(object_index);

  // Object header.
  PrintObjectHeader(object);

  // Fields.
  const bool has_fields = (object.has_enable_field ||
                           !object.string_fields.empty() ||
                           !object.table_fields.empty() ||
                           !object.object_fields.empty());
  if (has_fields)
    PrintFieldHeader();

  if (object.has_enable_field) {
    Tree::StringField enable_checkbox(
      "Enabled",
      std::string("<input type=\"checkbox\" ")
      + "id=\"" + object.label + "\" class=\"button\" "
      + (object.is_enabled ? "checked" : "") + ">");
    PrintStringField(enable_checkbox);
  }

  for (size_t i = 0; i < object.string_fields.size(); ++i)
    PrintStringField(object.string_fields[i]);
  for (size_t i = 0; i < object.table_fields.size(); ++i)
    PrintTableField(object.table_fields[i]);
  for (size_t i = 0; i < object.object_fields.size(); ++i)
    PrintObjectField(object.object_fields[i]);

  if (has_fields)
    PrintFieldFooter();

  // Child objects.
  for (size_t i = 0; i < object.child_object_indices.size(); ++i)
    PrintObject(object.child_object_indices[i]);

  // Object footer.
  out_ << "</ul></li>\n";
}

void HtmlTreePrinter::PrintObjectHeader(const Tree::Object& object) {
  // Create a checkbox that can be opened and closed.
  out_ << "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-"
       << object_counter_ << "\" class=\"tree_expandbox\"/><label for=\"list-"
       << object_counter_ << "\">";

  // If the type is not empty, this is a real object, not just a container.
  if (!object.type.empty()) {
    out_ << "ION " << object.type;
    if (!object.label.empty())
      out_ << " \"" << object.label << '\"';
    if (address_printing_enabled_)
      out_ << " [" << Pointer(object.pointer) << ']';
  }
  out_ << "</label><ul>\n";

  ++object_counter_;
}

void HtmlTreePrinter::PrintFieldHeader() {
  out_ << "<table class=\"nodes_field_table\">\n";
}

void HtmlTreePrinter::PrintFieldFooter() {
  out_ << "</table>\n";
}

void HtmlTreePrinter::PrintFieldStart(const std::string& name) {
  out_ << "<tr><td class=\"name\">" << name << "</td><td class=\"value\">";
}

void HtmlTreePrinter::PrintFieldEnd() {
  out_ << "</td></tr>\n";
}

void HtmlTreePrinter::PrintObjectField(const Tree::ObjectField& field) {
  PrintFieldStart(field.name);
  PrintObject(field.object_index);
  PrintFieldEnd();
}

void HtmlTreePrinter::PrintStringField(const Tree::StringField& field) {
  PrintFieldStart(field.name);
  out_ << field.value;
  PrintFieldEnd();
}

void HtmlTreePrinter::PrintTableField(const Tree::TableField& field) {
  PrintFieldStart(field.name);
  const Tree::Table& table = field.table;
  if (table.GetSize()) {
    out_ << "<table class=\"nodes_field_value_table\">\n";
    const size_t num_rows = table.GetHeight();
    const size_t num_columns = table.GetWidth();
    for (size_t row = 0; row < num_rows; ++row) {
      out_ << "<tr>\n";
      for (size_t col = 0; col < num_columns; ++col) {
        const std::string& cell = table.Get(col, row);
        if (table.HasLabelColumn() && col == 0)
          out_ << "<td><span class=\"table_label\">" << cell << "</span></td>";
        else
          out_ << "<td>" << cell << "</td>\n";
      }
      out_ << "</tr>\n";
    }
    out_ << "</table>\n";
  }
  PrintFieldEnd();
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Printer class functions.
//
//-----------------------------------------------------------------------------

Printer::Printer()
    : format_(kText),
      full_shape_printing_enabled_(false),
      address_printing_enabled_(true) {
}

Printer::~Printer() {
  HtmlTreePrinter::ResetObjectCounter();
}

void Printer::PrintScene(const NodePtr& node, std::ostream& out) {  // NOLINT
  if (node.Get()) {
    TreeBuilder tb(address_printing_enabled_, full_shape_printing_enabled_);
    const Tree tree = tb.BuildTree(*node);

    if (format_ == kText) {
      TextTreePrinter(out, tree, address_printing_enabled_).Print();
    } else {
      HtmlTreePrinter(out, tree, address_printing_enabled_).Print();
    }
  }
}

}  // namespace gfxutils
}  // namespace ion
