#include "train.h"
#include "esp_log.h"
#include "esp_random.h"
#include "har_dataset.h"
#include <string.h>
#include "freertos/FreeRTOS.h"

#include "util.h"

#define LOGI(fmt, ...) ESP_LOGI("aifes", "info: " fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) ESP_LOGE("aifes", "error: " fmt, ##__VA_ARGS__)

static void aifes_task_delay_callback(float loss) {
	vTaskDelay(1);
}

int8_t aifes_initialize_glorot_weights() {
    // We run the training function with 0 epochs. This runs initialization only.
    float dummy_input[model_structure[0]];
    float dummy_target[model_structure[1]];
    float dummy_output[model_structure[2]];

	for (int i=0; i < sizeof(dummy_input) / sizeof(float); i++) dummy_input[i] = 0;
	for (int i=0; i < sizeof(dummy_target) / sizeof(float); i++) dummy_target[i] = 0;
	for (int i=0; i < sizeof(dummy_output) / sizeof(float); i++) dummy_output[i] = 0;


    const uint16_t in_shape[2] = {1, model_structure[0]};
    const uint16_t out_shape[2] = {1, model_structure[LAYER_COUNT - 1]};

	// We cast away const to match the AIfES C API parameter types (they are read-only in execution)
    const aitensor_t input_tensor = AITENSOR_2D_F32(in_shape, (float*)dummy_input);
    const aitensor_t target_tensor = AITENSOR_2D_F32(out_shape, (float*)dummy_target);
    aitensor_t output_tensor = AITENSOR_2D_F32(out_shape, dummy_output);

    static const AIFES_E_training_parameter_fnn_f32 train_params = {
		.optimizer = AIfES_E_adam,
		.loss = AIfES_E_crossentropy,
		.learn_rate = 0,
		.sgd_momentum = 0.0f,
		.batch_size = 1,
		.epochs = 0,
		// Set to 999999 to prevent division by zero in AIfES training loop
		.epochs_loss_print_interval = 999999,
		.loss_print_function = aifes_task_delay_callback,
		.early_stopping = AIfES_E_early_stopping_off,
		.early_stopping_target_loss = 0.0f
	};

    static constexpr AIFES_E_init_weights_parameter_fnn_f32 init_params = {
		.init_weights_method = AIfES_E_init_glorot_uniform,
		.max_init_uniform = 0.0f,
		.min_init_uniform = 0.0f
	};

    const int8_t err = AIFES_E_training_fnn_f32(
        (aitensor_t *)&input_tensor, 
        (aitensor_t *)&target_tensor, 
        (AIFES_E_model_parameter_fnn_f32 *)&model, 
        (AIFES_E_training_parameter_fnn_f32 *)&train_params, 
        (AIFES_E_init_weights_parameter_fnn_f32 *)&init_params, 
        &output_tensor
    );

	return err;
}

int8_t aifes_train_epoch(
    const float* const s_inputs, 
    const float* const s_targets, 
    float* const s_outputs
) {
    if (s_inputs == NULL || s_targets == NULL || s_outputs == NULL) {
        LOGE("Invalid inputs for training");
        return -2;
    }

    const uint16_t in_shape[2] = { batch_size, model_structure[0]};
    const uint16_t out_shape[2] = { batch_size, model_structure[LAYER_COUNT-1]};

    // We cast away const to match the AIfES C API parameter types (they are read-only in execution)
    const aitensor_t input_tensor = AITENSOR_2D_F32(in_shape, (float*)s_inputs);
    const aitensor_t target_tensor = AITENSOR_2D_F32(out_shape, (float*)s_targets);
    aitensor_t output_tensor = AITENSOR_2D_F32(out_shape, s_outputs);

    static const AIFES_E_training_parameter_fnn_f32 train_params = {
		.optimizer = AIfES_E_adam,
		.loss = AIfES_E_crossentropy,
		.learn_rate = learn_rate,
		.sgd_momentum = 0.9f,
		.batch_size = batch_size,
		.epochs = 1,
		.epochs_loss_print_interval = 1, // we need this to let other processes breath
		.loss_print_function = aifes_task_delay_callback,
		.early_stopping = AIfES_E_early_stopping_off,
		.early_stopping_target_loss = 0.0f
	};

    static constexpr AIFES_E_init_weights_parameter_fnn_f32 init_params = {
		.init_weights_method = AIfES_E_init_no_init,
		.max_init_uniform = 0.0f,
		.min_init_uniform = 0.0f
	};

    const int8_t err = AIFES_E_training_fnn_f32(
        (aitensor_t *)&input_tensor, 
        (aitensor_t *)&target_tensor, 
        (AIFES_E_model_parameter_fnn_f32 *)&model, 
        (AIFES_E_training_parameter_fnn_f32 *)&train_params, 
        (AIFES_E_init_weights_parameter_fnn_f32 *)&init_params, 
        &output_tensor
    );

    return err;
}

int8_t aifes_predict( const float* const inputs, float* const outputs, const uint16_t num_samples) {
    if (inputs == NULL || outputs == NULL || num_samples == 0) {
        LOGE("Invalid inputs for prediction");
        return -2;
    }

    const uint16_t in_shape[2] = { num_samples, model_structure[0]};
    const uint16_t out_shape[2] = { num_samples, model_structure[LAYER_COUNT-1]};

    const aitensor_t input_tensor = AITENSOR_2D_F32(in_shape, (float*)inputs);
    aitensor_t output_tensor = AITENSOR_2D_F32(out_shape, outputs);

    const int8_t err = AIFES_E_inference_fnn_f32(
		(aitensor_t *) &input_tensor,
		(AIFES_E_model_parameter_fnn_f32 *)&model,
		&output_tensor
	);
    if (err != 0)
        LOGE("Inference failed, error: %d", err);

    return err;
}

void aifes_get_weights(float* const out_weights) {
    if (out_weights != NULL)
        memcpy(out_weights, weights, sizeof(float) * TOTAL_PARAMETERS);
}

void aifes_set_weights(const float* const in_weights) {
    if (in_weights == NULL) return;

    memcpy(weights, in_weights, sizeof(float) * TOTAL_PARAMETERS);
    LOGI("Model weights updated");
}

void aifes_log_parameters() {
    LOGI("================ Current Model Parameters (%u floats) ================", (unsigned int)TOTAL_PARAMETERS);
    
    // Layer 1
    LOGI("--- Layer 1: Input to Hidden (Weights [8x100] & Biases [8]) ---");
    for (uint32_t h = 0; h < model_structure[1]; h++) {
        LOGI("  Hidden Neuron %u: Weights snippet=[%.4f, %.4f, %.4f, %.4f, ...] | Bias=%.4f", 
                 (unsigned int)h,
                 weights[h * model_structure[0] + 0],
                 weights[h * model_structure[0] + 1],
                 weights[h * model_structure[0] + 2],
                 weights[h * model_structure[0] + 3],
                 weights[model_structure[0] * model_structure[1] + h]);
    }

    // Layer 2
    const uint32_t l1_total_size = (model_structure[0]+1) * model_structure[1];
    LOGI("--- Layer 2: Hidden to Output (Weights [6x8] & Biases [6]) ---");
    for (uint32_t o = 0; o < OUTPUT_NEURONS; o++) {
        LOGI("  Output Neuron %u: Weights snippet=[%.4f, %.4f, %.4f, %.4f, ...] | Bias=%.4f",
                 (unsigned int)o,
                 weights[l1_total_size + o * model_structure[1] + 0],
                 weights[l1_total_size + o * model_structure[1] + 1],
                 weights[l1_total_size + o * model_structure[1] + 2],
                 weights[l1_total_size + o * model_structure[1] + 3],
                 weights[l1_total_size + model_structure[1] * model_structure[2] + o]);
    }
             
    LOGI("=======================================================================");
}

void aifes_log_measure(uint8_t epoch, uint16_t iter) {
		int correct = 0;
        float total_loss = 0.0f;
        
        for (int sample_idx = 0; sample_idx < HAR_NUM_SAMPLES; sample_idx++) {
            const float* features = har_dataset[sample_idx].features;
            const uint8_t true_label = har_dataset[sample_idx].label;
            
            // Predict
            float prediction[OUTPUT_NEURONS];
            aifes_predict(features, prediction, 1);
            
            // Find predicted class
            int predicted_label = 0;
            for (int c = 1; c < OUTPUT_NEURONS; c++) {
                if (prediction[c] > prediction[predicted_label]) {
                    predicted_label = c;
                }
            }
            
            // Count correct
            if (predicted_label == true_label) {
                correct++;
            }
            
            // Calculate cross-entropy loss manually
            // Loss = -ln(prediction[true_label])
            // Clamp to avoid log(0)
            float prob = prediction[true_label];
            if (prob < 1e-7f) prob = 1e-7f;
            total_loss += -logf(prob);
        }
        
        const float accuracy = (float)correct / HAR_NUM_SAMPLES * 100.0f;
        const float avg_loss = total_loss / HAR_NUM_SAMPLES;
        
        LOGI("Epoch %2d | Iter %3d | Loss: %.4f | Acc: %5.1f%% (%3d/%d)", epoch, iter, avg_loss, accuracy, correct, HAR_NUM_SAMPLES);
}

void aifes_log_inference_test() {
    LOGI("=== INFERENCE TEST ===");
    for (int i = 0; i < 25; i++) {
        float final_output[OUTPUT_NEURONS];
        const uint8_t true_label = har_dataset[i].label;
        aifes_predict(har_dataset[i].features, final_output, 1);
        
        int pred = 0;
        for (int c = 1; c < OUTPUT_NEURONS; c++) {
            if (final_output[c] > final_output[pred]) pred = c;
        }
        
        LOGI("Sample %d: True=%d, Pred=%d, Output=[%.3f, %.3f, %.3f, %.3f, %.3f, %.3f] %s",
             i, true_label, pred,
             final_output[0], final_output[1], final_output[2],
             final_output[3], final_output[4], final_output[5],
             (pred == true_label) ? "OK" : "FALSE");
    }
}

void aifes_train_demo(void) {
	LOGI("Initializing HAR Training...");
	
	// init weights 
	aifes_initialize_glorot_weights();

	// 3. Log initial weights before training
	LOGI("Initial weights generated:");
	aifes_log_parameters();

	// 4. Run local training epochs (simulating local training before sending back to Flower)

	LOGI("Starting HAR training: %d epochs, learning rate = %.3f, batch size = %d...", 
		epochs, learn_rate, batch_size);

	for (int epoch = 0; epoch <= epochs; ++epoch) {
		// Loop over ALL samples
		for (int i = 0; i < 100; i++) {

			static float samples[batch_size][HAR_NUM_FEATURES];
			static float target[batch_size][OUTPUT_NEURONS];
			static float output[batch_size][OUTPUT_NEURONS];

			for (uint8_t i=0; i < batch_size; i++) {
				const uint16_t sample_idx = esp_random() % (HAR_NUM_SAMPLES - 1);
				memcpy(samples[i], &(har_dataset[sample_idx]), sizeof(har_dataset[sample_idx].features));
				const uint8_t label = har_dataset[sample_idx].label;
				for (int j=0; j < OUTPUT_NEURONS; j++) target[i][j] = label == j;
			}
			
			// const uint16_t sample_idx = esp_random() % (HAR_NUM_SAMPLES - 17);
			// memcpy(samples[0], &(har_dataset[sample_idx]), sizeof(har_dataset[sample_idx].features) * 16);
			// for (int i=0; i < sample_size; i++) {
			// 	const uint8_t label = har_dataset[sample_idx + i].label;
			// 	for (int j=0; j < OUTPUT_NEURONS; j++) target[i][j] = label == j;
			// }


			// const float* features = har_dataset[sample_idx].features;
			// const uint8_t label = har_dataset[sample_idx].label;

			// float target[OUTPUT_NEURONS] = {0.0f};
			// target[label] = 1.0f;
			// float output[OUTPUT_NEURONS];

			// const int8_t err = aifes_train_epoch(features, target, output, 1);
			const int8_t err = aifes_train_epoch((float *)samples, (float *)target, (float *)output);
			if (err != 0) {
                LOGE("Training failed at epoch %d, iteration %d, error=%d", epoch, i, err);
                goto cleanup;
            }

			if (i % 20 == 0) vTaskDelay(1);
			aifes_log_measure(epoch, i);
		}
		LOGI("-------------------");
		aifes_log_measure(epoch, 999);
		LOGI("-------------------");

		if (epoch % 10 == 0)
			aifes_log_inference_test();
	}

	// Log parameters after training
	LOGI("Weights after training (snippet):");
	aifes_log_parameters();
	

	// 5. Export weights to flat array (simulating Sergey serialization for Flower)
	// float* const s_exported_weights = (float *)static_alloc(TOTAL_PARAMETERS * sizeof(float));
	// aifes_get_weights(client, s_exported_weights);
	// LOGI("Successfully exported %u parameters to flat array (size: %lu bytes).", 
	// 		(unsigned int)TOTAL_PARAMETERS, TOTAL_PARAMETERS * sizeof(float));
	

cleanup:
    // Final test
	aifes_log_inference_test();

	// const float* const test_input = har_dataset[99].features;
	// float* const test_output = (float *) static_alloc(OUTPUT_NEURONS * sizeof(float));

	// // Generate a WALKING sample
	// // generate_mock_har_data(test_input, NULL, 1);  // Modify to take class parameter
	// 

	// aifes_predict(client, test_input, test_output, 1);

	// LOGI("Prediction for WALKING class:");
	// for(int i = 0; i < OUTPUT_NEURONS; i++) {
	// 	LOGI("  Class %d: %.4f", i, test_output[i]);
	// }

	// static_free(OUTPUT_NEURONS * sizeof(float));
	// // static_free(INPUT_NEURONS);
	// // static_free(TOTAL_PARAMETERS * sizeof(float));
	// static_free(sizeof(aifes_t));

	// 6. Simulating receiving a global model update from Flower server
	// LOGI("Simulating global model update from Flower server...");
	// // Modify one weight value just to show the update was applied
	// s_exported_weights[0] += 0.5f; 
	// aifes_set_weights(&client, s_exported_weights);

	// // Verify change is loaded
	// static float s_re_exported_weights[TOTAL_PARAMETERS] = {0.0f};
	// aifes_get_weights(&client, s_re_exported_weights);
	// LOGI("Updated weight [0] verify value: %.4f (expected: %.4f)", 
	// 		s_re_exported_weights[0], s_exported_weights[0]);

	// // 7. Run inference on a test sample
	// static float s_test_input[INPUT_NEURONS];
	// // Fill test_input with mock Sitting pattern (class 3)
	// for (uint32_t f = 0; f < INPUT_NEURONS; f++) {
	// 	s_test_input[f] = (0.05f + 1.0f) / 2.0f;
	// }
	// float test_output[OUTPUT_NEURONS] = {0.0f};

	// const int8_t err = aifes_predict(&client, s_test_input, test_output, 1);
	// if (err != 0) {
	// 	LOGE("Inference prediction failed, error code: %d", err);
	// 	return;
	// }

	// LOGI("Inference Test Sample (Sitting Pattern) -> Predicted Probabilities:");
	// LOGI("  Walking:            %.4f", test_output[0]);
	// LOGI("  Walking_Upstairs:   %.4f", test_output[1]);
	// LOGI("  Walking_Downstairs: %.4f", test_output[2]);
	// LOGI("  Sitting:            %.4f", test_output[3]);
	// LOGI("  Standing:           %.4f", test_output[4]);
	// LOGI("  Laying:             %.4f", test_output[5]);
}

void aifes_train_round(const float* const in_weights, float* const out_weights) {
	aifes_set_weights(in_weights);

	// Log initial weights before training
	LOGI("Initial weights: ");
	aifes_log_parameters();

	LOGI("Starting training: %d epochs, learning rate = %.3f, batch size = %d...", 
		epochs, learn_rate, batch_size);

	// for (int epoch = 1; epoch <= epochs; ++epoch) {
	for (int epoch = 1; epoch <= 1; ++epoch) {
		// Loop over ALL samples
		for (int i = 0; i < 100; i++) {
			static float samples[batch_size][HAR_NUM_FEATURES];
			static float target[batch_size][OUTPUT_NEURONS];
			static float output[batch_size][OUTPUT_NEURONS];

			for (uint8_t i=0; i < batch_size; i++) {
				const uint16_t sample_idx = esp_random() % (HAR_NUM_SAMPLES - 1);
				memcpy(samples[i], &(har_dataset[sample_idx]), sizeof(har_dataset[sample_idx].features));
				const uint8_t label = har_dataset[sample_idx].label;
				for (int j=0; j < OUTPUT_NEURONS; j++) target[i][j] = label == j;
			}

			// const uint16_t sample_idx = esp_random() % (HAR_NUM_SAMPLES - 1);
			// const float* features = har_dataset[sample_idx].features;
			// uint8_t label = har_dataset[sample_idx].label;

			// float target[OUTPUT_NEURONS] = {0.0f};
			// target[label] = 1.0f;
			// float output[OUTPUT_NEURONS];

			const int8_t err = aifes_train_epoch((float*)samples, (float*)target, (float*)output);
			if (err != 0) {
                LOGE("Training failed at epoch %d, sample %d, error=%d", epoch, i, err);
                goto cleanup;
            }

			if (i % 20 == 0) vTaskDelay(1);
			aifes_log_measure(epoch, i);
		}
		LOGI("-------------------");
		aifes_log_measure(epoch, 999);
		LOGI("-------------------");
	}

	// Log parameters after training
	LOGI("Weights after training (snippet):");
	aifes_log_parameters();

	// Export weights to flat array
	aifes_get_weights(out_weights);
	LOGI("Successfully exported %u parameters to flat array (size: %lu bytes).", 
			(unsigned int)TOTAL_PARAMETERS, TOTAL_PARAMETERS * sizeof(float));

cleanup:
    // Final test
	aifes_log_inference_test();
}

#undef LOGI
#undef LOGE
