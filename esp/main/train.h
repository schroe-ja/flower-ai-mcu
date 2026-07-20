#ifndef AIFES_CLIENT_H
#define AIFES_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include "aifes.h"

enum HarClass { WALKING, UPSTAIRS, DOWNSTAIRS, SITTING, STANDING, LAYING, OUTPUT_NEURONS };

#define LAYER_0 100
#define LAYER_1 16
#define LAYER_2 OUTPUT_NEURONS
#define TOTAL_PARAMETERS ((LAYER_0 + 1) * LAYER_1 + (LAYER_1 + 1) * LAYER_2)

// These have to be passed to AIfES via pointers so they cant be defines
static const uint32_t model_structure[] = {LAYER_0, LAYER_1, LAYER_2};
static const AIFES_E_activations model_activations[] = {AIfES_E_relu, AIfES_E_softmax};

static_assert(sizeof(model_structure) / sizeof(model_structure[0]) - 1 == sizeof(model_activations) / sizeof(model_activations[0]));

static constexpr uint8_t LAYER_COUNT = sizeof(model_structure) / sizeof(model_structure[0]);

static float weights[TOTAL_PARAMETERS];
static const AIFES_E_model_parameter_fnn_f32 model = {
	.layer_count = LAYER_COUNT,
	.fnn_structure = (uint32_t *) model_structure,
	.fnn_activations = (AIFES_E_activations *)model_activations,
	.flat_weights = weights
};

static constexpr float learn_rate = .0005f;
static constexpr uint32_t epochs = 1000;
static constexpr uint32_t batch_size = 16;

// Generate random initial weights using Glorot/Xavier uniform
int8_t aifes_initialize_glorot_weights();

// Train the local model for one epoch
int8_t aifes_train_epoch(
	const float* inputs, 
	const float* targets, 
	float* outputs
);

// Predict activity classes for a batch of input samples
int8_t aifes_predict(
	const float* const inputs, 
	float* const outputs, 
	const uint16_t num_samples
);

// Export weights to external flat float buffer
void aifes_get_weights(float* const out_weights);

// Import weights from external flat float buffer
void aifes_set_weights(const float* const in_weights);

// Debug logging to show sample weights/biases
void aifes_log_parameters();

void aifes_train_demo();

void aifes_train_round(const float* const in_weights, float* const out_weights);

#endif // AIFES_CLIENT_H
