set(SOURCES 
		"util.c"
		"train.c"
        "main.c"
        "proto/msg/transport.pb.c"
        "proto/nanopb/pb_encode.c"
        "proto/nanopb/pb_decode.c"
        "proto/nanopb/pb_common.c"

		# Core
        "aifes/aifes_config.cpp"
        
        # Basic - Base
        "aifes/basic/base/aialgo/aialgo_sequential_inference.c"
        "aifes/basic/base/aialgo/aialgo_sequential_training.c"
        "aifes/basic/base/ailayer/ailayer_dense.c"
        "aifes/basic/base/ailayer/ailayer_elu.c"
        "aifes/basic/base/ailayer/ailayer_input.c"
        "aifes/basic/base/ailayer/ailayer_leaky_relu.c"
        "aifes/basic/base/ailayer/ailayer_relu.c"
        "aifes/basic/base/ailayer/ailayer_sigmoid.c"
        "aifes/basic/base/ailayer/ailayer_softmax.c"
        "aifes/basic/base/ailayer/ailayer_softsign.c"
        "aifes/basic/base/ailayer/ailayer_tanh.c"
        "aifes/basic/base/ailayer/ailayer_template.c"
        "aifes/basic/base/ailoss/ailoss_crossentropy.c"
        "aifes/basic/base/ailoss/ailoss_mse.c"
        "aifes/basic/base/aimath/aimath_basic.c"
        "aifes/basic/base/aimath/aimath_f32.c"
        "aifes/basic/base/aimath/aimath_q31.c"
        "aifes/basic/base/aimath/aimath_q7.c"
        "aifes/basic/base/aimath/aimath_u8.c"
        "aifes/basic/base/aiopti/aiopti_adam.c"
        "aifes/basic/base/aiopti/aiopti_sgd.c"
        
        # Basic - Default implementations
        "aifes/basic/default/ailayer/ailayer_dense_default.c"
        "aifes/basic/default/ailayer/ailayer_elu_default.c"
        "aifes/basic/default/ailayer/ailayer_input_default.c"
        "aifes/basic/default/ailayer/ailayer_leaky_relu_default.c"
        "aifes/basic/default/ailayer/ailayer_relu_default.c"
        "aifes/basic/default/ailayer/ailayer_sigmoid_default.c"
        "aifes/basic/default/ailayer/ailayer_softmax_default.c"
        "aifes/basic/default/ailayer/ailayer_softsign_default.c"
        "aifes/basic/default/ailayer/ailayer_tanh_default.c"
        "aifes/basic/default/ailoss/ailoss_crossentropy_default.c"
        "aifes/basic/default/ailoss/ailoss_mse_default.c"
        "aifes/basic/default/aimath/aimath_f32_default.c"
        "aifes/basic/default/aimath/aimath_q31_default.c"
        "aifes/basic/default/aimath/aimath_q7_default.c"
        "aifes/basic/default/aiopti/aiopti_adam_default.c"
        "aifes/basic/default/aiopti/aiopti_sgd_default.c"
        
        # Express
        "aifes/basic/express/aifes_express_f32_fnn.c"
        "aifes/basic/express/aifes_express_q7_fnn.c"
        
        # CNN - Base
        "aifes/cnn/base/ailayer/ailayer_batch_normalization.c"
        "aifes/cnn/base/ailayer/ailayer_conv2d.c"
        "aifes/cnn/base/ailayer/ailayer_maxpool2d.c"
        "aifes/cnn/base/ailayer/ailayer_reshape.c"
        
        # CNN - Default
        "aifes/cnn/default/ailayer/ailayer_batch_normalization_default.c"
        "aifes/cnn/default/ailayer/ailayer_conv2d_default.c"
        "aifes/cnn/default/ailayer/ailayer_maxpool2d_default.c"
        "aifes/cnn/default/ailayer/ailayer_reshape_default.c"
        "aifes/cnn/default/aimath/aimath_cnn_f32_default.c"
)

if(IDF_TARGET STREQUAL "linux")
    message(STATUS "Building for Linux")
    list(APPEND SOURCES "network_linux.c")
else()
    message(STATUS "Building for ESP32")
    list(APPEND SOURCES "network_esp.c")
endif()


idf_component_register(
	SRCS ${SOURCES}

    INCLUDE_DIRS 
        "."
        "proto/nanopb"
        "aifes/"
        "aifes/basic/base"
        "aifes/basic/default"
        "aifes/basic/express"
        "aifes/cnn/base"
        "aifes/cnn/default"
        "aifes/core"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error)
set(COMPONENTS main)
