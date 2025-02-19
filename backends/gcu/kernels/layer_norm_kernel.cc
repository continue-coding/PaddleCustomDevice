// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/gcu_op_runner.h"
#include "kernels/funcs/gcu_kernel_funcs.h"

namespace custom_kernel {
template <typename T, typename Context>
void LayerNormKernel(const Context& dev_ctx,
                     const phi::DenseTensor& x,
                     const paddle::optional<phi::DenseTensor>& scale_opt,
                     const paddle::optional<phi::DenseTensor>& bias_opt,
                     float epsilon,
                     int begin_norm_axis,
                     phi::DenseTensor* out,
                     phi::DenseTensor* mean,
                     phi::DenseTensor* variance) {
  PADDLE_GCU_KERNEL_TRACE("layer_norm");

  int32_t mean_size = 1;
  auto mean_dims = mean->dims();
  int32_t mean_dims_size = mean_dims.size();
  for (int32_t i = 0; i < mean_dims_size; ++i) {
    mean_size *= mean_dims[i];
  }
  mean->Resize({mean_size});

  int32_t variance_size = 1;
  auto variance_dims = mean->dims();
  int32_t variance_dims_size = variance_dims.size();
  for (int32_t i = 0; i < variance_dims_size; ++i) {
    variance_size *= variance_dims[i];
  }
  variance->Resize({variance_size});

  dev_ctx.template Alloc<T>(out);
  dev_ctx.template Alloc<T>(mean);
  dev_ctx.template Alloc<T>(variance);

  if (LaunchAOTKernel()) {
    THROW_AOT_UNIMPLEMENTED();
  } else {  // kernel impl base on JIT
    TensorNameMap input_names;
    input_names["X"] = {"x"};

    TensorValueMap inputs;
    inputs["X"] = {const_cast<DenseTensor*>(&x)};

    if (scale_opt) {
      input_names["Scale"] = {"scale"};
      inputs["Scale"] = {const_cast<DenseTensor*>(scale_opt.get_ptr())};
    }
    if (bias_opt) {
      input_names["Bias"] = {"bias"};
      inputs["Bias"] = {const_cast<DenseTensor*>(bias_opt.get_ptr())};
    }

    TensorNameMap output_names;
    output_names["Y"] = {"y"};
    output_names["Mean"] = {"mean"};
    output_names["Variance"] = {"variance"};

    TensorValueMap outputs;
    outputs["Y"] = {out};
    outputs["Mean"] = {mean};
    outputs["Variance"] = {variance};

    GcuAttributeMap attrs;
    attrs["epsilon"] = epsilon;
    attrs["begin_norm_axis"] = begin_norm_axis;

    GcuRunner(input_names,
              inputs,
              output_names,
              outputs,
              attrs,
              "layer_norm",
              dev_ctx);
  }
}

template <typename T, typename Context>
void LayerNormGradKernel(const Context& dev_ctx,
                         const phi::DenseTensor& x,
                         const paddle::optional<phi::DenseTensor>& scale_opt,
                         const paddle::optional<phi::DenseTensor>& bias,
                         const phi::DenseTensor& mean,
                         const phi::DenseTensor& variance,
                         const phi::DenseTensor& out_grad,
                         float epsilon,
                         int begin_norm_axis,
                         phi::DenseTensor* x_grad,
                         phi::DenseTensor* scale_grad,
                         phi::DenseTensor* bias_grad) {
  PADDLE_GCU_KERNEL_TRACE("layer_norm_grad");
  dev_ctx.template Alloc<T>(x_grad);

  if (LaunchAOTKernel()) {
    THROW_AOT_UNIMPLEMENTED();
  } else {  // kernel impl base on JIT
    TensorNameMap input_names;
    input_names["X"] = {"x"};
    input_names["Mean"] = {"mean"};
    input_names["Variance"] = {"variance"};
    input_names[GradVarName("Y")] = {"y_grad"};

    TensorValueMap inputs;
    inputs["X"] = {const_cast<DenseTensor*>(&x)};
    inputs["Mean"] = {const_cast<DenseTensor*>(&mean)};
    inputs["Variance"] = {const_cast<DenseTensor*>(&variance)};
    inputs[GradVarName("Y")] = {const_cast<DenseTensor*>(&out_grad)};

    if (scale_opt) {
      input_names["Scale"] = {"scale"};
      inputs["Scale"] = {const_cast<DenseTensor*>(scale_opt.get_ptr())};
    }
    if (bias) {
      input_names["Bias"] = {"bias"};
      inputs["Bias"] = {const_cast<DenseTensor*>(bias.get_ptr())};
    }

    TensorNameMap output_names;
    output_names[GradVarName("X")] = {"x_grad"};

    TensorValueMap outputs;
    outputs[GradVarName("X")] = {x_grad};

    if (scale_grad != nullptr) {
      dev_ctx.template Alloc<T>(scale_grad);
      output_names[GradVarName("Scale")] = {"scale_grad"};
      outputs[GradVarName("Scale")] = {scale_grad};
    }
    if (bias_grad != nullptr) {
      dev_ctx.template Alloc<T>(bias_grad);
      output_names[GradVarName("Bias")] = {"bias_grad"};
      outputs[GradVarName("Bias")] = {bias_grad};
    }

    GcuAttributeMap attrs;
    attrs["epsilon"] = epsilon;
    attrs["begin_norm_axis"] = begin_norm_axis;

    GcuRunner(input_names,
              inputs,
              output_names,
              outputs,
              attrs,
              "layer_norm_grad",
              dev_ctx);
  }
}
}  // namespace custom_kernel

PD_REGISTER_PLUGIN_KERNEL(layer_norm,
                          gcu,
                          ALL_LAYOUT,
                          custom_kernel::LayerNormKernel,
                          float,
                          phi::dtype::float16) {}

PD_REGISTER_PLUGIN_KERNEL(layer_norm_grad,
                          gcu,
                          ALL_LAYOUT,
                          custom_kernel::LayerNormGradKernel,
                          float,
                          phi::dtype::float16) {}
