/*
 * Copyright 2017 The TensorFlow Authors. All Rights Reserved.
 * Copyright (c) 2019 Samsung Electronics Co., Ltd. All Rights Reserved
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

#ifndef __BASE_LOADER_BASE_LOADER_H__
#define __BASE_LOADER_BASE_LOADER_H__

#include "ir/Graph.h"
#include "ir/Shape.h"
#include "ir/Operations.Include.h"

#include "flatbuffers/flexbuffers.h"

#include <map>
#include <memory>
#include <fstream>
#include <limits>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <util/logging.h>

namespace onert
{
namespace base_loader
{

template <typename LoaderDomain> class BaseLoader
{
protected:
  using Verifier = typename LoaderDomain::Verifier;
  using ActivationFunctionType = typename LoaderDomain::ActivationFunctionType;
  using Buffer = typename LoaderDomain::Buffer;
  using BuiltinOperator = typename LoaderDomain::BuiltinOperator;
  using CustomOptionsFormat = typename LoaderDomain::CustomOptionsFormat;
  using Model = typename LoaderDomain::Model;
  using Operator = typename LoaderDomain::Operator;
  using Padding = typename LoaderDomain::Padding;
  using Pool2DOptions = typename LoaderDomain::Pool2DOptions;
  using SubGraph = typename LoaderDomain::SubGraph;
  using Tensor = typename LoaderDomain::Tensor;
  using TensorType = typename LoaderDomain::TensorType;
  using DimensionType = typename LoaderDomain::DimensionType;
  using SparseIndexVector = typename LoaderDomain::SparseIndexVector;

protected:
  bool isOptionalInputTensor(std::int32_t idx) { return idx == -1; }
  virtual bool allowOptionalInputTensor(BuiltinOperator) = 0;

public:
  /**
   * @brief Construct a new Loader object
   *
   * @param graph reference on subgraphs
   */
  explicit BaseLoader(std::unique_ptr<ir::Subgraphs> &subgs)
      : _base{nullptr}, _pagesize(getpagesize()), _fd(-1), _subgraphs(subgs), _model{nullptr}
  {
  }

  /**
   * @brief Load a model from file
   *
   * @param file_path
   */
  void loadFromFile(const char *file_path);
  /**
   * @brief Load a model from a buffer
   *
   * @param buffer buffer pointer
   * @param size buffer size
   */
  void loadFromBuffer(uint8_t *buffer, size_t size);

protected:
  ~BaseLoader() = default;
  void loadModel();

  // Helper functions
  ir::Activation convertActivation(ActivationFunctionType type);
  ir::DataType tensorTypeToDataType(TensorType type);
  ir::OperandIndex tensorIdxToOperandIdx(int32_t tensorIdx);
  void deallocateMmappedArea(uint8_t *ptr, size_t size);

  // Create operands form tflite::Tensor
  ir::OperandIndex loadOperand(const Tensor *tensor, ir::Graph &subg);
  void loadOperationIO(const Operator *op, ir::OperandIndexSequence &inputs,
                       ir::OperandIndexSequence &outputs);
  // Create operations from Operator
  void loadOperation(const Operator *op, ir::Graph &subg);
  // Load Strides and Paddings from options to param
  template <typename Param, typename OptionsType>
  void loadStridesAndPaddings(Param &param, const OptionsType *options);
  // Load Pool2D param
  template <typename Param> void loadPool2DOptions(Param &param, const Pool2DOptions *options);

private:
  virtual std::unique_ptr<ir::Graph> loadSubgraph(const SubGraph *subg) = 0;
  // Operations
  template <typename OpIR, typename... Args>
  const OpIR *loadOperationTo(const Operator *op, ir::Graph &subg, Args &&... args);
  void loadConv2D(const Operator *op, ir::Graph &subg);
  void loadDepthwiseConv2D(const Operator *op, ir::Graph &subg);
  void loadTransposeConv(const Operator *op, ir::Graph &subg);
  void loadPool2D(const Operator *op, ir::Graph &subg, ir::operation::Pool2D::PoolType op_type);
  void loadReshape(const Operator *op, ir::Graph &subg);
  void loadSoftmax(const Operator *op, ir::Graph &subg);
  void loadConcatenation(const Operator *op, ir::Graph &subg);
  void loadFC(const Operator *op, ir::Graph &subg);
  void loadBinaryArithmetic(const Operator *op, ir::Graph &subg,
                            ir::operation::BinaryArithmetic::ArithmeticType op_type);
  void loadAddV2(const Operator *op, ir::Graph &subg);
  void loadPack(const Operator *op, ir::Graph &subg);
  void loadResizeBilinear(const Operator *op, ir::Graph &subg);
  void loadResizeNearestNeighbor(const Operator *op, ir::Graph &subg);
  void loadTranspose(const Operator *op, ir::Graph &subg);
  void loadReduce(const Operator *op, ir::Graph &subg,
                  ir::operation::Reduce::ReduceType reduce_type);
  void loadReduceAll(const Operator *op, ir::Graph &subg);
  void loadElementwiseActivation(const Operator *op, ir::Graph &subg,
                                 ir::operation::ElementwiseActivation::Type op_type,
                                 float alpha = 0.f, float beta = 0.f);
  void loadElementwiseBinary(const Operator *op, ir::Graph &subg,
                             ir::operation::ElementwiseBinary::ElementwiseBinaryType op_type);
  void loadElementwiseUnary(const Operator *op, ir::Graph &subg,
                            ir::operation::ElementwiseUnary::Type op_type);
  void loadGather(const Operator *op, ir::Graph &subg);
  void loadCustom(const Operator *op, ir::Graph &subg);
  void loadBatchMatMul(const Operator *op, ir::Graph &subg);
  void loadSqueeze(const Operator *op, ir::Graph &subg);
  void loadSplit(const Operator *op, ir::Graph &subg);
  void loadSplitV(const Operator *op, ir::Graph &subg);
  void loadStridedSlice(const Operator *op, ir::Graph &subg);
  void loadUnpack(const Operator *op, ir::Graph &subg);
  void loadComparison(const Operator *op, ir::Graph &subg);
  void loadEinsum(const Operator *op, ir::Graph &subg);
  void loadOneHot(const Operator *op, ir::Graph &subg);
  void loadIf(const Operator *op, ir::Graph &subg);
  void loadWhile(const Operator *op, ir::Graph &subg);
  void loadArgMax(const Operator *op, ir::Graph &subg);
  void loadTile(const Operator *op, ir::Graph &subg);
  void loadFusedBatchNorm(const Operator *op, ir::Graph &subg);
  void loadLogSoftmax(const Operator *op, ir::Graph &subg);
  void loadSpaceToDepth(const Operator *op, ir::Graph &subg);
  void loadLeakyRelu(const Operator *op, ir::Graph &subg);

protected:
  // Base address for mapped region for loading (if needed)
  uint8_t *_base;
  // Memory page size
  int32_t _pagesize;
  // loaded file description
  int _fd;
  // Reference on loadable subgraphs
  std::unique_ptr<ir::Subgraphs> &_subgraphs;
  const Model *_model;
  // Maps Tensor indices to onert Operands.
  std::vector<ir::OperandIndex> _tensor_to_operand;
  std::unordered_map<ir::OperandIndex, std::string> _tensor_names;
  // Verifier
  std::unique_ptr<Verifier> _verifier;
};

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::BaseLoader::loadFromFile(const char *file_path)
{
  _fd = open(file_path, O_RDONLY);
  if (_fd < 0)
  {
    throw std::runtime_error("Failed to open file " + std::string(file_path));
  }

  struct stat file_stat;
  if (fstat(_fd, &file_stat) != 0)
  {
    throw std::runtime_error("Fstat failed or file " + std::string(file_path) +
                             " is not a regular file");
  }
  int size = file_stat.st_size;

  // Map model file into memory region
  _base = static_cast<uint8_t *>(mmap(NULL, size, PROT_READ, MAP_PRIVATE, _fd, 0));
  if (_base == MAP_FAILED)
  {
    close(_fd);
    throw std::runtime_error("mmap failed - " + std::string(strerror(errno)));
  }

  _verifier = std::make_unique<Verifier>(reinterpret_cast<const std::uint8_t *>(_base), size);

  loadModel();

  close(_fd);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::BaseLoader::loadFromBuffer(uint8_t *buffer, size_t size)
{
  _base = buffer;
  _verifier = std::make_unique<Verifier>(reinterpret_cast<const std::uint8_t *>(_base), size);
  loadModel();
}

template <typename LoaderDomain>
ir::Activation
BaseLoader<LoaderDomain>::BaseLoader::convertActivation(const ActivationFunctionType type)
{
  switch (type)
  {
    case ActivationFunctionType::ActivationFunctionType_NONE:
      return ir::Activation::NONE;
    case ActivationFunctionType::ActivationFunctionType_RELU:
      return ir::Activation::RELU;
    case ActivationFunctionType::ActivationFunctionType_RELU_N1_TO_1:
      return ir::Activation::RELU1;
    case ActivationFunctionType::ActivationFunctionType_RELU6:
      return ir::Activation::RELU6;
    case ActivationFunctionType::ActivationFunctionType_TANH:
      return ir::Activation::TANH;
    default:
      throw std::runtime_error(std::string("Unsupported activation type: ")
                                   .append(EnumNameActivationFunctionType(type)));
  }
}

template <typename LoaderDomain>
ir::DataType BaseLoader<LoaderDomain>::BaseLoader::tensorTypeToDataType(const TensorType type)
{
  switch (type)
  {
    case TensorType::TensorType_FLOAT32:
      return ir::DataType::FLOAT32;
    case TensorType::TensorType_INT32:
      return ir::DataType::INT32;
    case TensorType::TensorType_BOOL:
      return ir::DataType::BOOL8;
    case TensorType::TensorType_UINT8:
      return ir::DataType::QUANT_UINT8_ASYMM;
    case TensorType::TensorType_INT8:
      return ir::DataType::QUANT_INT8_SYMM;
    case TensorType::TensorType_INT64:
      return ir::DataType::INT64;
    default:
      throw std::runtime_error(
          std::string("Unsupported tensor type: ").append(EnumNameTensorType(type)));
  }
}

template <typename LoaderDomain>
ir::OperandIndex BaseLoader<LoaderDomain>::BaseLoader::tensorIdxToOperandIdx(int32_t tensorIdx)
{
  return isOptionalInputTensor(tensorIdx) ? ir::OperandIndex() : _tensor_to_operand[tensorIdx];
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::BaseLoader::deallocateMmappedArea(uint8_t *ptr, size_t size)
{
  // Calculate offset from base address of mapped region
  ptrdiff_t unaligned_offset_start = ptr - _base;
  ptrdiff_t unaligned_offset_end = unaligned_offset_start + size;

  // Calculated aligned offset from base address of mapped region
  // munmap accepts memory address which is a multiple of the pagesize
  ptrdiff_t aligned_offset_start =
      ((unaligned_offset_start + (_pagesize - 1)) / _pagesize) * _pagesize;
  ptrdiff_t aligned_offset_end = (unaligned_offset_end / _pagesize) * _pagesize;

  ptrdiff_t area_size = aligned_offset_end - aligned_offset_start;
  if (area_size > 0)
  {
    // Unmap mapped region for CachedData
    if (munmap(_base + aligned_offset_start, area_size) == -1)
    {
      VERBOSE(BASE_LOADER) << "munmap failed" << std::endl;
    }
  }
}

/* Copy is copied from tensorflow lite */
template <typename T> bool Copy(const T *data_ptr, std::vector<uint16_t> &arr)
{
  if (data_ptr->values() == nullptr)
  {
    return false;
  }

  int size = data_ptr->values()->size();
  arr.reserve(size);
  for (int i = 0; i < size; i++)
  {
    arr.emplace_back(static_cast<uint16_t>(data_ptr->values()->Get(i)));
  }
  return true;
}

template <typename LoaderDomain>
ir::OperandIndex BaseLoader<LoaderDomain>::loadOperand(const Tensor *tensor, ir::Graph &subg)
{
  ir::Shape shape;
  // Shape
  const auto *tensor_shape = tensor->shape();
  if (tensor_shape != nullptr)
  {
    for (const auto &dim : *tensor_shape)
    {
      shape.append(dim);
    }
  }

  // Note for tensor->shape_signature()
  // We don't handle shape signature
  //    How we handle:
  //       If shape_signature[k] == -1, we will use tensor->shape()[k] == 1
  //       If app wants to change the input shape, call nnfw_apply_input_tensorinfo() can
  //       be used.

  // Type
  ir::DataType data_type = tensorTypeToDataType(tensor->type());
  // Quantization
  auto q_params = tensor->quantization();
  float scale = 0.0;
  long zero_point = 0;
  if (q_params != nullptr)
  {
    if (q_params->scale())
    {
      if (q_params->scale()->size() != 1)
      {
        throw std::runtime_error("Only 1 scale for a tensor is supported.");
      }
      scale = q_params->scale()->Get(0);
    }

    if (q_params->zero_point())
    {
      if (q_params->zero_point()->size() != 1)
      {
        throw std::runtime_error("Only 1 zero_point value for a tensor is supported.");
      }
      zero_point = q_params->zero_point()->Get(0);
      // zero_point is long while TypeInfo.zero_point is defined as int32_t.
      assert(zero_point >= std::numeric_limits<int32_t>::min());
      assert(zero_point <= std::numeric_limits<int32_t>::max());
    }
    auto details = q_params->details_as_CustomQuantization();
    if (details != nullptr)
      throw std::runtime_error("Custom Quantization is not supported");
  }
  // Create TypeInfo
  ir::TypeInfo type_info(data_type, scale, zero_point);
  // Sparsity
  auto src_sparsity = tensor->sparsity();
  if (src_sparsity != nullptr)
  {
    std::vector<uint16_t> w1_segments;
    std::vector<uint16_t> w1_indices;
    // ignore traversal_order, block_map
    // load metadata
    const size_t dim_metadata_size = src_sparsity->dim_metadata()->size();
    if (dim_metadata_size != 2)
      throw std::runtime_error("sparse tensor is supported only for 2D");
    const auto *src_metadata = src_sparsity->dim_metadata()->Get(0);
    if (src_metadata->format() != DimensionType::DimensionType_DENSE)
      throw std::runtime_error("sparse tensor dim[0] is not DENSE");
    src_metadata = src_sparsity->dim_metadata()->Get(1);
    if (src_metadata->format() != DimensionType::DimensionType_SPARSE_CSR)
      throw std::runtime_error("sparse tensor dim[0] is not SPARSE_CSR");

    auto ParseSparseIndexVector = [src_metadata, &w1_segments, &w1_indices]() {
      if (src_metadata->array_segments() == nullptr || src_metadata->array_indices() == nullptr)
        return false;
      bool status = true;
      switch (src_metadata->array_segments_type())
      {
        case SparseIndexVector::SparseIndexVector_Int32Vector:
          status = Copy(src_metadata->array_segments_as_Int32Vector(), w1_segments);
          break;
        case SparseIndexVector::SparseIndexVector_Uint16Vector:
          status = Copy(src_metadata->array_segments_as_Uint16Vector(), w1_segments);
          break;
        case SparseIndexVector::SparseIndexVector_Uint8Vector:
          status = Copy(src_metadata->array_segments_as_Uint8Vector(), w1_segments);
          break;
        default:
          return false;
      }
      if (status != true)
        return false;
      switch (src_metadata->array_indices_type())
      {
        case SparseIndexVector::SparseIndexVector_Int32Vector:
          return Copy(src_metadata->array_indices_as_Int32Vector(), w1_indices);
        case SparseIndexVector::SparseIndexVector_Uint16Vector:
          return Copy(src_metadata->array_indices_as_Uint16Vector(), w1_indices);
        case SparseIndexVector::SparseIndexVector_Uint8Vector:
          return Copy(src_metadata->array_indices_as_Uint8Vector(), w1_indices);
        default:
          break;
      }
      return false;
    };
    if (ParseSparseIndexVector() == false)
      throw std::runtime_error("Error during parsing sparsity index information");
    type_info.sparse2DMetadata(std::move(w1_segments), std::move(w1_indices));
  }
  // Create operand
  const auto operand_index = subg.addOperand(shape, type_info);

  // Constant tensors are indicated by non-empty data.
  const auto *data = _model->buffers()->Get(tensor->buffer())->data();
  if (data != nullptr)
  {
    using std::ptrdiff_t;
    std::unique_ptr<ir::Data> data_obj;
    if (_fd == -1) // Model is from memory
    {
      data_obj = std::make_unique<ir::ExternalData>(data->data(), data->size());
    }
    else // Model is loaded(mmap'd) from a file
    {
      data_obj = std::make_unique<ir::CachedData>(data->data(), data->size());
      deallocateMmappedArea(const_cast<uint8_t *>(data->data()), data->size());
    }
    subg.setOperandValue(operand_index, std::move(data_obj));
  }

  _tensor_names.emplace(operand_index, tensor->name()->str());

  // Variablie
  if (tensor->is_variable())
    throw std::runtime_error("Variable tensor not supported!");

  return operand_index;
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadOperationIO(const Operator *op, ir::OperandIndexSequence &inputs,
                                               ir::OperandIndexSequence &outputs)
{
  for (const std::int32_t idx : *op->inputs())
  {
    // Optional tensors are not supported yet except for FULLY_CONNECTED and BCQ_FULLY_CONNECTED
    auto check_optional_input = [&]() {
      auto builtin_code = _model->operator_codes()->Get(op->opcode_index())->builtin_code();
      if (isOptionalInputTensor(idx) && !allowOptionalInputTensor(builtin_code))
        throw std::runtime_error(
            std::string("loader doesn't support optional input tensor yet for ")
                .append(EnumNameBuiltinOperator(builtin_code)));
    };
    check_optional_input();
    inputs.append(tensorIdxToOperandIdx(idx));
  }

  for (const std::int32_t idx : *op->outputs())
  {
    outputs.append(tensorIdxToOperandIdx(idx));
  }
}

template <typename LoaderDomain>
template <typename Param, typename OptionsType>
void BaseLoader<LoaderDomain>::loadStridesAndPaddings(Param &param, const OptionsType *options)
{
  // Strides
  param.stride.vertical = options->stride_h();
  param.stride.horizontal = options->stride_w();
  // Paddings
  if (options->padding() == Padding::Padding_SAME)
    param.padding.type = ir::PaddingType::SAME;
  if (options->padding() == Padding::Padding_VALID)
    param.padding.type = ir::PaddingType::VALID;
  // param paddings indexes unused
}

template <typename LoaderDomain>
template <typename Param>
void BaseLoader<LoaderDomain>::loadPool2DOptions(Param &param, const Pool2DOptions *options)
{
  // Strides and Paddings
  loadStridesAndPaddings(param, options);
  // Filter width and height
  // Strides
  param.kw = options->filter_width();
  param.kh = options->filter_height();
  // Activation
  param.activation = convertActivation(options->fused_activation_function());
}

template <typename LoaderDomain>
template <typename OpIR, typename... Args>
const OpIR *BaseLoader<LoaderDomain>::loadOperationTo(const Operator *op, ir::Graph &subg,
                                                      Args &&... args)
{
  static_assert(sizeof...(args) <= 1, "You can't have more than 1 arguments!");
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);

  std::unique_ptr<OpIR> new_op(new OpIR(inputs, outputs, std::forward<Args>(args)...));
  auto ret = new_op.get();
  subg.addOperation(std::move(new_op));

  return ret;
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadConv2D(const Operator *op, ir::Graph &subg)
{
  ir::operation::Conv2D::Param param;
  const auto *options = op->builtin_options_as_Conv2DOptions();
  param.activation = convertActivation(options->fused_activation_function());
  loadStridesAndPaddings(param, options);
  param.dilation.width_factor = options->dilation_w_factor();
  param.dilation.height_factor = options->dilation_h_factor();

  loadOperationTo<ir::operation::Conv2D>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadDepthwiseConv2D(const Operator *op, ir::Graph &subg)
{
  ir::operation::DepthwiseConv2D::Param param;
  const auto *options = op->builtin_options_as_DepthwiseConv2DOptions();
  param.activation = convertActivation(options->fused_activation_function());
  loadStridesAndPaddings(param, options);
  param.multiplier = options->depth_multiplier();
  // Dilation h/w factor unused

  loadOperationTo<ir::operation::DepthwiseConv2D>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadTransposeConv(const Operator *op, ir::Graph &subg)
{
  ir::operation::TransposeConv::Param param;
  const auto *options = op->builtin_options_as_TransposeConvOptions();
  loadStridesAndPaddings(param, options);

  loadOperationTo<ir::operation::TransposeConv>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadPool2D(const Operator *op, ir::Graph &subg,
                                          ir::operation::Pool2D::PoolType op_type)
{
  ir::operation::Pool2D::Param param;
  param.op_type = op_type;
  const auto *options = op->builtin_options_as_Pool2DOptions();

  loadPool2DOptions(param, options);

  loadOperationTo<ir::operation::Pool2D>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadReshape(const Operator *op, ir::Graph &subg)
{
  ir::operation::Reshape::Param param{};
  const auto *options = op->builtin_options_as_ReshapeOptions();
  if (options != nullptr)
  {
    const auto *new_shape = options->new_shape();
    if (new_shape)
    {
      for (uint i = 0; i < new_shape->size(); ++i)
      {
        param.new_shape.push_back(new_shape->Get(i));
      }
    }
  }

  loadOperationTo<ir::operation::Reshape>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadSoftmax(const Operator *op, ir::Graph &subg)
{
  ir::operation::Softmax::Param param;
  const auto *options = op->builtin_options_as_SoftmaxOptions();
  // Beta
  param.beta = options->beta();

  loadOperationTo<ir::operation::Softmax>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadConcatenation(const Operator *op, ir::Graph &subg)
{
  ir::operation::Concat::Param param;
  const auto *options = op->builtin_options_as_ConcatenationOptions();
  // Axis
  param.axis = options->axis();
  // activation unused

  loadOperationTo<ir::operation::Concat>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadFC(const Operator *op, ir::Graph &subg)
{
  ir::operation::FullyConnected::Param param;
  const auto *options = op->builtin_options_as_FullyConnectedOptions();

  param.activation = convertActivation(options->fused_activation_function());
  // weights_format unused

  const auto fc = loadOperationTo<ir::operation::FullyConnected>(op, subg, param);

  const auto &input_operand =
      subg.operands().at(fc->getInputs().at(ir::operation::FullyConnected::INPUT));
  auto &weights_operand =
      subg.operands().at(fc->getInputs().at(ir::operation::FullyConnected::WEIGHT));
  if (input_operand.typeInfo().type() == ir::DataType::FLOAT32 &&
      weights_operand.typeInfo().type() == ir::DataType::QUANT_UINT8_ASYMM)
  {
    weights_operand.type(ir::DataType::QUANT_INT8_SYMM);
  }
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadAddV2(const Operator *op, ir::Graph &subg)
{
  ir::operation::BinaryArithmetic::Param param;
  param.arithmetic_type = ir::operation::BinaryArithmetic::ArithmeticType::ADD;

  if (op->custom_options() == nullptr)
  {
    param.activation = ir::Activation::NONE;
  }
  else
  {
    size_t custom_op_data_size = op->custom_options()->size();
    auto custom_op_data = op->custom_options()->Data();
    auto data_root = flexbuffers::GetRoot(custom_op_data, custom_op_data_size);
    auto attr_map = data_root.AsMap();
    const auto fused_activation_func = static_cast<typename LoaderDomain::ActivationFunctionType>(
        attr_map["fused_activation_function"].AsInt8());
    param.activation = convertActivation(fused_activation_func);
  }

  loadOperationTo<ir::operation::BinaryArithmetic>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadBinaryArithmetic(
    const Operator *op, ir::Graph &subg, ir::operation::BinaryArithmetic::ArithmeticType op_type)
{
  ir::operation::BinaryArithmetic::Param param;
  param.arithmetic_type = op_type;
  switch (op_type)
  {
    case ir::operation::BinaryArithmetic::ArithmeticType::ADD:
    {
      const auto *add_options = op->builtin_options_as_AddOptions();
      param.activation = convertActivation(add_options->fused_activation_function());
      break;
    }
    case ir::operation::BinaryArithmetic::ArithmeticType::SUB:
    {
      const auto *sub_options = op->builtin_options_as_SubOptions();
      param.activation = convertActivation(sub_options->fused_activation_function());
      break;
    }
    case ir::operation::BinaryArithmetic::ArithmeticType::MUL:
    {
      const auto *mul_options = op->builtin_options_as_MulOptions();
      param.activation = convertActivation(mul_options->fused_activation_function());
      break;
    }
    case ir::operation::BinaryArithmetic::ArithmeticType::DIV:
    {
      const auto *div_options = op->builtin_options_as_DivOptions();
      param.activation = convertActivation(div_options->fused_activation_function());
      break;
    }
    default:
      assert(false &&
             "The function 'loadBinaryArithmetic' supports only BinaryArithmetic operations");
      break;
  }

  loadOperationTo<ir::operation::BinaryArithmetic>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadPack(const Operator *op, ir::Graph &subg)
{
  ir::operation::Pack::Param param;
  const auto *options = op->builtin_options_as_PackOptions();
  param.num = options->values_count();
  param.axis = options->axis();

  loadOperationTo<ir::operation::Pack>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadElementwiseActivation(
    const Operator *op, ir::Graph &subg, ir::operation::ElementwiseActivation::Type op_type,
    float alpha, float beta)
{
  ir::operation::ElementwiseActivation::Param param;
  param.op_type = op_type;
  param.alpha = alpha;
  param.beta = beta;

  loadOperationTo<ir::operation::ElementwiseActivation>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadResizeBilinear(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);
  auto input = inputs.at(0);
  auto size = inputs.at(1);

  // FIXME Handle ResizeBilinearOptions.
  if (!subg.operands().at(size).isConstant())
    throw std::runtime_error("ResizeBilinear: non-constant 'size' is not supported.");

  std::vector<std::int32_t> size_v = subg.operands().at(size).template asVector<std::int32_t>();

  ir::operation::ResizeBilinear::Param param;
  param.height_out = size_v[0];
  param.width_out = size_v[1];
  param.align_corners = op->builtin_options_as_ResizeBilinearOptions()->align_corners();
  param.half_pixel_centers = op->builtin_options_as_ResizeBilinearOptions()->half_pixel_centers();

  std::unique_ptr<ir::Operation> new_op(new ir::operation::ResizeBilinear({input}, outputs, param));
  subg.addOperation(std::move(new_op));
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadResizeNearestNeighbor(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);
  auto input = inputs.at(0);
  auto size = inputs.at(1);

  // FIXME Handle ResizeNearestNeighborOptions.
  if (!subg.operands().at(size).isConstant())
    throw std::runtime_error("ResizeNearestNeighbor: non-constant 'size' is not supported.");

  std::vector<std::int32_t> size_v = subg.operands().at(size).template asVector<std::int32_t>();

  ir::operation::ResizeNearestNeighbor::Param param;
  param.height_out = size_v[0];
  param.width_out = size_v[1];
  param.align_corners = op->builtin_options_as_ResizeNearestNeighborOptions()->align_corners();

  std::unique_ptr<ir::Operation> new_op(
      new ir::operation::ResizeNearestNeighbor({input}, outputs, param));
  subg.addOperation(std::move(new_op));
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadTranspose(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);
  auto input = inputs.at(0);
  auto perm = inputs.at(1);

  // FIXME Handle TransposeOptions.
  if (!subg.operands().at(perm).isConstant())
    throw std::runtime_error("Transpose: non-constant 'perm' is not supported.");

  ir::operation::Transpose::Param param;
  param.perm = subg.operands().at(perm).template asVector<int>();

  std::unique_ptr<ir::Operation> new_op(new ir::operation::Transpose({input}, outputs, param));
  subg.addOperation(std::move(new_op));
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadReduce(const Operator *op, ir::Graph &subg,
                                          ir::operation::Reduce::ReduceType reduce_type)
{
  ir::operation::Reduce::Param param;
  param.reduce_type = reduce_type;
  param.keep_dims = op->builtin_options_as_ReducerOptions()->keep_dims();

  loadOperationTo<ir::operation::Reduce>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadReduceAll(const Operator *op, ir::Graph &subg)
{
  ir::operation::Reduce::Param param;
  param.reduce_type = ir::operation::Reduce::ReduceType::ALL;
  if (op->custom_options() == nullptr)
  {
    param.keep_dims = false;
  }
  else
  {
    size_t custom_op_data_size = op->custom_options()->size();
    auto custom_op_data = op->custom_options()->Data();
    auto data_root = flexbuffers::GetRoot(custom_op_data, custom_op_data_size);
    auto attr_map = data_root.AsMap();
    param.keep_dims = attr_map["keep_dims"].AsBool();
  }

  loadOperationTo<ir::operation::Reduce>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadElementwiseBinary(
    const Operator *op, ir::Graph &subg,
    ir::operation::ElementwiseBinary::ElementwiseBinaryType op_type)
{
  ir::operation::ElementwiseBinary::Param param;
  param.op_type = op_type;

  loadOperationTo<ir::operation::ElementwiseBinary>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadElementwiseUnary(const Operator *op, ir::Graph &subg,
                                                    ir::operation::ElementwiseUnary::Type op_type)
{
  ir::operation::ElementwiseUnary::Param param;
  param.op_type = op_type;

  const auto eu = loadOperationTo<ir::operation::ElementwiseUnary>(op, subg, param);
  if (op_type == ir::operation::ElementwiseUnary::Type::CAST)
  {
    auto qasymm8ToUint8 = [](ir::Operand &operand) {
      if (operand.typeInfo().type() == ir::DataType::QUANT_UINT8_ASYMM)
      {
        operand.type(ir::DataType::UINT8);
      }
    };
    qasymm8ToUint8(
        subg.operands().at(eu->getInputs().at(ir::operation::ElementwiseUnary::Input::INPUT)));
    qasymm8ToUint8(subg.operands().at(eu->getOutputs().at(0)));
  }
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadGather(const Operator *op, ir::Graph &subg)
{
  ir::operation::Gather::Param param;
  param.axis = op->builtin_options_as_GatherOptions()->axis();

  loadOperationTo<ir::operation::Gather>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadBatchMatMul(const Operator *op, ir::Graph &subg)
{
  ir::operation::BatchMatMul::Param param;

  const auto builtin_op = _model->operator_codes()->Get(op->opcode_index())->builtin_code();

  switch (builtin_op)
  {
    case BuiltinOperator::BuiltinOperator_BATCH_MATMUL:
      param.adj_x = op->builtin_options_as_BatchMatMulOptions()->adjoint_lhs();
      param.adj_y = op->builtin_options_as_BatchMatMulOptions()->adjoint_rhs();
      break;
    case BuiltinOperator::BuiltinOperator_CUSTOM:
      if (op->custom_options() == nullptr)
      {
        param.adj_x = false;
        param.adj_y = false;
      }
      else
      {
        size_t custom_op_data_size = op->custom_options()->size();
        auto custom_op_data = op->custom_options()->Data();
        auto data_root = flexbuffers::GetRoot(custom_op_data, custom_op_data_size);
        auto attr_map = data_root.AsMap();
        param.adj_x = attr_map["adj_x"].AsBool();
        param.adj_y = attr_map["adj_y"].AsBool();
      }
      break;
    default:
      throw std::runtime_error(
          std::string("Wrong loaded operation: ").append(EnumNameBuiltinOperator(builtin_op)) +
          " as " + EnumNameBuiltinOperator(BuiltinOperator::BuiltinOperator_BATCH_MATMUL));
  }

  loadOperationTo<ir::operation::BatchMatMul>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadSpaceToDepth(const Operator *op, ir::Graph &subg)
{
  ir::operation::SpaceToDepth::Param param;
  const auto *options = op->builtin_options_as_SpaceToDepthOptions();
  param.block_size = options->block_size();

  loadOperationTo<ir::operation::SpaceToDepth>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadCustom(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  assert(op->custom_options_format() == CustomOptionsFormat::CustomOptionsFormat_FLEXBUFFERS &&
         "Unsupported custom operation options format");

  auto *op_code = _model->operator_codes()->Get(op->opcode_index());
  auto custom_op_name = op_code->custom_code()->str();

  enum class BuiltinOP
  {
    AddV2,
    ReduceAll,
    MatrixBandPart,
    BatchMatMul,
    Einsum,
    BroadcastTo,
    FusedBatchNorm,
    StatelessRandomUniform,
    Erf
  };

  // Mapping from custom op name string to BuiltinOP enum
  std::map<std::string, BuiltinOP> builtin_map = {
      {"AddV2", BuiltinOP::AddV2},
      {"All", BuiltinOP::ReduceAll},
      {"MatrixBandPart", BuiltinOP::MatrixBandPart},
      {"BatchMatMulV2", BuiltinOP::BatchMatMul},
      {"Einsum", BuiltinOP::Einsum},
      {"FusedBatchNormV3", BuiltinOP::FusedBatchNorm},
      {"BroadcastTo", BuiltinOP::BroadcastTo},
      {"StatelessRandomUniform", BuiltinOP::StatelessRandomUniform},
      {"Erf", BuiltinOP::Erf},
  };

  try
  {
    // Throw out_of_range if it is unknown custom op
    auto custom_op_id = builtin_map.at(custom_op_name);
    switch (custom_op_id)
    {
      case BuiltinOP::AddV2:
        loadAddV2(op, subg);
        break;
      case BuiltinOP::ReduceAll:
        loadReduceAll(op, subg);
        break;
      case BuiltinOP::MatrixBandPart:
        loadOperationTo<ir::operation::MatrixBandPart>(op, subg);
        break;
      case BuiltinOP::BatchMatMul:
        loadBatchMatMul(op, subg);
        break;
      case BuiltinOP::Einsum:
        loadEinsum(op, subg);
        break;
      case BuiltinOP::BroadcastTo:
        loadOperationTo<ir::operation::BroadcastTo>(op, subg);
        break;
      case BuiltinOP::FusedBatchNorm:
        loadFusedBatchNorm(op, subg);
        break;
      case BuiltinOP::StatelessRandomUniform:
        loadOperationTo<ir::operation::StatelessRandomUniform>(op, subg);
        break;
      case BuiltinOP::Erf:
        loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::ERF);
        break;
      default:
        throw std::runtime_error{
            "Loader: Custom OP map is defined but operation loader function is not defined"};
    }

    return;
  }
  catch (...)
  {
    loadOperationIO(op, inputs, outputs);

    auto constraint = ir::OperandConstraint::createExact(inputs.size());

    size_t custom_op_data_size = op->custom_options()->size();
    auto custom_op_data = new char[custom_op_data_size];
    std::copy(op->custom_options()->begin(), op->custom_options()->end(), custom_op_data);

    ir::operation::Custom::Userdata userdata{};
    userdata.data = custom_op_data;
    userdata.size = custom_op_data_size;

    auto new_op = std::make_unique<ir::operation::Custom>(constraint, inputs, outputs,
                                                          custom_op_name, userdata);

    subg.addOperation(std::move(new_op));
  }
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadSqueeze(const Operator *op, ir::Graph &subg)
{
  ir::operation::Squeeze::Param param;
  const auto *options = op->builtin_options_as_SqueezeOptions();
  const auto *dims = options->squeeze_dims();
  if (dims)
  {
    if (dims->size() > sizeof(param.dims) / sizeof(param.dims[0]))
      throw std::runtime_error("Squeeze: 'param.ndims' is out of range.");
    param.ndim = dims->size();
    for (int i = 0; i < param.ndim; ++i)
      param.dims[i] = dims->Get(i);
  }

  loadOperationTo<ir::operation::Squeeze>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadSplit(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);
  // Notice : input order is strange for tflite split
  auto input = inputs.at(1);
  auto axis = inputs.at(0);

  // FIXME Handle SplitOptions.
  if (!subg.operands().at(axis).isConstant())
    throw std::runtime_error("Split: non-constant 'axis' is not supported.");

  ir::operation::Split::Param param;
  param.axis = subg.operands().at(axis).template asScalar<int>();
  const auto *options = op->builtin_options_as_SplitOptions();
  param.num_splits = options->num_splits();

  std::unique_ptr<ir::Operation> new_op(new ir::operation::Split({input}, outputs, param));
  subg.addOperation(std::move(new_op));
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadSplitV(const Operator *op, ir::Graph &subg)
{
  ir::operation::SplitV::Param param;
  const auto *options = op->builtin_options_as_SplitVOptions();
  param.num_splits = options->num_splits();

  loadOperationTo<ir::operation::SplitV>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadStridedSlice(const Operator *op, ir::Graph &subg)
{
  ir::operation::StridedSlice::Param param;
  const auto *options = op->builtin_options_as_StridedSliceOptions();
  param.begin_mask = options->begin_mask();
  param.end_mask = options->end_mask();
  param.shrink_axis_mask = options->shrink_axis_mask();

  loadOperationTo<ir::operation::StridedSlice>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadUnpack(const Operator *op, ir::Graph &subg)
{
  ir::operation::Unpack::Param param;
  const auto *options = op->builtin_options_as_UnpackOptions();
  param.num = options->num();
  param.axis = options->axis();

  loadOperationTo<ir::operation::Unpack>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadComparison(const Operator *op, ir::Graph &subg)
{
  ir::operation::Comparison::Param param;
  const auto builtin_op = _model->operator_codes()->Get(op->opcode_index())->builtin_code();

  switch (builtin_op)
  {
    case BuiltinOperator::BuiltinOperator_EQUAL:
      param.comparison_type = ir::operation::Comparison::ComparisonType::Equal;
      break;
    case BuiltinOperator::BuiltinOperator_NOT_EQUAL:
      param.comparison_type = ir::operation::Comparison::ComparisonType::NotEqual;
      break;
    case BuiltinOperator::BuiltinOperator_GREATER_EQUAL:
      param.comparison_type = ir::operation::Comparison::ComparisonType::GreaterEqual;
      break;
    case BuiltinOperator::BuiltinOperator_GREATER:
      param.comparison_type = ir::operation::Comparison::ComparisonType::Greater;
      break;
    case BuiltinOperator::BuiltinOperator_LESS_EQUAL:
      param.comparison_type = ir::operation::Comparison::ComparisonType::LessEqual;
      break;
    case BuiltinOperator::BuiltinOperator_LESS:
      param.comparison_type = ir::operation::Comparison::ComparisonType::Less;
      break;
    default:
      throw std::runtime_error(
          std::string("Unsupported operation: ").append(EnumNameBuiltinOperator(builtin_op)));
  }

  loadOperationTo<ir::operation::Comparison>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadEinsum(const Operator *op, ir::Graph &subg)
{
  ir::operation::Einsum::Param param;
  if (op->custom_options() == nullptr)
  {
    throw std::runtime_error{"Einsum: empty equation"};
  }
  else
  {
    size_t custom_op_data_size = op->custom_options()->size();
    auto custom_op_data = op->custom_options()->Data();
    auto data_root = flexbuffers::GetRoot(custom_op_data, custom_op_data_size);
    auto attr_map = data_root.AsMap();
    param.equation = attr_map["equation"].ToString();
  }

  const auto es = loadOperationTo<ir::operation::Einsum>(op, subg, param);
  if (es->getInputs().size() != 2)
  {
    throw std::runtime_error{"Einsum: NYI input - only support two inputs"};
  }
}
template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadFusedBatchNorm(const Operator *op, ir::Graph &subg)
{
  ir::operation::FusedBatchNorm::Param param;
  if (op->custom_options() == nullptr)
  {
    throw std::runtime_error{"FusedBatchNorm: empty option"};
  }
  else
  {
    size_t custom_op_data_size = op->custom_options()->size();
    auto custom_op_data = op->custom_options()->Data();
    auto data_root = flexbuffers::GetRoot(custom_op_data, custom_op_data_size);
    auto attr_map = data_root.AsMap();
    param.is_training = attr_map["is_training"].AsBool();
    param.epsilon = attr_map["epsilon"].AsFloat();
    param.data_format = attr_map["data_format"].ToString();
  }

  const auto fbn = loadOperationTo<ir::operation::FusedBatchNorm>(op, subg, param);

  if (fbn->getInputs().size() != 5)
  {
    throw std::runtime_error{"FusedBatchNorm: NYI input - only support five inputs"};
  }
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadOneHot(const Operator *op, ir::Graph &subg)
{
  if (op->inputs()->size() != 4 || op->outputs()->size() != 1)
    throw std::runtime_error("OneHot Op has wrong number of input or output tensors.");

  // Set parameter
  ir::operation::OneHot::Param param;
  param.axis = op->builtin_options_as_OneHotOptions()->axis();

  loadOperationTo<ir::operation::OneHot>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadIf(const Operator *op, ir::Graph &subg)
{
  ir::operation::If::Param param;
  const auto *options = op->builtin_options_as_IfOptions();
  const uint32_t then_index = options->then_subgraph_index();
  const uint32_t else_index = options->else_subgraph_index();
  param.then_subg_index = ir::SubgraphIndex{then_index};
  param.else_subg_index = ir::SubgraphIndex{else_index};

  loadOperationTo<ir::operation::If>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadWhile(const Operator *op, ir::Graph &subg)
{
  ir::operation::While::Param param;
  const auto *options = op->builtin_options_as_WhileOptions();
  const uint32_t cond_index = options->cond_subgraph_index();
  const uint32_t body_index = options->body_subgraph_index();
  param.cond_subg_index = ir::SubgraphIndex{cond_index};
  param.body_subg_index = ir::SubgraphIndex{body_index};

  loadOperationTo<ir::operation::While>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadArgMax(const Operator *op, ir::Graph &subg)
{
  ir::operation::ArgMax::Param param;
  const auto output_type = op->builtin_options_as_ArgMaxOptions()->output_type();
  switch (output_type)
  {
    case TensorType::TensorType_INT32:
    case TensorType::TensorType_INT64:
      param.output_type = tensorTypeToDataType(output_type);
      break;
    default:
      throw std::runtime_error("ArgMax: `output_type` must be either int32 or int64.");
  }
  auto am = loadOperationTo<ir::operation::ArgMax>(op, subg, param);

  auto axisOperand = subg.operands().at(am->getInputs().at(ir::operation::ArgMax::Input::AXIS));
  if (!(axisOperand.operandSize() == 4 && (axisOperand.typeInfo().type() == ir::DataType::INT32 ||
                                           axisOperand.typeInfo().type() == ir::DataType::INT64)))
    throw std::runtime_error("ArgMax: `axis` with an int32 or int64 element is only supported.");
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadTile(const Operator *op, ir::Graph &subg)
{
  ir::OperandIndexSequence inputs;
  ir::OperandIndexSequence outputs;

  loadOperationIO(op, inputs, outputs);

  auto multiples = inputs.at(ir::operation::Tile::MULTIPLES);

  // FIXME Handle TileOptions
  if (!subg.operands().at(multiples).isConstant())
    throw std::runtime_error("Tile: non-constant 'multiples' is not supported.");

  std::unique_ptr<ir::Operation> new_op(new ir::operation::Tile(inputs, outputs));
  subg.addOperation(std::move(new_op));
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadLogSoftmax(const Operator *op, ir::Graph &subg)
{
  ir::operation::LogSoftmax::Param param;
  // In tflite, beta is fixed to 1.0 and axis is fixed to -1.
  param.beta = 1.0f;
  param.axis = -1;

  loadOperationTo<ir::operation::LogSoftmax>(op, subg, param);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadLeakyRelu(const Operator *op, ir::Graph &subg)
{
  float alpha = op->builtin_options_as_LeakyReluOptions()->alpha();
  loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::LEAKY_RELU, alpha,
                            1.f);
}

template <typename LoaderDomain>
void BaseLoader<LoaderDomain>::loadOperation(const Operator *op, ir::Graph &subg)
{
  const auto builtin_op = _model->operator_codes()->Get(op->opcode_index())->builtin_code();

  switch (builtin_op)
  {
    case BuiltinOperator::BuiltinOperator_CONV_2D:
      loadConv2D(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_AVERAGE_POOL_2D:
      loadPool2D(op, subg, ir::operation::Pool2D::PoolType::AVG);
      return;
    case BuiltinOperator::BuiltinOperator_DEPTHWISE_CONV_2D:
      loadDepthwiseConv2D(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_TRANSPOSE_CONV:
      loadTransposeConv(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RESHAPE:
      loadReshape(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SOFTMAX:
      loadSoftmax(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_MAX_POOL_2D:
      loadPool2D(op, subg, ir::operation::Pool2D::PoolType::MAX);
      return;
    case BuiltinOperator::BuiltinOperator_CONCATENATION:
      loadConcatenation(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_FULLY_CONNECTED:
      loadFC(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_ADD:
      loadBinaryArithmetic(op, subg, ir::operation::BinaryArithmetic::ArithmeticType::ADD);
      return;
    case BuiltinOperator::BuiltinOperator_SUB:
      loadBinaryArithmetic(op, subg, ir::operation::BinaryArithmetic::ArithmeticType::SUB);
      return;
    case BuiltinOperator::BuiltinOperator_MUL:
      loadBinaryArithmetic(op, subg, ir::operation::BinaryArithmetic::ArithmeticType::MUL);
      return;
    case BuiltinOperator::BuiltinOperator_DIV:
      loadBinaryArithmetic(op, subg, ir::operation::BinaryArithmetic::ArithmeticType::DIV);
      return;
    case BuiltinOperator::BuiltinOperator_PACK:
      loadPack(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RELU:
      loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::RELU,
                                ir::operation::ElementwiseActivation::infinity, 0.f);
      return;
    case BuiltinOperator::BuiltinOperator_RELU_N1_TO_1:
      loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::RELU, 1.f,
                                -1.f);
      return;
    case BuiltinOperator::BuiltinOperator_RELU6:
      loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::RELU, 6.f,
                                0.f);
      return;
    case BuiltinOperator::BuiltinOperator_RESIZE_BILINEAR:
      loadResizeBilinear(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RESIZE_NEAREST_NEIGHBOR:
      loadResizeNearestNeighbor(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RSQRT:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::RSQRT);
      return;
    case BuiltinOperator::BuiltinOperator_SELECT:
    case BuiltinOperator::BuiltinOperator_SELECT_V2:
      loadOperationTo<ir::operation::Select>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SQRT:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::SQRT);
      return;
    case BuiltinOperator::BuiltinOperator_SQUARED_DIFFERENCE:
      loadOperationTo<ir::operation::SquaredDifference>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_TANH:
      loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::TANH, 1.f,
                                1.f);
      return;
    case BuiltinOperator::BuiltinOperator_TRANSPOSE:
      loadTranspose(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_MEAN:
      loadReduce(op, subg, ir::operation::Reduce::ReduceType::MEAN);
      return;
    case BuiltinOperator::BuiltinOperator_REDUCE_ANY:
      loadReduce(op, subg, ir::operation::Reduce::ReduceType::ANY);
      return;
    case BuiltinOperator::BuiltinOperator_REDUCE_MAX:
      loadReduce(op, subg, ir::operation::Reduce::ReduceType::MAX);
      return;
    case BuiltinOperator::BuiltinOperator_REVERSE_V2:
      loadOperationTo<ir::operation::Reverse>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_PAD:
    case BuiltinOperator::BuiltinOperator_PADV2:
      loadOperationTo<ir::operation::Pad>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_LOGISTIC:
      loadElementwiseActivation(op, subg, ir::operation::ElementwiseActivation::Type::LOGISTIC);
      return;
    case BuiltinOperator::BuiltinOperator_EXP:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::EXP);
      return;
    case BuiltinOperator::BuiltinOperator_EXPAND_DIMS:
      loadOperationTo<ir::operation::ExpandDims>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_GATHER:
      loadGather(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SPACE_TO_BATCH_ND:
      loadOperationTo<ir::operation::SpaceToBatchND>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_BATCH_TO_SPACE_ND:
      loadOperationTo<ir::operation::BatchToSpaceND>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SUM:
      loadReduce(op, subg, ir::operation::Reduce::ReduceType::SUM);
      return;
    case BuiltinOperator::BuiltinOperator_CUSTOM:
      loadCustom(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SQUEEZE:
      loadSqueeze(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_PRELU:
      loadOperationTo<ir::operation::PReLU>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SPLIT:
      loadSplit(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SPLIT_V:
      loadSplitV(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_SLICE:
      loadOperationTo<ir::operation::Slice>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_STRIDED_SLICE:
      loadStridedSlice(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_UNPACK:
      loadUnpack(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_MINIMUM:
      loadElementwiseBinary(op, subg, ir::operation::ElementwiseBinary::ElementwiseBinaryType::MIN);
      return;
    case BuiltinOperator::BuiltinOperator_MAXIMUM:
      loadElementwiseBinary(op, subg, ir::operation::ElementwiseBinary::ElementwiseBinaryType::MAX);
      return;
    case BuiltinOperator::BuiltinOperator_CAST:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::CAST);
      return;
    case BuiltinOperator::BuiltinOperator_EQUAL:
    case BuiltinOperator::BuiltinOperator_NOT_EQUAL:
    case BuiltinOperator::BuiltinOperator_GREATER_EQUAL:
    case BuiltinOperator::BuiltinOperator_GREATER:
    case BuiltinOperator::BuiltinOperator_LESS_EQUAL:
    case BuiltinOperator::BuiltinOperator_LESS:
      loadComparison(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_ONE_HOT:
      loadOneHot(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_ABS:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::ABS);
      return;
    case BuiltinOperator::BuiltinOperator_COS:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::COS);
      return;
    case BuiltinOperator::BuiltinOperator_SIN:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::SIN);
      return;
    case BuiltinOperator::BuiltinOperator_SHAPE:
      loadOperationTo<ir::operation::Shape>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_REDUCE_PROD:
      loadReduce(op, subg, ir::operation::Reduce::ReduceType::PROD);
      return;
    case BuiltinOperator::BuiltinOperator_IF:
      loadIf(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_WHILE:
      loadWhile(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_NEG:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::NEG);
      return;
    case BuiltinOperator::BuiltinOperator_ARG_MAX:
      loadArgMax(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_LOG:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::LOG);
      return;
    case BuiltinOperator::BuiltinOperator_ROUND:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::ROUND);
      return;
    case BuiltinOperator::BuiltinOperator_POW:
      loadOperationTo<ir::operation::Pow>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_LOGICAL_NOT:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::LOGICAL_NOT);
      return;
    case BuiltinOperator::BuiltinOperator_LOGICAL_OR:
      loadElementwiseBinary(op, subg,
                            ir::operation::ElementwiseBinary::ElementwiseBinaryType::LOGICAL_OR);
      return;
    case BuiltinOperator::BuiltinOperator_FILL:
      loadOperationTo<ir::operation::Fill>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_ZEROS_LIKE:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::ZEROS_LIKE);
      return;
    case BuiltinOperator::BuiltinOperator_TILE:
      loadTile(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RANGE:
      loadOperationTo<ir::operation::Range>(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_BATCH_MATMUL:
      loadBatchMatMul(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_LOG_SOFTMAX:
      loadLogSoftmax(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_QUANTIZE:
      loadElementwiseUnary(op, subg, ir::operation::ElementwiseUnary::Type::QUANTIZE);
      return;
    case BuiltinOperator::BuiltinOperator_SPACE_TO_DEPTH:
      loadSpaceToDepth(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_L2_NORMALIZATION:
      loadOperationTo<ir::operation::L2Normalization>(op, subg);
      break;
    case BuiltinOperator::BuiltinOperator_LEAKY_RELU:
      loadLeakyRelu(op, subg);
      return;
    case BuiltinOperator::BuiltinOperator_RANK:
      loadOperationTo<ir::operation::Rank>(op, subg);
      return;
    default:
      throw std::runtime_error(
          std::string("Unsupported operation: ").append(EnumNameBuiltinOperator(builtin_op)));
  }
}

template <typename LoaderDomain> void BaseLoader<LoaderDomain>::loadModel()
{
  LoaderDomain::VerifyModelBuffer(*_verifier.get());
  _model = LoaderDomain::GetModel(_base);
  // Version unused
  // const auto version = _model->version();
  // Description unused
  // const auto *description = _model->description();
  // Metabuffer unsued
  // const auto *metadata_buffer = _model->metadata_buffer();
  // Load subgraphs and map operations on subgraph
  const auto domain_subgraphs = _model->subgraphs();
  auto subgraphs = std::make_unique<ir::Subgraphs>();
  for (uint32_t subgraph_index = 0; subgraph_index < domain_subgraphs->size(); ++subgraph_index)
  {
    auto subg = loadSubgraph((*_model->subgraphs())[subgraph_index]);
    subgraphs->push(ir::SubgraphIndex{subgraph_index}, std::move(subg));
  }
  _subgraphs = std::move(subgraphs);
}

} // namespace base_loader
} // namespace onert

#endif //__BASE_LOADER_BASE_LOADER_H__
