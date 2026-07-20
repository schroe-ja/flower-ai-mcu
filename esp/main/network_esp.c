#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "train.h"
#include "esp_log.h"

#include "util.h"
#include "wifi_credentials.h"

#define LOGI(fmt, ...) ESP_LOGI("network", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) ESP_LOGE("network", fmt, ##__VA_ARGS__)

#ifndef WIFI_SSID
#	error "WIFI_SSID must be defined at compile time!"
#endif
#ifndef WIFI_PASS
#	error "WIFI_PASS must be defined at compile time!"
#endif
#ifndef SERVER_IP
#	error "SERVER_IP must be defined at compile time!"
#endif
#ifndef SERVER_PORT
#	error "SERVER_PORT must be defined at compile time!"
#endif

int sock;

// Returns true on success.
bool read_all(int fd, const Slice *msg) {
	LOGI("Trying to read %zu bytes...\n", msg->len);
	uint8_t *const ptr = msg->ptr;
	ssize_t total_read = 0;

	while (total_read < msg->len) {
		const ssize_t bytes_read = read(fd, ptr + total_read, msg->len - total_read);
		if (bytes_read <= 0) {
			LOGE("    aborted!\n");
			return false;
		}
		total_read += bytes_read;
		LOGI("    %zd/%zu bytes\n", total_read, msg->len);
	}

	LOGI("    done!\n");
	return true;
}

// Returns true on success.
bool send_all(int fd, const Slice *msg) {
	LOGI("Trying to send %zu bytes...\n", msg->len);
	uint8_t *const ptr = msg->ptr;
	ssize_t total_sent = 0;

	while (total_sent < msg->len) {
		const ssize_t bytes_sent = send(fd, ptr + total_sent, msg->len - total_sent, 0);
		if (bytes_sent <= 0) {
			LOGE("    aborted!\n");
			return false;
		}
		total_sent += bytes_sent;
		LOGI("    %zd/%zu bytes\n", total_sent, msg->len);
	}

	LOGI("    done!\n");
	return true;
}

static void wifi_event_handler(
	void *arg,
	esp_event_base_t ev_base,
	int32_t ev_id,
	void *ev_data)
{
	if (ev_base == WIFI_EVENT && ev_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	}
	else if (ev_base == WIFI_EVENT && ev_id == WIFI_EVENT_STA_DISCONNECTED) {
		LOGE("Disconnected from Wi-Fi. Retrying...");
		esp_wifi_connect();
	}
	else if (ev_base == IP_EVENT && ev_id == IP_EVENT_STA_GOT_IP) {
		esp_netif_t *const netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
		if (!netif) LOGE("esp_netif_get_handle_from_ifkey returned NULL!\n");
		esp_netif_ip_info_t ip_info;
		esp_netif_get_ip_info(netif, &ip_info);

		wifi_ap_record_t ap_info;
		if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
			LOGE("nvm...\n");
			return;
		}

		LOGI("Successfully connected to %s per Wi-Fi and got IP " IPSTR "!\n",
			ap_info.ssid,
			IP2STR(&ip_info.ip));
	}
}

void init_wifi(void) {
	LOGI("Connecting to WiFi...\n");

	// Initialize NVS (required for Wi-Fi storage)
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_create_default_wifi_sta();

#ifdef USE_MDNS // TODO TEST
	const int mdns_err = mdns_init();
	if (mdns_err != ESP_OK) exit(1);
	else printf("TODO yippie\n\n\n\n");

	struct esp_ip4_addr addr = {0};
	printf("Searching for the Flower host...\n");

	//mdns_hostname_set("esp32");

	const int query_err = mdns_query_a(HOSTNAME, 5000, &addr);
	if (query_err == ESP_ERR_NOT_FOUND) {
		LOGE("Host " HOSTNAME ".local not found on the network!\n");
	}
	else if (query_err != ESP_OK) {
		LOGE("Faced with another kind of mdns_query_a error!\n");
	}
	else printf("TODO TEST: Got new IP at " IPSTR "\n", IP2STR(&addr));
#endif

	// TODO CONSIDER
#ifdef USE_STATIC_IP
	// Setup static IP address
	esp_netif_t *const netif = esp_netif_create_default_wifi_sta();
	esp_netif_dhcpc_stop(netif);
	esp_netif_ip_info_t ip_info;
	IP4_ADDR(&ip_info.ip, 192, 168, 1, 150);
	IP4_ADDR(&ip_info.gw, 192, 168, 1, 1);
	IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
	esp_netif_set_ip_info(netif, &ip_info);
#endif

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASS,
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	LOGI("WiFi capabilities unlocked!\n");
}

void network_task(void *pv_params) {
	LOGI("Starting network_task...\n");
	esp_netif_t *const netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
	esp_netif_ip_info_t ip_info;

	while (true) {
		esp_netif_get_ip_info(netif, &ip_info);
		if (ip_info.ip.addr) break;
		LOGI("Waiting for IP...\n");
		vTaskDelay(pdMS_TO_TICKS(3000));
	}
	LOGI("Got IP: " IPSTR "\n", IP2STR(&ip_info.ip));
	LOGI("Attempting to connect to server at: %s:%d...\n", SERVER_IP, SERVER_PORT);

	const struct sockaddr_in dest_addr = {
		.sin_addr.s_addr = inet_addr(SERVER_IP),
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT)
	};

	// Outer loop responsible for setting up connection
	while (true) {
		LOGI("Setting up connection...\n");

		// Initialize socket
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
		if (sock < 0) {
			LOGE("Unable to create socket");
			goto end_sock;
		}
		LOGI("Created socket!\n");

		// Connect to server
		const int err = connect(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
		if (err != 0) {
			LOGE("Socket unable to connect! (%s)", strerror(errno));
			errno = 0;
			goto end;
		}

		LOGI("Successfully connected! Reading payload...\n");

		// Inner loop responsible for actual communication loop
		while (true) {
			LOGI("loop run\n");
			Slice weights = {0};

			weights.len = sizeof(float) * TOTAL_PARAMETERS;
			LOGI("Total size in bytes is %zu (%zu * %zu)\n", weights.len, sizeof(float), TOTAL_PARAMETERS);
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

			// ^ NOTE Ignore the LSP message lol
			// TODO + NOTE old protocol approach send(sock, &len_net, sizeof(uint32_t), 0);
			if (!send_all(sock, &weights)) {
				LOGE("Failed to send payload!");
				static_free(weights.len);
				goto end;
			}
			vTaskDelay(pdMS_TO_TICKS(3000));

			static_free(weights.len);
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
