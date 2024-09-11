#pragma once

#include "dl_math.hpp"
#include "dl_module_base.hpp"
#include "dl_sigmoid_lut.hpp"

namespace dl {
namespace module {
/**
 * NOTE:
 *
 * @tparam feature_t supports int16_t and int8_t,
 *         - int16_t: stands for operation in int16_t quantize
 *         - int8_t: stands for operation in int8_t quantize
 */
class Sigmoid : public Module {
private:
    const int8_t *sigmoid_lut; /*sigmoid loop up table*/
public:
    /**
     * @brief Construct a new Sigmoid object.
     *
     * @param name            name of module
     * @param inplace         inplace type.
     */
    Sigmoid(const char *name = NULL,
            module_inplace_t inplace = MODULE_NON_INPLACE,
            quant_type_t quant_type = QUANT_TYPE_NONE) :
        Module(name, inplace, quant_type)
    {
    }

    /**
     * @brief Destroy the Sigmoid object.
     */
    ~Sigmoid() {}

    std::vector<std::vector<int>> get_output_shape(std::vector<std::vector<int>> &input_shapes)
    {
        assert(input_shapes.size() == 1);
        std::vector<std::vector<int>> output_shapes(1, input_shapes[0]);
        return output_shapes;
    }

    void forward(std::vector<dl::TensorBase *> &tensors, runtime_mode_t mode)
    {
        DL_LOG_LAYER_LATENCY_INIT();
        DL_LOG_LAYER_LATENCY_START();
        if (quant_type == QUANT_TYPE_SYMM_8BIT) {
            TensorBase *input = tensors[m_inputs_index[0]];
            TensorBase *output = tensors[m_outputs_index[0]];
            int8_t *input_ptr = (int8_t *)input->get_element_ptr();
            int8_t *output_ptr = (int8_t *)output->get_element_ptr();

            if (output->exponent == -7) {
                // use lut
                const int8_t *sigmoid_lut = dl::lut::get_sigmoid_lut(input->exponent);
                for (size_t i = 0; i < input->size; i++) {
                    *output_ptr++ = sigmoid_lut[*input_ptr++ + 128];
                }
            } else {
                float input_scale = DL_SCALE(input->exponent);
                float output_scale = DL_RESCALE(output->exponent);
                for (size_t i = 0; i < input->size; i++) {
                    float temp = dl::math::sigmoid((float)input_ptr[i] * input_scale);
                    dl::tool::truncate(output_ptr[i], round_half_even(temp * output_scale));
                }
            }
        } else if (quant_type == QUANT_TYPE_SYMM_16BIT) {
            TensorBase *input = tensors[m_inputs_index[0]];
            TensorBase *output = tensors[m_outputs_index[0]];
            int16_t *input_ptr = (int16_t *)input->get_element_ptr();
            int16_t *output_ptr = (int16_t *)output->get_element_ptr();

            float input_scale = DL_SCALE(input->exponent);
            float output_scale = DL_RESCALE(output->exponent);
            for (size_t i = 0; i < input->size; i++) {
                float temp = dl::math::sigmoid((float)input_ptr[i] * input_scale);
                dl::tool::truncate(output_ptr[i], round_half_even(temp * output_scale));
            }
        }
        DL_LOG_LAYER_LATENCY_END(this->name, "Sigmoid");
    }

    void forward_args(void *args) {}

    /**
     * @brief deserialize Sigmoid module instance by node serialization information
     */
    static Module *deserialize(fbs::FbsModel *fbs_model, std::string node_name)
    {
        Module *op = nullptr;
        quant_type_t quant_type;
        fbs_model->get_operation_attribute(node_name, "quant_type", quant_type);

        // Create module
        if (quant_type == QUANT_TYPE_SYMM_8BIT || quant_type == QUANT_TYPE_SYMM_16BIT) {
            op = new Sigmoid(node_name.c_str(), MODULE_INPLACE_CHANGED_BUFFER, quant_type);
        }
        return op;
    }

    void print() { ESP_LOGI("Sigmoid", "quant_type: %s.", quant_type_to_string(quant_type)); }
};
} // namespace module
} // namespace dl
