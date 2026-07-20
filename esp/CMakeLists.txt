# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS
	main
	heap
	esp_wifi
	esp_netif
	nvs_flash
	lwip
	mdns
)

project(flower_client)
