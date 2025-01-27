// MIT License
//
// Copyright (c) 2020 Sergio Izquierdo
// Copyright (c) 2020 CarlPoirier
// Copyright (c) 2020 Jiannan Liu
// Copyright (c) 2020 liufeng27
// Copyright (c) 2022 Alfredo Rodriguez
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*!
 *  @file       tensor.h
 *  @author     Alfredo Rodriguez
 *  @author     CarlPoirier
 *  @author     Jiannan Liu
 *  @author     liufeng27
 *  @author     Sergio Izquierdo
 *  @date       @showdate "%B %d, %Y" 2020-06-27
 */

#ifndef INCLUDE_CPPFLOW_TENSOR_H_
#define INCLUDE_CPPFLOW_TENSOR_H_

// C headers
#include <tensorflow/c/tf_tensor.h>
#include <tensorflow/c/eager/c_api.h>

// C++ headers
#include <memory>
#include <vector>
#include <cstring>
#include <string>
#include <span>
#include <ranges>
#include <numeric>

// CppFlow headers
#include "cppflow/context.h"
#include "cppflow/datatype.h"

namespace cppflow {

template<typename T>
concept contiguous_sized_range = std::ranges::contiguous_range<T> && std::ranges::sized_range<T>;

/**
 * @class tensor
 * @brief A TensorFlow eager tensor wrapper
 *
 */
class tensor {
 public:
  tensor()= default;

  /**
   * Creates a tensor with the given values and specified shape
   * @tparam T A type that can be convertible into a tensor
   * @param values The values to be converted (in a flattened version)
   * @param shape The shape of the converted tensor
   */
  template<contiguous_sized_range ValueRange, contiguous_sized_range ShapeRange>
  requires supported_tf_type<typename ValueRange::value_type> && std::is_same_v<typename ShapeRange::value_type, int64_t>
  tensor(const ValueRange& values, const ShapeRange& shape);

  /**
   * Creates a flat tensor with the given values
   * @tparam T A type that can be convertible into a tensor
   * @param values The values to be converted
   */
  template<contiguous_sized_range ValueRange>
  requires supported_tf_type<typename ValueRange::value_type>
  tensor(const ValueRange& values);

  /**
   * Creates a tensor with the given value
   * @tparam T A type that can be convertible into a tensor
   * @param value The value to be converted
   */
  template<supported_tf_type T>
  tensor(const T& value);

  template<supported_tf_type T, contiguous_sized_range ShapeRange>
  requires std::is_same_v<typename ShapeRange::value_type, int64_t>
  tensor(T dummy, const ShapeRange& shape);

  tensor(const tensor &tensor) = default;
  tensor(tensor &&tensor) = default;
  explicit tensor(TFE_TensorHandle* handle);
  explicit tensor(TF_Tensor* t);

  ~tensor() = default;

  tensor &operator=(const tensor &other) = default;
  tensor &operator=(tensor &&other) = default;

  /**
   * @return Shape of the tensor
   */
  tensor shape() const;

  /**
   * @param on_memory If false, the function will return the name of the device that produced the tensor.
   * If true, the function will return the name of the device in whose memory the tensor resides
   * @return Returns the name of the device of the tensor
   */
  std::string device(bool on_memory = false) const;


  /**
   * @return The tensor datatype
   */
  datatype dtype() const;

  /**
   * Converts the tensor into a C++ vector
   * @tparam T The c++ type (must be equivalent to the tensor type)
   * @return A vector representing the flat tensor
   */
  template<typename T>
  std::vector<T> get_data() const;

  /**
   * Converts the tensor into a C++ vector
   * @tparam T The c++ type (must be equivalent to the tensor type)
   * @return A vector representing the flat tensor
   */
  template<typename T>
  std::span<T> get_data_view() &;

  /**
   * Converts the tensor into a C++ vector
   * @tparam T The c++ type (must be equivalent to the tensor type)
   * @return A vector representing the flat tensor
   */
  template<typename T>
  std::span<const T> get_data_view() const &;

  /* This has no implementaion because it would lead to dangling pointers*/
  template<typename T>
  std::span<T> get_data_view() const &&;

  // NOTE:
  //    Usually, one should not call get_eager_handle() or get_tensor() below.
  //    They are designed for implementation details in cppflow.
  //    If you are calling them directly, it is likely that you are using some
  //    tenforflow APIs not supported in cppflow.

  // Additional NOTE:
  //    TF_Tensor is an immutable tensor inside tensorflow.
  //    TFE_TensorHandle is a TF_Tensor and the associated device,
  //        plus some data cache

  // @todo Need to determine if we can mark the return value or *this as const
  std::shared_ptr<TFE_TensorHandle> get_eager_handle() const {
    return tfe_handle;
  }

  // Get the TF_Tensor data from the eager handle
  // Call `get_data<T>()` instead if possible
  // NOTE:
  //    Changes to the returned TF_Tensor may not be reflected in the
  //        actual device memory!
  //    Do *NOT* modify the returned TF_Tensor!
  //    See comments of `tf_tensor` for more details.
  std::shared_ptr<TF_Tensor> get_tensor() const;

  // DO NOT directly access this member, call get_eager_handle() instead
  // @todo This is kept as public to be compatible with existing code and
  //       should be mark as private
  std::shared_ptr<TFE_TensorHandle> tfe_handle;

 private:
  template<contiguous_sized_range ShapeRange>
  requires std::is_same_v<typename ShapeRange::value_type, int64_t>
  tensor(enum TF_DataType type, const void* data, size_t len,
         const ShapeRange& shape);

  // This member serves as a local cache of the data in tfe_handle.
  // It refers to `local_mirrors_` if on device, or `data_` if on host CPU.
  // Changes to this variable may not be reflected in the actual device memory,
  // e.g. on GPUs or on remote nodes.
  // Access it via get_tensor() if not in constructor
  mutable std::shared_ptr<TF_Tensor> tf_tensor;
};

}  // namespace cppflow


/******************************
 *   IMPLEMENTATION DETAILS   *
 ******************************/


namespace cppflow {

template<contiguous_sized_range ShapeRange>
requires std::is_same_v<typename ShapeRange::value_type, int64_t>
inline tensor::tensor(enum TF_DataType type, const void *data, size_t len,
                      const ShapeRange& shape) {
  this->tf_tensor = {TF_AllocateTensor(type, shape.data(),
                                       static_cast<int>(shape.size()), len),
                     TF_DeleteTensor};
  memcpy(TF_TensorData(this->tf_tensor.get()), data,
         TF_TensorByteSize(this->tf_tensor.get()));
  this->tfe_handle = {TFE_NewTensorHandle(this->tf_tensor.get(),
                                          context::get_status()),
                      TFE_DeleteTensorHandle};
  status_check(context::get_status());
}

template<contiguous_sized_range ValueRange, contiguous_sized_range ShapeRange>
requires supported_tf_type<typename ValueRange::value_type> && std::is_same_v<typename ShapeRange::value_type, int64_t>
tensor::tensor(const ValueRange& values, const ShapeRange& shape)
    : tensor(deduce_tf_type<typename ValueRange::value_type>(), values.data(),
             values.size() * sizeof(typename ValueRange::value_type), shape) {}

template<contiguous_sized_range ValueRange>
requires supported_tf_type<typename ValueRange::value_type>
tensor::tensor(const ValueRange& values)
    : tensor(values, std::array{static_cast<int64_t>(values.size())}) {}

template<supported_tf_type T>
tensor::tensor(const T& value)
    : tensor(std::array{value}, std::vector<int64_t>{}) {}

template<supported_tf_type T, contiguous_sized_range ShapeRange>
requires  std::is_same_v<typename ShapeRange::value_type, int64_t>
tensor::tensor(T, const ShapeRange& shape)
{
  const size_t len{std::accumulate(shape.begin(), shape.end(), size_t(1), std::multiplies<size_t>{}) * sizeof(T)};
  this->tf_tensor = {TF_AllocateTensor(deduce_tf_type<T>(), shape.data(),
                                       static_cast<int>(shape.size()), len),
                     TF_DeleteTensor};
  this->tfe_handle = {TFE_NewTensorHandle(this->tf_tensor.get(),
                                          context::get_status()),
                      TFE_DeleteTensorHandle};
  status_check(context::get_status());
}

inline tensor::tensor(TFE_TensorHandle* handle) {
  this->tfe_handle = {handle, TFE_DeleteTensorHandle};
}

inline tensor::tensor(TF_Tensor* t) {
  this->tf_tensor = {t, TF_DeleteTensor};
  this->tfe_handle = {TFE_NewTensorHandle(this->tf_tensor.get(),
                                          context::get_status()),
                      TFE_DeleteTensorHandle};
  status_check(context::get_status());
}

inline tensor tensor::shape() const {
  auto op = TFE_NewOp(context::get_context(), "Shape", context::get_status());
  status_check(context::get_status());

  TFE_OpAddInput(op, this->tfe_handle.get(), context::get_status());
  status_check(context::get_status());

  // Output type should be int64_t
  TFE_OpSetAttrType(op, "out_type", cppflow::datatype::TF_INT64);

  TFE_OpSetDevice(op, "/device:CPU:0", context::get_status());
  status_check(context::get_status());

  // EXECUTE
  int n = 1;
  TFE_TensorHandle* res[1] = { nullptr };
  TFE_Execute(op, res, &n, context::get_status());
  status_check(context::get_status());
  TFE_DeleteOp(op);

  return tensor(res[0]);
}

inline std::string tensor::device(bool on_memory) const {
  std::string res;
  if (on_memory)
    res = TFE_TensorHandleBackingDeviceName(this->tfe_handle.get(),
                                            context::get_status());
  else
    res = std::string(TFE_TensorHandleDeviceName(this->tfe_handle.get(),
                                                 context::get_status()));

  status_check(context::get_status());
  return res;
}

template<typename T>
std::vector<T> tensor::get_data() const {
  // Check if asked datatype and tensor datatype match
  if (this->dtype() != deduce_tf_type<T>()) {
    auto type1 = cppflow::to_string(deduce_tf_type<T>());
    auto type2 = cppflow::to_string(this->dtype());
    auto error = "Datatype in function get_data (" + type1 +
                 ") does not match tensor datatype (" + type2 + ")";
    throw std::runtime_error(error);
  }

  auto res_tensor = get_tensor();

  // Check tensor data is not empty
  auto raw_data = TF_TensorData(res_tensor.get());
  // this->error_check(raw_data != nullptr, "Tensor data is empty");

  size_t size = (TF_TensorByteSize(res_tensor.get()) /
                 TF_DataTypeSize(TF_TensorType(res_tensor.get())));

  // Convert to correct type
  const auto T_data = static_cast<T*>(raw_data);
  return std::vector<T>(T_data, T_data + size);
}

template<typename T>
std::span<T> tensor::get_data_view() &
{
  // Check if asked datatype and tensor datatype match
  if (this->dtype() != deduce_tf_type<T>()) {
    auto type1 = cppflow::to_string(deduce_tf_type<T>());
    auto type2 = cppflow::to_string(this->dtype());
    auto error = "Datatype in function get_data (" + type1 +
                 ") does not match tensor datatype (" + type2 + ")";
    throw std::runtime_error(error);
  }

  auto res_tensor = get_tensor();
  auto raw_data = TF_TensorData(res_tensor.get());
  const size_t size = (TF_TensorByteSize(res_tensor.get()) /
                 TF_DataTypeSize(TF_TensorType(res_tensor.get())));
  return std::span<T>(static_cast<T*>(raw_data), size);
}

template<typename T>
std::span<const T> tensor::get_data_view() const & {
  // Check if asked datatype and tensor datatype match
  if (this->dtype() != deduce_tf_type<T>()) {
    auto type1 = cppflow::to_string(deduce_tf_type<T>());
    auto type2 = cppflow::to_string(this->dtype());
    auto error = "Datatype in function get_data (" + type1 +
                 ") does not match tensor datatype (" + type2 + ")";
    throw std::runtime_error(error);
  }

  auto res_tensor = get_tensor();
  auto raw_data = TF_TensorData(res_tensor.get());
  const size_t size = (TF_TensorByteSize(res_tensor.get()) /
                 TF_DataTypeSize(TF_TensorType(res_tensor.get())));
  return std::span<const T>(static_cast<T*>(raw_data), size);
}

inline datatype tensor::dtype() const {
  return TFE_TensorHandleDataType(this->tfe_handle.get());
}

// NOTE:
//    Changes to the returned TF_Tensor are not reflected in
//    the actual device memory!
inline std::shared_ptr<TF_Tensor> tensor::get_tensor() const {
  if (!tf_tensor) {
    tf_tensor = {TFE_TensorHandleResolve(tfe_handle.get(),
                                         context::get_status()),
                 TF_DeleteTensor};
    status_check(context::get_status());
  }
  return tf_tensor;
}
}  // namespace cppflow

#endif  // INCLUDE_CPPFLOW_TENSOR_H_
