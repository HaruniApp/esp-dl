#pragma once

#include "dl_module_base.hpp"
#include "dl_base_add2d.hpp"

namespace dl {
namespace module {
/**
 * NOTE: addition is element-wise, i.e., output[i,j,k] = input0[i,j,k] + input1[i,j,k]
 *
 * @tparam feature_t supports int16_t and int8_t,
 *         - int16_t: stands for operation in int16_t quantize
 *         - int8_t: stands for operation in int8_t quantize
 */
class Add2D : public Module {
public:
    /**
     * @brief Construct a new Add2D object.
     *
     * @param name            name of module
     * @param inplace         true: the output will store to input0
     *                        false: the output will store to a separate memory
     */
    Add2D(const char *name = NULL, bool inplace = false, quant_type_t quant_type = QUANT_TYPE_NONE) :
        Module(name, inplace, quant_type)
    {
    }

    /**
     * @brief Destroy the Add2D object.
     */
    ~Add2D() {}

    std::vector<std::vector<int>> get_output_shape(std::vector<std::vector<int>> &input_shapes)
    {
        assert(input_shapes.size() == 2);
        assert(input_shapes[0].size() == 3 && input_shapes[1].size() == 3);

        std::vector<std::vector<int>> shapes;
        if (input_shapes[0][2] >= input_shapes[1][2])
            shapes = std::vector<std::vector<int>>(1, input_shapes[0]);
        else
            shapes = std::vector<std::vector<int>>(1, input_shapes[1]);
        
        return shapes;
    }

    void forward(std::vector<dl::TensorBase *> &tensors, runtime_mode_t mode)
    {
        // DL_LOG_LAYER_LATENCY_INIT();
        // DL_LOG_LAYER_LATENCY_START();
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            forward_template<int8_t>(tensors, mode);
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            forward_template<int16_t>(tensors, mode);
        }
        // DL_LOG_LAYER_LATENCY_END(this->name, "Add2D");
    }

    void forward_args(void *args) 
    {
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            base::add2d<int8_t>(args);
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            base::add2d<int16_t>(args);
        }
    }

    template <typename T>
    void forward_template(std::vector<TensorBase *> &tensors, runtime_mode_t mode)
    {
        TensorBase *input0 = tensors[m_inputs_index[0]];
        TensorBase *input1 = tensors[m_inputs_index[1]];
        TensorBase *output = tensors[m_outputs_index[0]];

        std::vector<base::arithArgsType<T>> m_args = base::get_arith_operation_args<T>(output, 
                                                                                       input0, 
                                                                                       input1, 
                                                                                       Linear,
                                                                                       nullptr,
                                                                                       mode);
        int task_size = m_args.size();
        if (task_size == 1) { // single task
            forward_args((void *)&m_args[0]);
        } else if (task_size == 2) { // multi task, use semaphore to maintain synchronization.
            module_forward_dual_core(this, (void *)&m_args[0], (void *)&m_args[1]);
        } else {
            ESP_LOGE("Add2D", "Only support task size is 1 or 2, currently task size is %d", task_size);
        }
    }

    /**
     * @brief deserialize Add module instance by node serialization information
     */
    static Module *deserialize(fbs::FbsModel *fbs_model, std::string node_name)
    {
        Module *op = nullptr;
        quant_type_t quant_type;
        fbs_model->get_operation_attribute(node_name, "quant_type", quant_type);

        // Create module
        if (quant_type == QUANT_TYPE_SYMM_8BIT || quant_type == QUANT_TYPE_SYMM_16BIT) {
            op = new Add2D(NULL, true, quant_type);
        }
        return op;
    }

    void print() 
    {
        ESP_LOGI("Add2D", "quant_type: %s.", quant_type_to_string(quant_type));
    }
};
} // namespace module
} // namespace dl
