// ------------------------------------------------------------------------ Include Library
#include "iot_config.h"
#include "FreeRTOS.h"
#include "aws_demo.h"
#include "transport_secure_sockets.h"
#include "task.h"
#include "iot_system_init.h"
#include "iot_logging_task.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_interface.h"
#include "driver/uart.h"
#include "esp_netif.h"
#include "iot_network_manager_private.h"
#include "iot_uart.h"

#include "driver/adc.h"
#include "esp_camera.h"

#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "core_http_client.h"
#include "iot_default_root_certificates.h"
#include "cJSON.h"

#include "iot_wifi.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "lwip/inet.h"

#include "string.h"
#include "stdio.h"
#include "stdint.h"

#include "esp_ota_ops.h"
#include "esp_partition.h"

// ------------------------------------------------------------------------ Extern variables
extern IotUARTHandle_t xConsoleUart;

// ------------------------------------------------------------------------ Defines
#define FIRMWARE_VERSION	"V2.3"
// gpio GPIO_NUM_33 adc1-5
#define SOIL_MOISTURE_SENSOR_PIN	ADC1_CHANNEL_5
#define LIGHT_SENSOR_PIN			ADC1_CHANNEL_5
#define LED_INDICATOR	GPIO_NUM_14
#define SETTING_BTN		GPIO_NUM_13
#define FluidValv1_PIN	GPIO_NUM_4
#define FluidValv2_PIN	GPIO_NUM_2
#define DHT22_PIN		GPIO_NUM_13

#define MILLISECONDS_PER_SECOND		( 1000U )
#define MILLISECONDS_PER_TICK		( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

#define NVS_KEY_WIFI_SSID		"NVS-KEY1"
#define NVS_KEY_WIFI_PASSWORD	"NVS-KEY2"
#define NVS_KEY_DEVICE_NAME		"NVS-KEY3"
#define NVS_KEY_HOST			"NVS-KEY4"
#define NVS_KEY_PORT			"NVS-KEY5"
#define NVS_KEY_THING_NAME		"NVS-KEY6"
#define NVS_KEY_ROOT_KEY		"NVS-KEY7"
#define NVS_KEY_CERT_KEY		"NVS-KEY8"
#define NVS_KEY_PRIVATE_KEY		"NVS-KEY9"
#define NVS_KEY_TYPE_DEVICE		"NVS-KEY10"
#define NVS_KEY_HTTPS_URL		"NVS-KEY11"
#define NVS_KEY_QUALITY_IMG		"NVS-KEY12"

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

#define FluidValv1_AWS_MQTT_SUB_PUB	"vw"
#define FluidValv2_AWS_MQTT_SUB_PUB	"vn"
#define TIME_VW_AWS_MQTT_SUB_PUB	"TimeVw"
#define TIME_VN_AWS_MQTT_SUB_PUB	"TimeVn"
#define SoilMoisture_AWS_MQTT_PUB	"sh"
#define TEMPERATURE_AIR_AWS_MQTT_PUB	"t"
#define HUMIDITY_AIR_AWS_MQTT_PUB		"h"
#define ILLUMINATION_AWS_MQTT_PUB		"ls"

#define AWS_MQTT_COMAND_REQ_IMG		"imageReq"
#define AWS_MQTT_COMAND_UPDATE_FIRMWARE_URL		"UpdateFirmwareURL"
#define AWS_MQTT_COMAND_UPDATE_FIRMWARE_LEN		"UpdateFirmwareLen"
#define AWS_MQTT_COMAND_HTTPS_HEAD_AUTORIZATION		"Authorization"

#define MQTT_KEEPALIVE_SEC	650U
#define AWS_MQTT_TIMER_VALUE_UPDATE_INFO	600000U
#define AWS_MQTT_TIMER_VALUE_PING_BROKER	45000U

#define TYPE_DEVICE_NONE	0
#define TYPE_DEVICE_USER	1
#define TYPE_DEVICE_GENERAL	2

#define HTTP_REQUEST_KEEP_ALIVE_FLAG    0x1U
// ------------------------------------------------------------------------ Global variables
static const char *TAG = "Global tag";

static uint8_t type_device = TYPE_DEVICE_NONE;

static char buffer_json_tx[ 7168 ] = { 0 };
static char buffer_json_rx[ 7168 ] = { 0 };

const char wifi_connect_SSID[32];
const char wifi_connect_password[32];
bool led_indicator_state = false;

char tcp_server_requestId[16];
bool tcp_server_flag_rec_comand_get = false;
bool tcp_server_flag_rec_comand_set = false;

struct NetworkContext
{
    SecureSocketsTransportParams_t * pParams;
};
NetworkContext_t MQTTNetworkContext = { 0 };
SecureSocketsTransportParams_t MQTTSecureSocketsTransportParams = { 0 };
static MQTTContext_t GlobalMqttContext;
static uint32_t ulGlobalEntryTimeMs;
static uint8_t pcNetworkBuffer[1024U];

static uint8_t ucUserBuffer[1024];
static size_t ucUserBuffer_len = sizeof(ucUserBuffer);

const char aws_device_name[64];
const char aws_host[64];
const char aws_port[8];
const char aws_thing_name[64];
const char aws_root_key[2048];
const char aws_cert_key[2048];
const char aws_private_key[2048];
const char aws_type_device[16];
const char https_send_photo_URL[256];
const char camera_quality_image[16];
char aws_pub_topic[128];
char aws_pub_topic_photo[128];
char aws_sub_topic[128];
char https_get_firmware_URL[256];
char https_head_autorization[72];
size_t https_firmware_len = 0;

MQTTSubscribeInfo_t MySubsInfo;
MQTTPublishInfo_t MyPubInfo = { 0 };
uint16_t packetId_PUB;

bool aws_flag_accepted_comand = true;
bool aws_flag_once1 = true;
int FluidValv1_state = 0;
int FluidValv2_state = 0;
uint32_t time_vw = 0;
uint32_t time_vn = 0;
uint32_t timer_vw = 0;
uint32_t timer_vn = 0;
int SoilMoisture = 0;
int old_SoilMoisture = 0;
bool flag_change_sensor_values = false;
int aws_image_req_comand = 0;
int aws_update_firmware_comand = 0;

float temperature_air = 0;
float humidity_air = 0;
int illumination_value = 0;

float old_temperature_air = 0;
float old_humidity_air = 0;
int old_illumination_value = 0;

uint32_t timer_300ms = 0;
uint32_t timer_ping_broker = 0;
uint32_t timer_mqtt_pub = 0;
uint32_t timer_send_photo_https = 0;

// ------------------------------------------------------------------------ Prototype functions
void aws_mqtt_parse_msg(char* inp);

// ------------------------------------------------------------------------ Event handler functions
static void prvEventCallback( MQTTContext_t * pMqttContext, MQTTPacketInfo_t * pPacketInfo, MQTTDeserializedInfo_t * pDeserializedInfo )
{
    if(pPacketInfo->type == 0x30)
    {
        for(int i = 0; i < pPacketInfo->remainingLength; i++)
        {
            if(pPacketInfo->pRemainingData[0] == '{') break;
            pPacketInfo->pRemainingData++;
        }
        memset(buffer_json_rx, 0, sizeof(buffer_json_rx));
        memcpy(buffer_json_rx, pPacketInfo->pRemainingData, strlen((char *)pPacketInfo->pRemainingData));
        aws_mqtt_parse_msg(buffer_json_rx);
    }
}

// ------------------------------------------------------------------------ Functions
void toogle_led_incicator()
{
	led_indicator_state = !led_indicator_state;
	gpio_set_level(LED_INDICATOR, led_indicator_state);
}

void error_func(char* error_problem_msg, int number_of_flashes_per_2_second)
{
	const int delay_on = 45;
	const int delay_off = 250;
	int delay = 3000 - (number_of_flashes_per_2_second * (delay_on + delay_off));

	gpio_pad_select_gpio(LED_INDICATOR);
    gpio_set_direction(LED_INDICATOR, GPIO_MODE_OUTPUT);

	while(1)
	{
		for(int i = 0; i < number_of_flashes_per_2_second; i++)
		{
			gpio_set_level(LED_INDICATOR, 1);
			vTaskDelay(pdMS_TO_TICKS(delay_on));
			gpio_set_level(LED_INDICATOR, 0);
			vTaskDelay(pdMS_TO_TICKS(delay_off));
		}

		printf(error_problem_msg);
		vTaskDelay(pdMS_TO_TICKS(delay));
	}
}

void restart_device(char* info_restart, int delay_value)
{
	printf(info_restart);
	vTaskDelay(pdMS_TO_TICKS(delay_value));
	esp_restart();
}

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static uint32_t prvGetTimeMs( void )
{
    TickType_t xTickCount = 0;
    uint32_t ulTimeMs = 0UL;

    /* Get the current tick count. */
    xTickCount = xTaskGetTickCount();

    /* Convert the ticks to milliseconds. */
    ulTimeMs = ( uint32_t ) xTickCount * MILLISECONDS_PER_TICK;

    /* Reduce ulGlobalEntryTimeMs from obtained time so as to always return the
     * elapsed time in the application. */
    ulTimeMs = ( uint32_t ) ( ulTimeMs - ulGlobalEntryTimeMs );

    return ulTimeMs;
}

void gpio_init_type_user()
{
	gpio_pad_select_gpio(LED_INDICATOR);
    gpio_set_direction(LED_INDICATOR, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(FluidValv1_PIN);
    gpio_set_direction(FluidValv1_PIN, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(FluidValv2_PIN);
    gpio_set_direction(FluidValv2_PIN, GPIO_MODE_OUTPUT);

    adc_gpio_init(ADC_UNIT_1, SOIL_MOISTURE_SENSOR_PIN);
    adc1_config_channel_atten(SOIL_MOISTURE_SENSOR_PIN, ADC_ATTEN_DB_11);
}

void gpio_init_type_general()
{
	gpio_pad_select_gpio(LED_INDICATOR);
    gpio_set_direction(LED_INDICATOR, GPIO_MODE_OUTPUT);

    adc_gpio_init(ADC_UNIT_1, LIGHT_SENSOR_PIN);
    adc1_config_channel_atten(LIGHT_SENSOR_PIN, ADC_ATTEN_DB_11);
}

void nvs_read_parametrs()
{
	nvs_handle nvs_my_handle;
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_my_handle);
	if (err != ESP_OK) 
	{
		printf("Nvs, error open\n");
	} 
	else 
	{
		printf("Nvs, ok open\n");

		char* value;
		size_t string_size;

		err = nvs_get_str(nvs_my_handle, NVS_KEY_WIFI_SSID, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_WIFI_SSID, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)wifi_connect_SSID, 0, sizeof(wifi_connect_SSID));
	      memcpy((void *)wifi_connect_SSID, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_WIFI_PASSWORD, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_WIFI_PASSWORD, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)wifi_connect_password, 0, sizeof(wifi_connect_password));
	      memcpy((void *)wifi_connect_password, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_DEVICE_NAME, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_DEVICE_NAME, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_device_name, 0, sizeof(aws_device_name));
	      memcpy((void *)aws_device_name, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_HOST, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_HOST, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_host, 0, sizeof(aws_host));
	      memcpy((void *)aws_host, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_PORT, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_PORT, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_port, 0, sizeof(aws_port));
	      memcpy((void *)aws_port, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_THING_NAME, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_THING_NAME, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_thing_name, 0, sizeof(aws_thing_name));
	      memcpy((void *)aws_thing_name, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_ROOT_KEY, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_ROOT_KEY, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_root_key, 0, sizeof(aws_root_key));
	      memcpy((void *)aws_root_key, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_CERT_KEY, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_CERT_KEY, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_cert_key, 0, sizeof(aws_cert_key));
	      memcpy((void *)aws_cert_key, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_PRIVATE_KEY, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_PRIVATE_KEY, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_private_key, 0, sizeof(aws_private_key));
	      memcpy((void *)aws_private_key, value, strlen(value));
	    }

		err = nvs_get_str(nvs_my_handle, NVS_KEY_TYPE_DEVICE, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_TYPE_DEVICE, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)aws_type_device, 0, sizeof(aws_type_device));
	      memcpy((void *)aws_type_device, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_HTTPS_URL, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_HTTPS_URL, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)https_send_photo_URL, 0, sizeof(https_send_photo_URL));
	      memcpy((void *)https_send_photo_URL, value, strlen(value));
	    }

	    err = nvs_get_str(nvs_my_handle, NVS_KEY_QUALITY_IMG, NULL, &string_size);
	    value = malloc(string_size);
	    err = nvs_get_str(nvs_my_handle, NVS_KEY_QUALITY_IMG, value, &string_size);
	    if(err == ESP_OK)
	    {
	      memset((void *)camera_quality_image, 0, sizeof(camera_quality_image));
	      memcpy((void *)camera_quality_image, value, strlen(value));
	    }

	    err = nvs_commit(nvs_my_handle);
	    nvs_close(nvs_my_handle);
	}
}

void nsv_save_parametrs()
{
	nvs_handle nvs_my_handle;
	esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_my_handle);
	if (err != ESP_OK) 
	{
		printf("Nvs, error open\n");
	} 
	else
	{
		printf("Nvs, ok open\n");

		err = nvs_set_str(nvs_my_handle, NVS_KEY_WIFI_SSID, wifi_connect_SSID);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_WIFI_PASSWORD, wifi_connect_password);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_DEVICE_NAME, aws_device_name);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_HOST, aws_host);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_PORT, aws_port);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_THING_NAME, aws_thing_name);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_ROOT_KEY, aws_root_key);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_CERT_KEY, aws_cert_key);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_PRIVATE_KEY, aws_private_key);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_TYPE_DEVICE, aws_type_device);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_HTTPS_URL, https_send_photo_URL);
		err = nvs_set_str(nvs_my_handle, NVS_KEY_QUALITY_IMG, camera_quality_image);

		err = nvs_commit(nvs_my_handle);
		nvs_close(nvs_my_handle);
	}

	vTaskDelay(pdMS_TO_TICKS(30));
}

void print_parametrs()
{
	printf("Device firmware version: "FIRMWARE_VERSION"\n");
	printf("Parametrs, WifiSSID:");
	printf(wifi_connect_SSID);
	printf("\n");
	printf("Parametrs, WifiPassword:");
	printf(wifi_connect_password);
	printf("\n");
	printf("Parametrs, DeviceName:");
	printf(aws_device_name);
	printf("\n");
	printf("Parametrs, AWSEndPoint:");
	printf(aws_host);
	printf("\n");
	printf("Parametrs, AWSPort:");
	printf(aws_port);
	printf("\n");
	printf("Parametrs, ThingName:");
	printf(aws_thing_name);
	printf("\n");
	printf("Parametrs, RootCA:");
	printf(aws_root_key);
	printf("\n");
	printf("Parametrs, CertKey:");
	printf(aws_cert_key);
	printf("\n");
	printf("Parametrs, PrivateKey:");
	printf(aws_private_key);
	printf("\n");
	printf("Parametrs, aws_type_device:");
	printf(aws_type_device);
	printf("\n");
	printf("Parametrs, https_send_photo_URL:");
	printf(https_send_photo_URL);
	printf("\n");
	printf("Parametrs, camera_quality_image:");
	printf(camera_quality_image);
	printf("\n");
}

bool check_parametrs()
{
	bool ret_state = true;
	if(strlen(wifi_connect_SSID) == 0) ret_state = false;
	if(strlen(wifi_connect_password) == 0) ret_state = false;
	if(strlen(aws_device_name) == 0) ret_state = false;
	if(strlen(aws_host) == 0) ret_state = false;
	if(strlen(aws_thing_name) == 0) ret_state = false;
	if(strlen(aws_root_key) == 0) ret_state = false;
	if(strlen(aws_cert_key) == 0) ret_state = false;
	if(strlen(aws_private_key) == 0) ret_state = false;
	if(strlen(aws_type_device) == 0 && (strcmp ("USER", aws_type_device) != 0 ||  strcmp ("GENERAL", aws_type_device) != 0)) ret_state = false;
	if(strlen(https_send_photo_URL) == 0) ret_state = false;
	if(strlen(camera_quality_image) == 0 && (strcmp ("LOW", camera_quality_image) != 0 ||  strcmp ("MEDIUM", camera_quality_image) != 0 || strcmp ("HIGH", camera_quality_image) != 0)) ret_state = false;

	if(strcmp ("USER", aws_type_device) == 0) type_device = TYPE_DEVICE_USER;
	if(strcmp ("GENERAL", aws_type_device) == 0) type_device = TYPE_DEVICE_GENERAL;

	return ret_state;
}

void tcp_server_parse(char* inp)
{
	cJSON *json_processing = cJSON_Parse(inp);
	if(json_processing != NULL)
	{
		cJSON *actionObj = cJSON_GetObjectItem(json_processing, "action");
		cJSON *requestIdObj = cJSON_GetObjectItem(json_processing, "requestId");

		if(actionObj != NULL)
    	{
    		if (strcmp ("GET", actionObj->valuestring)==0)
    		{
    			tcp_server_flag_rec_comand_get = true;
    		}
    		if (strcmp ("SET", actionObj->valuestring)==0)
    		{
    			tcp_server_flag_rec_comand_set = true;
    		}
    	}

    	if(tcp_server_flag_rec_comand_set == true)
    	{
    		cJSON *PayloadObj = cJSON_GetObjectItem(json_processing, "payload");
    		if(PayloadObj != NULL)
    		{
    			cJSON *deviceNameObj = cJSON_GetObjectItem(PayloadObj, "deviceName");
    			if(deviceNameObj != NULL)
    			{
    				memset((void *)aws_device_name, 0, sizeof(aws_device_name));
    				memcpy((void *)aws_device_name, deviceNameObj->valuestring, strlen(deviceNameObj->valuestring));
    			}

    			cJSON *hostObj = cJSON_GetObjectItem(PayloadObj, "host");
    			if(hostObj != NULL)
    			{
    				memset((void *)aws_host, 0, sizeof(aws_host));
    				memcpy((void *)aws_host, hostObj->valuestring, strlen(hostObj->valuestring));
    			}

    			cJSON *portObj = cJSON_GetObjectItem(PayloadObj, "port");
    			if(portObj != NULL)
    			{
    				memset((void *)aws_port, 0, sizeof(aws_port));
    				memcpy((void *)aws_port, portObj->valuestring, strlen(portObj->valuestring));
    			}

    			cJSON *thingNameObj = cJSON_GetObjectItem(PayloadObj, "thingName");
    			if(thingNameObj != NULL)
    			{
    				memset((void *)aws_thing_name, 0, sizeof(aws_thing_name));
    				memcpy((void *)aws_thing_name, thingNameObj->valuestring, strlen(thingNameObj->valuestring));
    			}

    			cJSON *wifiNameObj = cJSON_GetObjectItem(PayloadObj, "wifiName");
    			if(wifiNameObj != NULL)
    			{
    				memset((void *)wifi_connect_SSID, 0, sizeof(wifi_connect_SSID));
    				memcpy((void *)wifi_connect_SSID, wifiNameObj->valuestring, strlen(wifiNameObj->valuestring));
    			}

    			cJSON *wifiPasswordObj = cJSON_GetObjectItem(PayloadObj, "wifiPassword");
    			if(wifiPasswordObj != NULL)
    			{
    				memset((void *)wifi_connect_password, 0, sizeof(wifi_connect_password));
    				memcpy((void *)wifi_connect_password, wifiPasswordObj->valuestring, strlen(wifiPasswordObj->valuestring));
    			}

    			cJSON *caRootObj = cJSON_GetObjectItem(PayloadObj, "caRoot");
    			if(caRootObj != NULL)
    			{
    				memset((void *)aws_root_key, 0, sizeof(aws_root_key));
    				memcpy((void *)aws_root_key, caRootObj->valuestring, strlen(caRootObj->valuestring));
    			}

    			cJSON *certKeyObj = cJSON_GetObjectItem(PayloadObj, "certKey");
    			if(certKeyObj != NULL)
    			{
    				memset((void *)aws_cert_key, 0, sizeof(aws_cert_key));
    				memcpy((void *)aws_cert_key, certKeyObj->valuestring, strlen(certKeyObj->valuestring));
    			}

    			cJSON *privateKeyObj = cJSON_GetObjectItem(PayloadObj, "privateKey");
    			if(privateKeyObj != NULL)
    			{
    				memset((void *)aws_private_key, 0, sizeof(aws_private_key));
    				memcpy((void *)aws_private_key, privateKeyObj->valuestring, strlen(privateKeyObj->valuestring));
    			}

				cJSON *deviceTypeObj = cJSON_GetObjectItem(PayloadObj, "deviceType");
    			if(deviceTypeObj != NULL)
    			{
    				memset((void *)aws_type_device, 0, sizeof(aws_type_device));
    				memcpy((void *)aws_type_device, deviceTypeObj->valuestring, strlen(deviceTypeObj->valuestring));
    			}

    			cJSON *imageQualityObj = cJSON_GetObjectItem(PayloadObj, "imageQuality");
    			if(imageQualityObj != NULL)
    			{
    				memset((void *)camera_quality_image, 0, sizeof(camera_quality_image));
    				memcpy((void *)camera_quality_image, imageQualityObj->valuestring, strlen(imageQualityObj->valuestring));
    			}

    			cJSON *imageURLObj = cJSON_GetObjectItem(PayloadObj, "imageURL");
    			if(imageURLObj != NULL)
    			{
    				memset((void *)https_send_photo_URL, 0, sizeof(https_send_photo_URL));
    				memcpy((void *)https_send_photo_URL, imageURLObj->valuestring, strlen(imageURLObj->valuestring));
    			}
    		}
    		nsv_save_parametrs();
    	}

    	if(requestIdObj != NULL)
    	{
    		memset((void *)tcp_server_requestId, 0, sizeof(tcp_server_requestId));
    		memcpy((void *)tcp_server_requestId, requestIdObj->valuestring, strlen(requestIdObj->valuestring));
    	}
	}
	else
	{
		printf("Parse json error\n");
	}

	cJSON_Delete(json_processing);
}

void tcp_server_create_json()
{
	cJSON *json_obj_main;
	cJSON *json_obj_payload;
	json_obj_main = cJSON_CreateObject();
	json_obj_payload = cJSON_CreateObject();
	if(tcp_server_flag_rec_comand_get == true)
	{
		cJSON_AddStringToObject(json_obj_payload, "deviceName", aws_device_name);
		cJSON_AddStringToObject(json_obj_payload, "host", aws_host);
		cJSON_AddStringToObject(json_obj_payload, "port", aws_port);
		cJSON_AddStringToObject(json_obj_payload, "thingName", aws_thing_name);
		cJSON_AddStringToObject(json_obj_payload, "wifiName", wifi_connect_SSID);
		cJSON_AddStringToObject(json_obj_payload, "wifiPassword", wifi_connect_password);
		cJSON_AddStringToObject(json_obj_payload, "caRoot", aws_root_key);
		cJSON_AddStringToObject(json_obj_payload, "certKey", aws_cert_key);
		cJSON_AddStringToObject(json_obj_payload, "privateKey", aws_private_key);
		cJSON_AddStringToObject(json_obj_payload, "deviceType", aws_type_device);
		cJSON_AddStringToObject(json_obj_payload, "imageURL", https_send_photo_URL);
		cJSON_AddStringToObject(json_obj_payload, "imageQuality", camera_quality_image);
		cJSON_AddItemToObject(json_obj_main, "payload", json_obj_payload);
	}
	cJSON_AddStringToObject(json_obj_main, "requestId", tcp_server_requestId);
	char *msg_json = cJSON_Print(json_obj_main);
	memset(buffer_json_tx, 0, sizeof(buffer_json_tx));
	memcpy(buffer_json_tx, msg_json, strlen(msg_json));
	cJSON_Delete(json_obj_main);
}

static void do_retransmit(const int sock)
{
    int len;

    do {
    	memset(buffer_json_rx, 0, sizeof(buffer_json_rx));
        len = recv(sock, buffer_json_rx, sizeof(buffer_json_rx) - 1, 0);
        if (len < 0) 
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } 
        else if (len == 0) 
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else 
        {
            buffer_json_rx[len] = 0; // Null-terminate whatever is received and treat it like a string

            tcp_server_parse(buffer_json_rx);

            if(tcp_server_flag_rec_comand_get == true)
            {
            	nvs_read_parametrs();
            	tcp_server_create_json();
	            send(sock, buffer_json_tx, strlen(buffer_json_tx), 0);
	            vTaskDelay(pdMS_TO_TICKS(300));
            	tcp_server_flag_rec_comand_get = false;
            }

            if(tcp_server_flag_rec_comand_set == true)
    		{
				tcp_server_create_json();
	            send(sock, buffer_json_tx, strlen(buffer_json_tx), 0);
	            vTaskDelay(pdMS_TO_TICKS(300));
    			tcp_server_flag_rec_comand_set = false;
    		}

        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) 
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(2020);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) 
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }

    err = listen(listen_sock, 1);
    if (err != 0) 
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) 
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);

    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

void setting_mode()
{
	// DHCP address server IP: 192.168.1.1
	WIFI_On();
	WIFI_SetMode(eWiFiModeAP);
	char AP_SSID[] = "DeviceIOT";
	char AP_password[] = "DeviceIOT";
	WIFINetworkParams_t WifiAPconf;
	memcpy(WifiAPconf.ucSSID, AP_SSID, strlen(AP_SSID));
	WifiAPconf.ucSSIDLength = strlen(AP_SSID);
	WifiAPconf.xSecurity = eWiFiSecurityWPA2;
	memcpy(WifiAPconf.xPassword.xWPA.cPassphrase, AP_password, strlen(AP_password));
    WifiAPconf.xPassword.xWPA.ucLength = strlen(AP_password);
	WIFI_ConfigureAP(&WifiAPconf);
	WIFI_StartAP();
	xTaskCreate(tcp_server_task, "tcp_server_task", 4096, NULL, 1, NULL);
	while(1)
	{
		printf("Setting mode\n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void aws_mqtt_parse_msg(char* inp)
{
	cJSON *json_processing = cJSON_Parse(buffer_json_rx);
	if(json_processing != NULL)
	{
		cJSON *state = cJSON_GetObjectItem(json_processing, "state");
		cJSON *desired = cJSON_GetObjectItem(state, "desired");

		if(desired != NULL && cJSON_IsNull(desired) == false)
		{
			if(type_device == TYPE_DEVICE_USER)
    		{
			  	cJSON *FluidValv1Obj = cJSON_GetObjectItem(desired, FluidValv1_AWS_MQTT_SUB_PUB);
				if(FluidValv1Obj != NULL)
				{
					FluidValv1_state = FluidValv1Obj->valueint;
					if(FluidValv1_state > 1) FluidValv1_state = 1;
					else if(FluidValv1_state < 0) FluidValv1_state = 0;
					printf("aws mqtt, rec comand "FluidValv1_AWS_MQTT_SUB_PUB"\n");
					timer_vw = prvGetTimeMs();
					aws_flag_accepted_comand = true;
				}

				cJSON *FluidValv2Obj = cJSON_GetObjectItem(desired, FluidValv2_AWS_MQTT_SUB_PUB);
				if(FluidValv2Obj != NULL)
				{
					FluidValv2_state = FluidValv2Obj->valueint;
					if(FluidValv2_state > 1) FluidValv2_state = 1;
					else if(FluidValv2_state < 0) FluidValv2_state = 0;
					printf("aws mqtt, rec comand "FluidValv2_AWS_MQTT_SUB_PUB"\n");
					timer_vn = prvGetTimeMs();
					aws_flag_accepted_comand = true;
				}

				cJSON *imageReqObj = cJSON_GetObjectItem(desired, AWS_MQTT_COMAND_REQ_IMG);
				if(imageReqObj != NULL)
				{
					aws_image_req_comand = imageReqObj->valueint;
					if(aws_image_req_comand > 1) aws_image_req_comand = 1;
					else if(aws_image_req_comand < 0) aws_image_req_comand = 0;
					printf("aws mqtt, rec comand "AWS_MQTT_COMAND_REQ_IMG"\n");
					aws_flag_accepted_comand = true;
				}

				cJSON *TimeVwObj = cJSON_GetObjectItem(desired, TIME_VW_AWS_MQTT_SUB_PUB);
				if(TimeVwObj != NULL)
				{
					time_vw = TimeVwObj->valueint;
					if(time_vw > 1800) time_vw = 1800;
					else if(time_vw < 0) time_vw = 0;
					printf("aws mqtt, rec comand "TIME_VW_AWS_MQTT_SUB_PUB"\n");
					aws_flag_accepted_comand = true;
				}

				cJSON *TimeVnObj = cJSON_GetObjectItem(desired, TIME_VN_AWS_MQTT_SUB_PUB);
				if(TimeVnObj != NULL)
				{
					time_vn = TimeVnObj->valueint;
					if(time_vn > 1800) time_vn = 1800;
					else if(time_vn < 0) time_vn = 0;
					printf("aws mqtt, rec comand "TIME_VN_AWS_MQTT_SUB_PUB"\n");
					aws_flag_accepted_comand = true;
				}
			}

			cJSON *HTTPSheadAutorizationObj = cJSON_GetObjectItem(desired, AWS_MQTT_COMAND_HTTPS_HEAD_AUTORIZATION);
			if(HTTPSheadAutorizationObj != NULL)
			{
				memset(https_head_autorization, 0, sizeof(https_head_autorization));
				strcat(https_head_autorization, HTTPSheadAutorizationObj->valuestring);
				printf("aws mqtt, rec comand "AWS_MQTT_COMAND_HTTPS_HEAD_AUTORIZATION"\n");
				aws_flag_accepted_comand = true;
			}

			cJSON *UpdateFirmwareURLObj = cJSON_GetObjectItem(desired, AWS_MQTT_COMAND_UPDATE_FIRMWARE_URL);
			cJSON *UpdateFirmwareLenObj = cJSON_GetObjectItem(desired, AWS_MQTT_COMAND_UPDATE_FIRMWARE_LEN);
			if(UpdateFirmwareURLObj != NULL && UpdateFirmwareLenObj != NULL)
			{
				https_firmware_len = UpdateFirmwareLenObj->valueint;
				memset(https_get_firmware_URL, 0, sizeof(https_get_firmware_URL));
				strcat(https_get_firmware_URL, UpdateFirmwareURLObj->valuestring);
				aws_update_firmware_comand = 1;
				printf("aws mqtt, rec comand UpdateFirmware\n");
				aws_flag_accepted_comand = true;
			}
		}
	}
	cJSON_Delete(json_processing);
}

void aws_mqtt_create_json_for_type_user()
{
	cJSON *json_obj_main;
	cJSON *json_obj_state;
	cJSON *json_obj_reported;
	cJSON *json_obj_desired;
	json_obj_state = cJSON_CreateObject();
	json_obj_reported = cJSON_CreateObject();
	json_obj_main = cJSON_CreateObject();
	json_obj_desired = cJSON_CreateObject();
	if(aws_update_firmware_comand == 1)
	{
		cJSON_AddStringToObject(json_obj_reported, "FirmwareVersion", "UPDATING");
	}
	else
	{
		cJSON_AddStringToObject(json_obj_reported, "FirmwareVersion", FIRMWARE_VERSION);
	}
	if(aws_flag_once1 == true)
	{
		cJSON_AddStringToObject(json_obj_reported, "type", aws_type_device);
		cJSON_AddStringToObject(json_obj_reported, "DeviceName", aws_device_name);
		aws_flag_once1 = false;
	}
	if(aws_flag_accepted_comand == true)
	{
		json_obj_desired = cJSON_CreateNull();
		aws_flag_accepted_comand = false;
	}
	cJSON_AddNumberToObject(json_obj_reported, FluidValv1_AWS_MQTT_SUB_PUB, FluidValv1_state);
	cJSON_AddNumberToObject(json_obj_reported, FluidValv2_AWS_MQTT_SUB_PUB, FluidValv2_state);
	cJSON_AddNumberToObject(json_obj_reported, SoilMoisture_AWS_MQTT_PUB, SoilMoisture);
	cJSON_AddNumberToObject(json_obj_reported, TIME_VW_AWS_MQTT_SUB_PUB, time_vw);
	cJSON_AddNumberToObject(json_obj_reported, TIME_VN_AWS_MQTT_SUB_PUB, time_vn);
	cJSON_AddItemToObject(json_obj_state, "reported", json_obj_reported);
	cJSON_AddItemToObject(json_obj_state, "desired", json_obj_desired);
	cJSON_AddItemToObject(json_obj_main, "state", json_obj_state);
	char *msg_json = cJSON_Print(json_obj_main);
	memset(buffer_json_tx, 0, sizeof(buffer_json_tx));
	memcpy(buffer_json_tx, msg_json, strlen(msg_json));
	cJSON_Delete(json_obj_main);
}

void aws_mqtt_create_json_for_type_general()
{
	cJSON *json_obj_main;
	cJSON *json_obj_state;
	cJSON *json_obj_reported;
	cJSON *json_obj_desired;
	json_obj_state = cJSON_CreateObject();
	json_obj_reported = cJSON_CreateObject();
	json_obj_main = cJSON_CreateObject();
	json_obj_desired = cJSON_CreateObject();
	if(aws_update_firmware_comand == 1)
	{
		cJSON_AddStringToObject(json_obj_reported, "FirmwareVersion", "UPDATING");
	}
	else
	{
		cJSON_AddStringToObject(json_obj_reported, "FirmwareVersion", FIRMWARE_VERSION);
	}
	if(aws_flag_once1 == true)
	{
		cJSON_AddStringToObject(json_obj_reported, "type", aws_type_device);
		cJSON_AddStringToObject(json_obj_reported, "DeviceName", aws_device_name);
		aws_flag_once1 = false;
	}
	if(aws_flag_accepted_comand == true)
	{
		json_obj_desired = cJSON_CreateNull();
		aws_flag_accepted_comand = false;
	}

	cJSON_AddNumberToObject(json_obj_reported, TEMPERATURE_AIR_AWS_MQTT_PUB, temperature_air);
	cJSON_AddNumberToObject(json_obj_reported, HUMIDITY_AIR_AWS_MQTT_PUB, humidity_air);
	cJSON_AddNumberToObject(json_obj_reported, ILLUMINATION_AWS_MQTT_PUB, illumination_value);

	cJSON_AddItemToObject(json_obj_state, "reported", json_obj_reported);
	cJSON_AddItemToObject(json_obj_state, "desired", json_obj_desired);
	cJSON_AddItemToObject(json_obj_main, "state", json_obj_state);
	char *msg_json = cJSON_Print(json_obj_main);
	memset(buffer_json_tx, 0, sizeof(buffer_json_tx));
	memcpy(buffer_json_tx, msg_json, strlen(msg_json));
	cJSON_Delete(json_obj_main);
}

void aws_net_init()
{
	vDevModeKeyProvisioning();
	IotSdk_Init();
    AwsIotNetworkManager_Init();
    AwsIotNetworkManager_EnableNetwork(AWSIOT_NETWORK_TYPE_WIFI);
}

void aws_mqtt_connect()
{
	// ------------- socket connect
    ServerInfo_t xServerInfo = { 0 };
    SocketsConfig_t xSocketConfig = { 0 };

    MQTTNetworkContext.pParams = &MQTTSecureSocketsTransportParams;

    xServerInfo.pHostName = aws_host;
    xServerInfo.hostNameLength = strlen(aws_host);
    xServerInfo.port = atoi(aws_port);

    xSocketConfig.pAlpnProtos = NULL;
    xSocketConfig.enableTls = true;
    xSocketConfig.disableSni = false;
    xSocketConfig.sendTimeoutMs = 500U;
    xSocketConfig.recvTimeoutMs = 500U;
    xSocketConfig.pRootCa = aws_root_key;
    xSocketConfig.rootCaSize = sizeof( char ) + strlen(aws_root_key);
    xSocketConfig.maxFragmentLength = 0;

    if(SecureSocketsTransport_Connect( &MQTTNetworkContext, &xServerInfo, &xSocketConfig ) != TRANSPORT_SOCKET_STATUS_SUCCESS)
    {
    	restart_device("Mqtt, net error connect\n", 30000);
    }
    else
    {
    	printf("Mqtt, net connect ok\n");
    }

    // --------------- mqtt init
    MQTTFixedBuffer_t MQTTNetworkBuffer;
    MQTTNetworkBuffer.pBuffer = pcNetworkBuffer;
    MQTTNetworkBuffer.size = sizeof(pcNetworkBuffer);

    TransportInterface_t MQTTTransport;
    MQTTTransport.pNetworkContext = &MQTTNetworkContext;
    MQTTTransport.send = SecureSocketsTransport_Send;
    MQTTTransport.recv = SecureSocketsTransport_Recv;

    if(MQTT_Init( &GlobalMqttContext, &MQTTTransport, prvGetTimeMs, prvEventCallback, &MQTTNetworkBuffer ) != MQTTSuccess)
    {
    	restart_device("Mqtt, init error\n", 30000);
    }
    else
    {
    	printf("Mqtt, init ok\n");
    }

    // ---------------- mqtt connect
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;
    memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );
    xConnectInfo.cleanSession = true;
    xConnectInfo.pClientIdentifier = aws_thing_name;
    xConnectInfo.clientIdentifierLength = (uint16_t)strlen(aws_thing_name);
    xConnectInfo.keepAliveSeconds = MQTT_KEEPALIVE_SEC;
    if(MQTT_Connect( &GlobalMqttContext, &xConnectInfo, NULL, 5000U, &xSessionPresent ) != MQTTSuccess)
    {
    	restart_device("Mqtt, connect error\n", 30000);
    }
    else
    {
    	printf("Mqtt, connect ok\n");
    }

    // -------------- mqtt subs
	memset(aws_sub_topic, 0, sizeof(aws_sub_topic));
	strcat(aws_sub_topic, "$aws/things/");
	strcat(aws_sub_topic, aws_thing_name);
	strcat(aws_sub_topic, "/shadow/update/accepted");

    size_t my_subscriptionCount = 1;
    MySubsInfo.qos = MQTTQoS0;
    MySubsInfo.pTopicFilter = aws_sub_topic;
    MySubsInfo.topicFilterLength = strlen(aws_sub_topic);
    uint16_t packetId_SUB = MQTT_PACKET_ID_INVALID;
    packetId_SUB = MQTT_GetPacketId( &GlobalMqttContext );
    
  	if(MQTT_Subscribe( &GlobalMqttContext, &MySubsInfo, my_subscriptionCount, packetId_SUB) != MQTTSuccess)
    {
    	restart_device("Mqtt, subs error\n", 30000);
    }
    else
    {
    	printf("Mqtt, subs ok\n");
    }

    // -------------- mqtt pub init
    memset(aws_pub_topic, 0, sizeof(aws_pub_topic));
	strcat(aws_pub_topic, "$aws/things/");
	strcat(aws_pub_topic, aws_thing_name);
	strcat(aws_pub_topic, "/shadow/update");

    MyPubInfo.qos = MQTTQoS0;
    MyPubInfo.retain = false;
    MyPubInfo.dup = false;
    MyPubInfo.pTopicName = aws_pub_topic;
    MyPubInfo.topicNameLength = strlen(aws_pub_topic);
    packetId_PUB = MQTT_PACKET_ID_INVALID;
}

void check_setting_mode()
{
	gpio_pad_select_gpio(SETTING_BTN);
    gpio_set_direction(SETTING_BTN, GPIO_MODE_INPUT);
    if(gpio_get_level(SETTING_BTN) == 0) setting_mode();
    gpio_set_direction(SETTING_BTN, GPIO_MODE_DISABLE);
}

void camera_init()
{
	camera_config_t camera_config = 
	{
	    .pin_pwdn = CAM_PIN_PWDN,
	    .pin_reset = CAM_PIN_RESET,
	    .pin_xclk = CAM_PIN_XCLK,
	    .pin_sscb_sda = CAM_PIN_SIOD,
	    .pin_sscb_scl = CAM_PIN_SIOC,

	    .pin_d7 = CAM_PIN_D7,
	    .pin_d6 = CAM_PIN_D6,
	    .pin_d5 = CAM_PIN_D5,
	    .pin_d4 = CAM_PIN_D4,
	    .pin_d3 = CAM_PIN_D3,
	    .pin_d2 = CAM_PIN_D2,
	    .pin_d1 = CAM_PIN_D1,
	    .pin_d0 = CAM_PIN_D0,
	    .pin_vsync = CAM_PIN_VSYNC,
	    .pin_href = CAM_PIN_HREF,
	    .pin_pclk = CAM_PIN_PCLK,

	    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
	    .xclk_freq_hz = 5000000,
	    .ledc_timer = LEDC_TIMER_0,
	    .ledc_channel = LEDC_CHANNEL_0,

	    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
	    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

	    .jpeg_quality = 12, //0-63 lower number means higher quality
	    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
	    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
	};

	if(strcmp ("LOW", camera_quality_image) == 0) camera_config.frame_size = FRAMESIZE_QVGA;
	if(strcmp ("MEDIUM", camera_quality_image) == 0) camera_config.frame_size = FRAMESIZE_SVGA;
	if(strcmp ("HIGH", camera_quality_image) == 0) camera_config.frame_size = FRAMESIZE_UXGA;

	esp_camera_init(&camera_config);
}

void camera_test_uart()
{
	while(1)
	{
        camera_fb_t *pic = esp_camera_fb_get();// use pic->buf to access the image
        iot_uart_write_async(xConsoleUart, "CAM_IMG:", 8);
        iot_uart_write_async(xConsoleUart, pic->buf, pic->len);
        iot_uart_write_async(xConsoleUart, "END_IMG\n", 8);
        //printf("Pic len:%d\n", pic->len);
        esp_camera_fb_return(pic);
		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

void send_photo_https()
{
	camera_fb_t *pic = esp_camera_fb_get();

	// ------------------------------- connect net
	#define HTTPS_POST_PORT    443
	#define HTTPS_POST_TRANSPORT_SEND_RECV_TIMEOUT_MS	(5000)

	NetworkContext_t HTTPSNetworkContext = { 0 };
	SecureSocketsTransportParams_t HTTPSSecureSocketsTransportParams = { 0 };
	ServerInfo_t HTTPSServerInfo = { 0 };
    SocketsConfig_t HTTPSSocketsConfig = { 0 };
    TransportInterface_t HTTPSTransportInterface;
    HTTPRequestHeaders_t xRequestHeaders;
    HTTPRequestInfo_t xRequestInfo;
 	HTTPResponse_t xResponse;
    char cServerHost[256];
    const char * pcAddress = NULL;
	size_t xServerHostLength;
	const char * pcPath;
	size_t xPathLen = 0;

    getUrlAddress( https_send_photo_URL, strlen(https_send_photo_URL), &pcAddress, &xServerHostLength );
    memcpy( cServerHost, pcAddress, xServerHostLength );
    cServerHost[xServerHostLength] = '\0';

    HTTPSServerInfo.pHostName = cServerHost;
    HTTPSServerInfo.hostNameLength = xServerHostLength;
    HTTPSServerInfo.port = HTTPS_POST_PORT;

    HTTPSSocketsConfig.enableTls = true;
    HTTPSSocketsConfig.pAlpnProtos = NULL;
    HTTPSSocketsConfig.maxFragmentLength = 0;
    HTTPSSocketsConfig.disableSni = false;
    if(HTTPSSocketsConfig.enableTls)
    {
    	HTTPSSocketsConfig.pRootCa = tlsATS1_ROOT_CERTIFICATE_PEM;
    	HTTPSSocketsConfig.rootCaSize = tlsATS1_ROOT_CERTIFICATE_LENGTH;
	}
    HTTPSSocketsConfig.sendTimeoutMs = HTTPS_POST_TRANSPORT_SEND_RECV_TIMEOUT_MS;
    HTTPSSocketsConfig.recvTimeoutMs = HTTPS_POST_TRANSPORT_SEND_RECV_TIMEOUT_MS;

    HTTPSNetworkContext.pParams = &HTTPSSecureSocketsTransportParams;

    if(SecureSocketsTransport_Connect( &HTTPSNetworkContext, &HTTPSServerInfo, &HTTPSSocketsConfig ) != TRANSPORT_SOCKET_STATUS_SUCCESS)
    {
        printf("HTTPS, net connect error\n");
        return;
    }
    
    HTTPSTransportInterface.pNetworkContext = &HTTPSNetworkContext;
    HTTPSTransportInterface.send = SecureSocketsTransport_Send;
    HTTPSTransportInterface.recv = SecureSocketsTransport_Recv;
    
    ( void ) memset( &xRequestHeaders, 0, sizeof( xRequestHeaders ) );
    ( void ) memset( &xRequestInfo, 0, sizeof( xRequestInfo ) );
    ( void ) memset( &xResponse, 0, sizeof( xResponse ) );

    getUrlPath( https_send_photo_URL, strlen(https_send_photo_URL), &pcPath, &xPathLen );

    printf("HTTPS, host:");
    printf(cServerHost);
    printf("\n");
    printf("HTTPS, pcPath:");
    printf(pcPath);
    printf("\n");

    xRequestInfo.pHost = cServerHost;
    xRequestInfo.hostLen = xServerHostLength;
    xRequestInfo.pMethod = HTTP_METHOD_POST;
    xRequestInfo.methodLen = sizeof( HTTP_METHOD_POST ) - 1;
    xRequestInfo.pPath = pcPath;
    xRequestInfo.pathLen = strlen( pcPath );
    xRequestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    memset(ucUserBuffer, 0, sizeof(ucUserBuffer));

    xRequestHeaders.pBuffer = ucUserBuffer;
    xRequestHeaders.bufferLen = ucUserBuffer_len;

    xResponse.pBuffer = ucUserBuffer;
    xResponse.bufferLen = ucUserBuffer_len;
    
    if(HTTPClient_InitializeRequestHeaders( &xRequestHeaders, &xRequestInfo ) != HTTPSuccess)
    {
    	printf("HTTPS, InitializeRequestHeaders error\n");
    }

    HTTPClient_AddHeader(&xRequestHeaders, "Content-Type", strlen("Content-Type"), "image/jpeg", strlen("image/jpeg"));
	char content_len_str[16] = { 0 };
    itoa(pic->len, content_len_str, 10);
    HTTPClient_AddHeader(&xRequestHeaders, "Content-Length", strlen("Content-Length"), content_len_str, strlen(content_len_str));


    printf("HTTPS, headers:\n");
    printf((char *)xRequestHeaders.pBuffer);
    printf("\n");

    HTTPClient_Send( &HTTPSTransportInterface, &xRequestHeaders, pic->buf, pic->len, &xResponse, 0 );

    printf("HTTPS, headers responce:\n");
    printf((char *)xResponse.pBuffer);
    printf("\n");

    SecureSocketsTransport_Disconnect( &HTTPSNetworkContext);

    esp_camera_fb_return(pic);
}

void https_get_firmware(char* URL_S3_FILE, size_t len_firmware)
{
	// ------------------------------- connect net
	#define HTTPS_GET_PORT    80
	#define HTTPS_GET_TRANSPORT_SEND_RECV_TIMEOUT_MS	(10000)

	NetworkContext_t HTTPSNetworkContext = { 0 };
	SecureSocketsTransportParams_t HTTPSSecureSocketsTransportParams = { 0 };
	ServerInfo_t HTTPSServerInfo = { 0 };
    SocketsConfig_t HTTPSSocketsConfig = { 0 };
    TransportInterface_t HTTPSTransportInterface;
    HTTPRequestHeaders_t xRequestHeaders;
    HTTPRequestInfo_t xRequestInfo;
 	HTTPResponse_t xResponse;
    char cServerHost[256];
    const char * pcAddress = NULL;
	size_t xServerHostLength;
	const char * pcPath;
	size_t xPathLen = 0;

    getUrlAddress( URL_S3_FILE, strlen(URL_S3_FILE), &pcAddress, &xServerHostLength );
    memcpy( cServerHost, pcAddress, xServerHostLength );
    cServerHost[xServerHostLength] = '\0';

    HTTPSServerInfo.pHostName = cServerHost;
    HTTPSServerInfo.hostNameLength = xServerHostLength;
    HTTPSServerInfo.port = HTTPS_GET_PORT;

    HTTPSSocketsConfig.enableTls = false;
    HTTPSSocketsConfig.pAlpnProtos = NULL;
    HTTPSSocketsConfig.maxFragmentLength = 0;
    HTTPSSocketsConfig.disableSni = false;
    if(HTTPSSocketsConfig.enableTls)
    {
    	HTTPSSocketsConfig.pRootCa = tlsATS1_ROOT_CERTIFICATE_PEM;
    	HTTPSSocketsConfig.rootCaSize = tlsATS1_ROOT_CERTIFICATE_LENGTH;
	}
    HTTPSSocketsConfig.sendTimeoutMs = HTTPS_GET_TRANSPORT_SEND_RECV_TIMEOUT_MS;
    HTTPSSocketsConfig.recvTimeoutMs = HTTPS_GET_TRANSPORT_SEND_RECV_TIMEOUT_MS;

    HTTPSNetworkContext.pParams = &HTTPSSecureSocketsTransportParams;


    if(SecureSocketsTransport_Connect( &HTTPSNetworkContext, &HTTPSServerInfo, &HTTPSSocketsConfig ) != TRANSPORT_SOCKET_STATUS_SUCCESS)
    {
        printf("HTTPS, net connect error\n");
        return;
    }

    HTTPSTransportInterface.pNetworkContext = &HTTPSNetworkContext;
    HTTPSTransportInterface.send = SecureSocketsTransport_Send;
    HTTPSTransportInterface.recv = SecureSocketsTransport_Recv;
    
    ( void ) memset( &xRequestHeaders, 0, sizeof( xRequestHeaders ) );
    ( void ) memset( &xRequestInfo, 0, sizeof( xRequestInfo ) );
    ( void ) memset( &xResponse, 0, sizeof( xResponse ) );

    getUrlPath( URL_S3_FILE, strlen(URL_S3_FILE), &pcPath, &xPathLen );

    printf("HTTPS, host:");
    printf(cServerHost);
    printf("\n");
    printf("HTTPS, pcPath:");
    printf(pcPath);
    printf("\n");

    xRequestInfo.pHost = cServerHost;
    xRequestInfo.hostLen = xServerHostLength;
    xRequestInfo.pMethod = HTTP_METHOD_GET;
    xRequestInfo.methodLen = sizeof( HTTP_METHOD_GET ) - 1;
    xRequestInfo.pPath = pcPath;
    xRequestInfo.pathLen = strlen( pcPath );
    xRequestInfo.reqFlags = HTTP_REQUEST_KEEP_ALIVE_FLAG;

    int buffer_https_len = 1024 * 1024 * 2;
    char * buffer_https = (char*) malloc(buffer_https_len + 1);

    memset(buffer_https, 0, buffer_https_len);
    xRequestHeaders.pBuffer = buffer_https;
    xRequestHeaders.bufferLen = buffer_https_len;
    xResponse.pBuffer = buffer_https;
    xResponse.bufferLen = buffer_https_len;
    
    if(HTTPClient_InitializeRequestHeaders( &xRequestHeaders, &xRequestInfo ) != HTTPSuccess)
    {
    	printf("HTTPS, InitializeRequestHeaders error\n");
    }

    if(strlen(https_head_autorization) > 0)
    {
    	HTTPClient_AddHeader(&xRequestHeaders, "Authorization", strlen("Authorization"), https_head_autorization, strlen(https_head_autorization));
    	memset(https_head_autorization, 0, sizeof(https_head_autorization));
    }

    printf("HTTPS, headers:\n");
    printf((char *)xRequestHeaders.pBuffer);
    printf("\n");

    HTTPClient_Send( &HTTPSTransportInterface, &xRequestHeaders, NULL, 0, &xResponse, 0 );

    printf("HTTPS, head responce:\n");
    iot_uart_write_async(xConsoleUart, xResponse.pHeaders, xResponse.headersLen);
    printf("\n");
    printf("HTTPS, headers responce len: %d\n", xResponse.bodyLen);

    if(xResponse.bodyLen == len_firmware)
    {
	    const esp_partition_t *update_partition = NULL;
	    esp_ota_handle_t update_handle = 0 ;
	    update_partition = esp_ota_get_next_update_partition(NULL);

	    if(esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle) != ESP_OK)
	    {
	    	printf("OTA, begin error\n");
	    }
	    else
	    {
	    	printf("OTA, begin ok\n");
	    	if(esp_ota_write( update_handle, (const void *)xResponse.pBody, xResponse.bodyLen) != ESP_OK)
	    	{
	    		printf("OTA, write error\n");
	    	}
	    	else
	    	{
	    		printf("OTA, write ok\n");
	    		if (esp_ota_end(update_handle) != ESP_OK)
		    	{
		    		printf("OTA, end error\n");
		    	}
		    	else
		    	{
		    		printf("OTA, end ok\n");
		    		if(esp_ota_set_boot_partition(update_partition) != ESP_OK)
		    		{
		    			printf("OTA, update partition error\n");
		    		}
		    		else
		    		{
		    			printf("OTA, update partition ok\n");
		    			vTaskDelay(pdMS_TO_TICKS(2000));
		    			esp_restart();
		    		}
		    	}

	    	}
	    }
	}

    free(buffer_https);

    SecureSocketsTransport_Disconnect( &HTTPSNetworkContext);
}

int getSignalLevel( int usTimeOut, bool state )
{
	int uSec = 0;
	while( gpio_get_level(DHT22_PIN)==state ) {

		if( uSec > usTimeOut ) 
			return -1;
		++uSec;
		ets_delay_us(1);		// uSec delay
	}
	return uSec;
}

esp_err_t readDHT()
{

	int uSec = 0;
	uint8_t dhtData[5];
	uint8_t byteInx = 0;
	uint8_t bitInx = 7;

	for (int k = 0; k<5; k++) 
		dhtData[k] = 0;

	// == Send start signal to DHT sensor ===========

	gpio_set_direction( DHT22_PIN, GPIO_MODE_OUTPUT );

	// pull down for 3 ms for a smooth and nice wake up 
	gpio_set_level( DHT22_PIN, 0 );
	ets_delay_us( 3000 );			

	// pull up for 25 us for a gentile asking for data
	gpio_set_level( DHT22_PIN, 1 );
	ets_delay_us( 25 );

	gpio_set_direction( DHT22_PIN, GPIO_MODE_INPUT );		// change to input mode
  
	// == DHT will keep the line low for 80 us and then high for 80us ====

	uSec = getSignalLevel( 85, 0 );

	if( uSec<0 ) return ESP_ERR_TIMEOUT; 

	// -- 80us up ------------------------

	uSec = getSignalLevel( 85, 1 );

	if( uSec<0 ) return ESP_ERR_TIMEOUT;

	// == No errors, read the 40 data bits ================
  
	for( int k = 0; k < 40; k++ ) 
	{

		// -- starts new data transmission with >50us low signal

		uSec = getSignalLevel( 56, 0 );
		if( uSec<0 ) return ESP_ERR_TIMEOUT;

		// -- check to see if after >70us rx data is a 0 or a 1

		uSec = getSignalLevel( 75, 1 );
		if( uSec<0 ) return ESP_ERR_TIMEOUT;

		// add the current read to the output data
		// since all dhtData array where set to 0 at the start, 
		// only look for "1" (>28us us)
	
		if (uSec > 40) {
			dhtData[ byteInx ] |= (1 << bitInx);
			}
	
		// index to next byte

		if (bitInx == 0) { bitInx = 7; ++byteInx; }
		else bitInx--;
	}

	// == get humidity from Data[0] and Data[1] ==========================

	humidity_air = dhtData[0];
	humidity_air *= 0x100;					// >> 8
	humidity_air += dhtData[1];
	humidity_air /= 10;						// get the decimal

	// == get temp from Data[2] and Data[3]
	
	temperature_air = dhtData[2] & 0x7F;	
	temperature_air *= 0x100;				// >> 8
	temperature_air += dhtData[3];
	temperature_air /= 10;

	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
		temperature_air *= -1;


	// == verify if checksum is ok ===========================================
	// Checksum is the sum of Data 8 bits masked out 0xFF
	
	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF)) 
		return ESP_OK;

	else 
		return ESP_FAIL;
}

// ------------------------------------------------------------------------ my_main function
void my_main()
{
	nvs_read_parametrs();
	print_parametrs();
	check_setting_mode();
	if(check_parametrs() == false) error_func("Check parametrs, error\n", 1);
	camera_init();

	aws_net_init();
	esp_ota_mark_app_valid_cancel_rollback();
	aws_mqtt_connect();

	if(type_device == TYPE_DEVICE_USER)
	{
		gpio_init_type_user();
	}
	else if(type_device == TYPE_DEVICE_GENERAL)
	{
		gpio_init_type_general();
	}

	while(1)
	{
		// read sensor and control pins
		if((prvGetTimeMs() - timer_300ms) >= 300)
		{
			if(type_device == TYPE_DEVICE_USER)
			{
				SoilMoisture = adc1_get_raw(SOIL_MOISTURE_SENSOR_PIN);
				if(SoilMoisture > 2500) SoilMoisture = 2500;
				else if(SoilMoisture < 1360) SoilMoisture = 1360;
				SoilMoisture = map(SoilMoisture, 1360, 2500, 0, 100);
				SoilMoisture = 100 - SoilMoisture;
				if(abs(SoilMoisture - old_SoilMoisture) >= 4)
				{
					flag_change_sensor_values = true;
					old_SoilMoisture = SoilMoisture;
				}

				if(time_vw == 0)
				{
					gpio_set_level(FluidValv1_PIN, FluidValv1_state);
				}
				else
				{
					if(FluidValv1_state == 1)
					{
						gpio_set_level(FluidValv1_PIN, 1);
						if(((prvGetTimeMs() - timer_vw) / 1000) >= time_vw)
						{
							FluidValv1_state = 0;
							gpio_set_level(FluidValv1_PIN, FluidValv1_state);
							flag_change_sensor_values = true;
						}
					}
					else
					{
						gpio_set_level(FluidValv1_PIN, 0);
					}
				}

				if(time_vn == 0)
				{
					gpio_set_level(FluidValv2_PIN, FluidValv2_state);
				}
				else
				{
					if(FluidValv2_state == 1)
					{
						gpio_set_level(FluidValv2_PIN, 1);
						if(((prvGetTimeMs() - timer_vn) / 1000) >= time_vn)
						{
							FluidValv2_state = 0;
							gpio_set_level(FluidValv2_PIN, FluidValv2_state);
							flag_change_sensor_values = true;
						}
					}
					else
					{
						gpio_set_level(FluidValv2_PIN, 0);
					}
				}

				toogle_led_incicator();
				printf("FluidValv1:%d , FluidValv2:%d , SoilMoisture:%d %\n", FluidValv1_state, FluidValv2_state, SoilMoisture);
			}
			else if(type_device == TYPE_DEVICE_GENERAL)
			{
				illumination_value = adc1_get_raw(LIGHT_SENSOR_PIN);
				illumination_value = map(illumination_value, 0, 4095, 0, 100);
				illumination_value = 100 - illumination_value;
				if(abs(illumination_value - old_illumination_value) >= 4)
				{
					flag_change_sensor_values = true;
					old_illumination_value = illumination_value;
				}

				readDHT();
				if(abs(temperature_air - old_temperature_air) >= 2.0)
				{
					flag_change_sensor_values = true;
					old_temperature_air = temperature_air;
				}

				if(abs(humidity_air - old_humidity_air) >= 5.0)
				{
					flag_change_sensor_values = true;
					old_humidity_air = humidity_air;
				}

				printf("Air temp: %.2f , Air humidity: %.2f , Lighting: %d\n", temperature_air, humidity_air, illumination_value);
			}
			timer_300ms = prvGetTimeMs();
		}

		// pub info
		if((prvGetTimeMs() - timer_mqtt_pub) >= AWS_MQTT_TIMER_VALUE_UPDATE_INFO || aws_flag_accepted_comand == true || flag_change_sensor_values == true)
		{
			if(type_device == TYPE_DEVICE_USER)
			{
				aws_mqtt_create_json_for_type_user();
			}
			else if(type_device == TYPE_DEVICE_GENERAL)
			{
				aws_mqtt_create_json_for_type_general();
			}
			MyPubInfo.pPayload = &buffer_json_tx;
            MyPubInfo.payloadLength = strlen(buffer_json_tx);
            if(MQTT_Publish( &GlobalMqttContext, &MyPubInfo, packetId_PUB ) != MQTTSuccess)
            {
		    	restart_device("Mqtt, pub error\n", 30000);
		    }
		    else
		    {
		    	printf("Mqtt, pub ok\n");
		    }
		    MQTT_ProcessLoop( &GlobalMqttContext, 100 );

            flag_change_sensor_values = false;

            if(type_device == TYPE_DEVICE_USER)
			{
	            if(aws_image_req_comand == 1)
	            {
	            	MQTT_Disconnect(&GlobalMqttContext);
					SecureSocketsTransport_Disconnect(&MQTTNetworkContext);
					send_photo_https();
					aws_mqtt_connect();
					aws_image_req_comand = 0;
	            }
        	}

            if(aws_update_firmware_comand == 1)
            {
            	MQTT_Disconnect(&GlobalMqttContext);
				SecureSocketsTransport_Disconnect(&MQTTNetworkContext);
				https_get_firmware(https_get_firmware_URL, https_firmware_len);
				aws_mqtt_connect();
            	aws_update_firmware_comand = 0;
            }

			timer_mqtt_pub = prvGetTimeMs();
		}

		// ping broker
		if((prvGetTimeMs() - timer_ping_broker) >= AWS_MQTT_TIMER_VALUE_PING_BROKER)
		{
			if(MQTT_Ping(&GlobalMqttContext) != MQTTSuccess)
			{
		    	restart_device("Mqtt, ping error\n", 30000);
		    }
		    else
		    {
		    	printf("Mqtt, ping ok\n");
		    }
			timer_ping_broker = prvGetTimeMs();
		}

		// check wifi connect state
		if(WIFI_IsConnected(NULL) == false)
		{
			restart_device("Wifi, reconnect, after 30 sec restart\n", 30000);
		}

		MQTT_ProcessLoop( &GlobalMqttContext, 0 );
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}