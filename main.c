#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"


#define INPUT_PINS 1
#define OUTPUT_PINS 14
#define PIN_COUNT 14
#define SERVER_URL "http://10.126.201.84:5000/data"
#define WIFI_SSID "Galaxy A51BA7B"
#define WIFI_PASS  "xhnb3353"
#define WIFI_TIMEOUT_MS 1000
#define WIFI_MAX_RETRY 5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int retry_count = 0;



static const char *TAG3 = "NVS_SERIAL";
static const char *TAG = "SERVER_SEND";
static const char *TAG2 = "wifi-connect";




// tek input var tesisat kablosunun uçları birleştirilip bu inputa bağlanacak.
// kalan uçlar output pinlerine
//toplam 14 output var
gpio_num_t gpioinputlist[INPUT_PINS] = {
    GPIO_NUM_5
};

gpio_num_t gpiooutputlist[OUTPUT_PINS] = {
    GPIO_NUM_4,GPIO_NUM_12,GPIO_NUM_13,GPIO_NUM_14,GPIO_NUM_16,GPIO_NUM_17,GPIO_NUM_18,GPIO_NUM_19,GPIO_NUM_21,GPIO_NUM_22,GPIO_NUM_23,GPIO_NUM_26,GPIO_NUM_27,GPIO_NUM_32
};


void setup_esp_pins_input()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,   
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < PIN_COUNT; i++) {
        io_conf.pin_bit_mask |= (1ULL << gpioinputlist[i]);
    }

    gpio_config(&io_conf);
}

void setup_esp_pins_output()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 0,   
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (int i = 0; i < PIN_COUNT; i++) {
        io_conf.pin_bit_mask |= (1ULL << gpiooutputlist[i]);
    }

    gpio_config(&io_conf);
}


//bu listeye 14 kalonun doğruluk değeri yazılıyor.
int kablodogruluklistesi[PIN_COUNT] = {0};

//14 kablonun bağlantı testi burda yapılıyor.
void testconnections(){
    int allTrue = 1;
    for (int i = 0; i < PIN_COUNT; i++)
    {
        int toplambirgelmesayisi = 0;
        int outlevel = gpio_get_level(gpiooutputlist[i]);


        //bu for döngüsünde 30'u değiştirerek kabloların denenme süresini değiştirebilirsin.
        for (int numoftry = 0;numoftry<30;numoftry++)
        {
            if (outlevel == 1)
                {
                    toplambirgelmesayisi++;
                }
            vTaskDelay(pdMS_TO_TICKS(100));
        }


        //bu if 30 denenmeden 26 tanesi başarılı olursa kablo testi geçti yazdırıyor.
        if (toplambirgelmesayisi > 25)
        {
            kablodogruluklistesi[i] = 1;
            printf("kablo %d çalışıyor.\n",(i+1));
            
        }
        else{
            kablodogruluklistesi[i] = 0;
            printf("Kablo %d çalışmıyor.\n",(i+1));
            
        }
    }

    for(int kablo_listesi_test_değişkeni=0;kablo_listesi_test_değişkeni<14;kablo_listesi_test_değişkeni++){

        if (!kablodogruluklistesi[kablo_listesi_test_değişkeni])
        {
            allTrue = 0;
            break;
        }
    }
            
    if (allTrue)
    {
        printf("kablo testi ONAYLANDI\n");
    }
    else
    {
        printf("Kablo testi GEÇEMEDİ\n");
    }

}


//listeyi JSON'a çeviren fonksiyon
char* list_to_json(void) {
    cJSON *root = cJSON_CreateObject();     
    cJSON *array = cJSON_CreateArray();     
    for (int i = 0; i < PIN_COUNT; i++) {
        cJSON_AddItemToArray(array, cJSON_CreateNumber(kablodogruluklistesi[i]));
    }
    cJSON_AddItemToObject(root, "data", array);
    // JSON stringe çevir 
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return json_str; 
}


// servera yollama fonksiyonu
esp_err_t send_to_server(const char *json_str) {
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_post_field(client, json_str, strlen(json_str));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "JSON sent successfully");
    } else {
        ESP_LOGE(TAG, "Error sending JSON: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


//alttaki iki fonksiyon wifi bağlama fonksiyonu.
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi bağlantısı koptu. Tekrar bağlanılıyor...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP alındı: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi başlatıldı, bağlanıyor...");
}


//nvs başlatma fonksiyonu.
void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

//seri numara saklama fonksiyonu(temelli).
void save_serial_number_multi(const char *serial, int index) {
    nvs_handle_t handle;
    char key[16];
    snprintf(key, sizeof(key), "serial%d", index); // serial1, serial2...

    if (nvs_open("storage", NVS_READWRITE, &handle) != ESP_OK) return;

    if (nvs_set_str(handle, key, serial) == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI("NVS_MULTI", "Saved %s at key %s", serial, key);
    }

    nvs_close(handle);
}


//seri no okuma.
void read_all_serial_numbers() {
    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find("nvs", "storage", NVS_TYPE_STR, &it);

    while (res == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        char value[64];
        size_t length = sizeof(value);

        nvs_handle_t handle;
        if (nvs_open("storage", NVS_READONLY, &handle) == ESP_OK) {
            if (nvs_get_str(handle, info.key, value, &length) == ESP_OK) {
                ESP_LOGI("NVS_MULTI", "Key: %s, Value: %s", info.key, value);
            }
            nvs_close(handle);
        }

        res = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}




void app_main()
{
    setup_esp_pins_input();
    setup_esp_pins_output();
    wifi_init_sta();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           pdMS_TO_TICKS(10000));
    if(bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi bağlantısı başarılı!");
    }
    else{
        ESP_LOGI(TAG, "Wi-Fi bağlantısı başarısız veya zaman aşımı!");

    }

    testconnections();

    for (int i = 0; i < 14; i++)
    {
        printf("%d,",kablodogruluklistesi[i]);
    }

    char *json_output = list_to_json();
    printf("JSON: %s\n", json_output);

    init_nvs();
    save_serial_number_multi("SN-001", 1);
    save_serial_number_multi("SN-002", 2);
    save_serial_number_multi("SN-003", 3);


    read_all_serial_numbers();

    if (send_to_server(json_output) == ESP_OK) {
        ESP_LOGI(TAG, "Veri başarıyla gönderildi!");
    } else {
        ESP_LOGE(TAG, "Veri gönderilemedi!");
    }

    // İşin bitince serbest bırak
    free(json_output);
    vTaskDelay(pdMS_TO_TICKS(10000));


}

//25 ve 26, 32, 33 gpio çalışmıyor yada output olarak ayarlanmıyor.



