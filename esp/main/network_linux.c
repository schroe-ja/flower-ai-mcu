#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Standard POSIX socket headers for Linux
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Mocked ESP-IDF headers
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Project headers
#include "train.h"
#include "util.h"
#include "wifi_credentials.h"

#define LOGI(fmt, ...) ESP_LOGI("network", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) ESP_LOGE("network", fmt, ##__VA_ARGS__)

// Wi-Fi checks removed as they are not needed on Linux
#ifndef SERVER_IP
#    error "SERVER_IP must be defined at compile time!"
#endif
#ifndef SERVER_PORT
#    error "SERVER_PORT must be defined at compile time!"
#endif

int sock;

bool read_all(int fd, const Slice *msg) {
	LOGI("Trying to read %zu bytes...", msg->len);
	uint8_t *const ptr = msg->ptr;
	ssize_t total_read = 0;

	while (total_read < msg->len) {
		const ssize_t bytes_read = read(fd, ptr + total_read, msg->len - total_read);
		if (bytes_read <= 0) {
			LOGE("    aborted, error: %zd (%s)!", bytes_read, strerror(errno));
			return false;
		}
		total_read += bytes_read;
		LOGI("    %zd/%zu bytes", total_read, msg->len);
	}

	LOGI("    done!");
	return true;
}

// Returns true on success.
bool send_all(int fd, const Slice *msg) {
	LOGI("Trying to send %zu bytes...", msg->len);
	uint8_t *const ptr = msg->ptr;
	ssize_t total_sent = 0;

	while (total_sent < msg->len) {
		const ssize_t bytes_sent = send(fd, ptr + total_sent, msg->len - total_sent, 0);
		if (bytes_sent <= 0) {
			LOGE("    aborted!");
			return false;
		}
		total_sent += bytes_sent;
		LOGI("    %zd/%zu bytes", total_sent, msg->len);
	}

	LOGI("    done!");
	return true;
}


// Wi-Fi initialization is a no-op on Linux since the host OS manages the network
void init_wifi(void) {
	LOGI("Skipping Wi-Fi init on linux.");
}

void network_task(void *pv_params) {
	LOGI("Starting network_task (Linux OS Routing)...");
	LOGI("Attempting to connect to server at %s:%d...", SERVER_IP, SERVER_PORT);

	const struct sockaddr_in dest_addr = {
		.sin_addr.s_addr = inet_addr(SERVER_IP),
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT)
	};

	// Outer loop responsible for setting up connection
	while (true) {
		LOGI("Setting up connection...");

		// Initialize standard POSIX socket
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (sock < 0) {
			LOGE("Unable to create socket: errno %d", errno);
			goto end_sock;
		}
		LOGI("Created socket!");

		// Connect to server
		const int err = connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
		if (err != 0) {
			LOGE("Socket unable to connect! (%s)", strerror(errno));
			errno = 0;
			goto end;
		}

		LOGI("Successfully connected! Reading payload...");

		// Inner loop responsible for actual communication loop
		while (true) {
			LOGI("loop run");
			Slice weights = {0};

			weights.len = sizeof(float) * TOTAL_PARAMETERS;
			LOGI("Total size in bytes is %zu (%zu * %zu)", weights.len, sizeof(float), TOTAL_PARAMETERS);

			weights.ptr = static_alloc(weights.len);
			if (!weights.ptr) {
				LOGE("Allocation failed! Used %zu/%dB, tried to allocate %zu more!",
						static_ram_used, STATIC_RAM_SIZE, weights.len);
				goto end;
			}
			if (!read_all(sock, &weights)) {
				LOGE("Failed to read payload!");
				static_free(weights.len);
				goto end;
			}

			// process_msg(&incom);
			aifes_train_round((float *)weights.ptr, (float *)weights.ptr);

			if (!send_all(sock, &weights)) {
				LOGE("Failed to send payload!");
				static_free(weights.len);
				goto end;
			}

			static_free(weights.len);
			LOGI("Waiting for closing...");
			vTaskDelay(pdMS_TO_TICKS(3000));
			LOGI("    done!");
			goto end;
		}

end:
		close(sock);
end_sock:
		vTaskDelay(pdMS_TO_TICKS(3000));
	}

	__builtin_unreachable();
}

#undef LOGI
#undef LOGE
