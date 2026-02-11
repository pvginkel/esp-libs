#include "support.h"

#include "CoreDumpUploader.h"

#include "esp_app_desc.h"
#include "esp_core_dump.h"
#include "esp_http_client.h"
#include "esp_partition.h"

// Copied over private struct so that we can parse the core dump partition data.

/**
 * @brief Core dump data header
 * This header predecesses the actual core dump data (ELF or binary). */
typedef struct _core_dump_header_t {
    uint32_t data_len;     /*!< Data length */
    uint32_t version;      /*!< Core dump version */
    uint32_t tasks_num;    /*!< Number of tasks */
    uint32_t tcb_sz;       /*!< Size of a TCB, in bytes */
    uint32_t mem_segs_num; /*!< Number of memory segments */
    uint32_t chip_rev;     /*!< Chip revision */
} core_dump_header_t;

#define BUFFER_SIZE 1024

LOG_TAG(CoreDumpUploader);

esp_err_t CoreDumpUploader::upload() {
    const auto err = esp_core_dump_image_check();
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGD(TAG, "No core dump partition found");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No core dump found to upload, error %s", esp_err_to_name(err));
        return ESP_OK;
    }

    size_t out_addr;
    size_t out_size;
    ESP_ERROR_RETURN(esp_core_dump_image_get(&out_addr, &out_size));

    // Always erase the core dump after attempting upload.
    DEFER(esp_core_dump_image_erase());

    // Find the core dump partition to read data from.
    const auto* partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    ESP_RETURN_ON_FALSE(partition, ESP_ERR_NOT_FOUND, TAG, "Core dump partition not found");

    const size_t partition_offset = out_addr - partition->address;

    // Read the core dump header to determine the ELF size.
    // Layout: [header 24B] [ELF data] [padding] [checksum 4B CRC32 / 32B SHA256]
    core_dump_header_t header;
    ESP_ERROR_RETURN(esp_partition_read(partition, partition_offset, &header, sizeof(header)));

    const size_t checksum_size = (header.version & 0xFF) == 3 ? 32 : 4;
    const size_t elf_offset = partition_offset + sizeof(header);
    const size_t elf_size = out_size - sizeof(header) - checksum_size;

    ESP_LOGI(TAG, "Uploading core dump ELF (%d bytes)", (int)elf_size);

    // Get firmware version for query parameters.
    const auto* app_desc = esp_app_get_description();

    auto url = strformat("%s?chip=%s&firmware_version=%s", _url, CONFIG_IDF_TARGET, app_desc->version);

    // Set up HTTP client.
    esp_http_client_config_t config = {
        .url = url.c_str(),
        .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
        .buffer_size_tx = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    auto client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
    DEFER(esp_http_client_cleanup(client));

    const auto& authorization = _application->get_authorization();

    ESP_ERROR_RETURN(esp_http_client_set_method(client, HTTP_METHOD_POST));
    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Authorization", authorization.c_str()));
    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Content-Type", "application/octet-stream"));
    ESP_ERROR_RETURN(esp_http_client_open(client, (int)elf_size));

    // Read ELF from flash (skipping header) and stream to HTTP.
    auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
    size_t written = 0;

    while (written < elf_size) {
        const auto chunk_size = std::min((size_t)BUFFER_SIZE, elf_size - written);
        ESP_ERROR_RETURN(esp_partition_read(partition, elf_offset + written, buffer.get(), chunk_size));

        ESP_ASSERT_RETURN(esp_http_client_write(client, buffer.get(), chunk_size) >= 0, ESP_FAIL);

        written += chunk_size;
    }

    auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        ESP_ERROR_RETURN(esp_err_t(-length));
    }

    const auto status_code = esp_http_client_get_status_code(client);
    if (status_code >= 200 && status_code < 300) {
        ESP_LOGI(TAG, "Core dump uploaded successfully (status %d)", status_code);
    } else {
        ESP_LOGW(TAG, "Core dump upload failed with status %d", status_code);
    }

    return ESP_OK;
}
