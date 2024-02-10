/*
 * FreeRTOS V1.4.7
 * by Muhan and Ruslan
 USER CONTROLLER SOFTWARE
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 
MicroSD card    ESP32
    CLK     GPIO 14
    CMD     GPIO 15
    DATA0   GPIO 2
    DATA1 / flashlight  GPIO 4
    DATA2   GPIO 12
    DATA3   GPIO 13

D0          GPIO 5  Y2_GPIO_NUM
D1          GPIO 18 Y3_GPIO_NUM
D2          GPIO 19 Y4_GPIO_NUM
D3          GPIO 21 Y5_GPIO_NUM
D4          GPIO 36 Y6_GPIO_NUM
D5          GPIO 39 Y7_GPIO_NUM
D6          GPIO 34 Y8_GPIO_NUM
D7          GPIO 35 Y9_GPIO_NUM
XCLK        GPIO 0  XCLK_GPIO_NUM
PCLK        GPIO 22 PCLK_GPIO_NUM
VSYNC       GPIO 25 VSYNC_GPIO_NUM
HREF        GPIO 23 HREF_GPIO_NUM
SDA         GPIO 26 SIOD_GPIO_NUM
SCL         GPIO 27 SIOC_GPIO_NUM
POWER PIN   GPIO 32 PWDN_GPIO_NUM

GPIO 33 ADC 1 5 is FREE to use

ADD CAMERA CODE HERE
*/

#include "iot_config.h"

/* FreeRTOS includes. */

#include "FreeRTOS.h"
#include "task.h"

/* Demo includes */
#include "aws_demo.h"
#include "aws_dev_mode_key_provisioning.h"

/* AWS System includes. */
#include "bt_hal_manager.h"
#include "iot_system_init.h"
#include "iot_logging_task.h"

#include "nvs_flash.h"
#if !AFR_ESP_LWIP
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#endif

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_interface.h"
#include "esp_bt.h"
#if CONFIG_NIMBLE_ENABLED == 1
    #include "esp_nimble_hci.h"
#else
    #include "esp_gap_ble_api.h"
    #include "esp_bt_main.h"
#endif

#include "driver/uart.h"
#include "aws_application_version.h"
#include "tcpip_adapter.h"

#include "iot_network_manager_private.h"

#include "iot_uart.h"

#if BLE_ENABLED
    #include "bt_hal_manager_adapter_ble.h"
    #include "bt_hal_manager.h"
    #include "bt_hal_gatt_server.h"

    #include "iot_ble.h"
    #include "iot_ble_config.h"
    #include "iot_ble_wifi_provisioning.h"
    #include "iot_ble_numericComparison.h"
#endif

// ***************************************************************************************
#include "transport_secure_sockets.h"
#include "core_mqtt.h"
#include "core_mqtt_state.h"
#include "backoff_algorithm.h"
#include "aws_clientcredential.h"
#include "iot_default_root_certificates.h"
//#include "cJSON.h"
#include "C:\AFR\FreeRTOS\vendors\espressif\esp-idf\components\json\cJSON\cJSON.h"
#include "driver/adc.h"
#include "math.h"
//#include "dht.h"
// ***************************************************************************************


/* Logging Task Defines. */
#define mainLOGGING_MESSAGE_QUEUE_LENGTH    ( 32 )
#define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 4 )
#define mainDEVICE_NICK_NAME                "Espressif_Demo"

#define AWS_MQTT_TIMER_VALUE_PING_BROKER    45000U
/* Static arrays for FreeRTOS+TCP stack initialization for Ethernet network connections
 * are use are below. If you are using an Ethernet connection on your MCU device it is
 * recommended to use the FreeRTOS+TCP stack. The default values are defined in
 * FreeRTOSConfig.h. */

/**
 * @brief Initializes the board.
 */
static void prvMiscInitialization( void );

#if BLE_ENABLED
/* Initializes bluetooth */
    static esp_err_t prvBLEStackInit( void );
    /** Helper function to teardown BLE stack. **/
    esp_err_t xBLEStackTeardown( void );
#endif

IotUARTHandle_t xConsoleUart;


static void iot_uart_init( void )
{
    IotUARTConfig_t xUartConfig;
    int32_t status = IOT_UART_SUCCESS;
    
    xConsoleUart = iot_uart_open( UART_NUM_0 );
    configASSERT( xConsoleUart );
    
    status = iot_uart_ioctl( xConsoleUart, eUartGetConfig, &xUartConfig );
    configASSERT( status == IOT_UART_SUCCESS );
    
    xUartConfig.ulBaudrate = 115200;
    xUartConfig.xParity = eUartParityNone;
    xUartConfig.xStopbits = eUartStopBitsOne;
    xUartConfig.ucFlowControl = true;

    status = iot_uart_ioctl( xConsoleUart, eUartSetConfig, &xUartConfig );
    configASSERT( status == IOT_UART_SUCCESS );
}
/*-----------------------------------------------------------*/

/**
 * @brief Application runtime entry point.
 */


// ************************************************************************************************
int test_num = 0;
int sh = 0;
int sh_buff = 0;

int vw1_state = 0;
int vw2_state = 0;

char user[] = "USER";


uint32_t timer_ping_broker = 0;

bool flag_accepted_comand = false;
static char buffer_json_tx[ 4096 ] = { 0 };
static char buffer_json_rx[ 4096 ] = { 0 };

#define MILLISECONDS_PER_SECOND                           ( 1000U )
#define MILLISECONDS_PER_TICK                             ( MILLISECONDS_PER_SECOND / configTICK_RATE_HZ )

static uint32_t mqttConnectedNetwork = AWSIOT_NETWORK_TYPE_NONE;
static NetworkContext_t xNetworkContext;
static MQTTContext_t globalMqttContext;
static uint32_t ulGlobalEntryTimeMs;
static uint8_t pcNetworkBuffer[ 1024U ];

#define mqttexamplePENDING_ACKS_MAX_SIZE             10
#define mqttexampleSUBSCRIPTIONS_MAX_COUNT           4
#define mqttexamplePUBLISH_COUNT                     8
#define mqttexampleDEMO_BUFFER_SIZE                  50
#define mqttexampleDYNAMIC_BUFFER_SIZE               25
#define mqttexampleCOMMAND_QUEUE_SIZE                12
#define mqttexamplePUBLISH_QUEUE_SIZE                10
#define mqttexampleSUBSCRIBE_TASK_DELAY_MS           400U
#define mqttexamplePUBLISH_DELAY_SYNC_MS             100U
#define mqttexamplePUBLISH_DELAY_ASYNC_MS            100U
#define mqttexamplePUBLISHER_SYNC_COMPLETE_BIT       ( 1U << 1 )
#define mqttexamplePUBLISHER_ASYNC_COMPLETE_BIT      ( 1U << 2 )
#define mqttexampleSUBSCRIBE_TASK_COMPLETE_BIT       ( 1U << 3 )
#define mqttexampleSUBSCRIBE_COMPLETE_BIT            ( 1U << 0 )
#define mqttexampleUNSUBSCRIBE_COMPLETE_BIT          ( 1U << 1 )
#define mqttexampleTASK_STACK_SIZE                   ( configMINIMAL_STACK_SIZE * 4 )
#define mqttexampleMAX_WAIT_ITERATIONS               ( 20 )
#define mqttexampleDEMO_TICKS_TO_WAIT                pdMS_TO_TICKS( 1000 )

// ====================================== end prototype
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

static void aws_mqtt_parse_msg()
{
  cJSON *json_processing = cJSON_Parse(buffer_json_rx);
  if(json_processing != NULL)
  {
    cJSON *state = cJSON_GetObjectItem(json_processing, "state");
    cJSON *desired = cJSON_GetObjectItem(state, "desired");

    if(desired != NULL)
    {

      cJSON *statevwObj = cJSON_GetObjectItem(desired, "vw");
      if(statevwObj != NULL)
      {
        vw1_state = statevwObj->valueint;
        if(vw1_state > 1) vw1_state = 1;
        else if(vw1_state < 0) vw1_state = 0;
        statevwObj = NULL;
      }


      cJSON *statevnObj = cJSON_GetObjectItem(desired, "vn");
      if(statevnObj != NULL)
      {
        vw2_state = statevnObj->valueint;
        if(vw2_state > 1) vw2_state = 1;
        else if(vw2_state < 0) vw2_state = 0;
        statevnObj = NULL;
      }
    }
   
    cJSON_Delete(json_processing);
    flag_accepted_comand = true;
  }
}



static void prvEventCallback( MQTTContext_t * pMqttContext, MQTTPacketInfo_t * pPacketInfo, MQTTDeserializedInfo_t * pDeserializedInfo )
{
    char rec_topic[128] = { 0 };
    printf("\r\n rec packet type:");
    printf("%d\r\n", pPacketInfo->type);


    if(pPacketInfo->type == 0x30)
    {
        for(int i = 0; i < pPacketInfo->remainingLength; i++)
        {
            if(pPacketInfo->pRemainingData[0] == '{') break;
            pPacketInfo->pRemainingData++;
        }
        memset(buffer_json_rx, 0, sizeof(buffer_json_rx));
        memcpy(buffer_json_rx, pPacketInfo->pRemainingData, strlen((char *)pPacketInfo->pRemainingData));
        //printf("\r\n rec msg:");
        //iot_uart_write_async(xConsoleUart, buffer_json_rx, strlen((char *)buffer_json_rx));
        aws_mqtt_parse_msg();
    }
}

/*-----------------------------------------------------------*/

void net_sock_mqtt_init_and_connect()
{
    IotSdk_Init();
    AwsIotNetworkManager_Init();
    AwsIotNetworkManager_EnableNetwork(AWSIOT_NETWORK_TYPE_WIFI);

    const IotNetworkInterface_t * pNetworkInterface = NULL;
    void * pConnectionParams = NULL, * pCredentials = NULL;
    int status;

    pNetworkInterface = AwsIotNetworkManager_GetNetworkInterface( mqttConnectedNetwork );
    pConnectionParams = AwsIotNetworkManager_GetConnectionParams( mqttConnectedNetwork );
    pCredentials = AwsIotNetworkManager_GetCredentials( mqttConnectedNetwork );

    // ------------- socket connect
    ServerInfo_t xServerInfo = { 0 };
    SocketsConfig_t xSocketConfig = { 0 };

    xServerInfo.pHostName = clientcredentialMQTT_BROKER_ENDPOINT;
    xServerInfo.hostNameLength = sizeof( clientcredentialMQTT_BROKER_ENDPOINT ) - 1U;
    xServerInfo.port = clientcredentialMQTT_BROKER_PORT;

    xSocketConfig.enableTls = true;
    xSocketConfig.disableSni = false;
    xSocketConfig.sendTimeoutMs = 500U;
    xSocketConfig.recvTimeoutMs = 500U;
    xSocketConfig.pRootCa = tlsATS1_ROOT_CERTIFICATE_PEM;
    xSocketConfig.rootCaSize = sizeof( tlsATS1_ROOT_CERTIFICATE_PEM );

    //SecureSocketsTransport_Connect( &xNetworkContext, &xServerInfo, &xSocketConfig );

    if(SecureSocketsTransport_Connect( &xNetworkContext, &xServerInfo, &xSocketConfig ) != TRANSPORT_SOCKET_STATUS_SUCCESS)
    {
        restart_device("MQTT, NET ERROR CONNECT\n", 30000);
    }
    else
    {
        printf("MQTT, NET CONNECT SUCCESS\n");
    }



    // --------------- mqtt init
    TransportInterface_t xTransport;
    MQTTFixedBuffer_t xNetworkBuffer;

    xNetworkBuffer.pBuffer = pcNetworkBuffer;
    xNetworkBuffer.size = 1024U;

    xTransport.pNetworkContext = &xNetworkContext;
    xTransport.send = SecureSocketsTransport_Send;
    xTransport.recv = SecureSocketsTransport_Recv;

    //MQTT_Init( &globalMqttContext, &xTransport, prvGetTimeMs, prvEventCallback, &xNetworkBuffer );


    if(MQTT_Init (&globalMqttContext, &xTransport, prvGetTimeMs, prvEventCallback, &xNetworkBuffer) != MQTTSuccess)
    {
        restart_device("MQTT Init ERROR!!!\n", 30000);   
    }
    else
    {
        printf("MQTT Init SUCCESS\n");
    }


    // ---------------- mqtt connect
    MQTTConnectInfo_t xConnectInfo;
    bool xSessionPresent = false;
    memset( &xConnectInfo, 0x00, sizeof( xConnectInfo ) );
    xConnectInfo.cleanSession = true;
    xConnectInfo.pClientIdentifier = clientcredentialIOT_THING_NAME;
    xConnectInfo.clientIdentifierLength = ( uint16_t ) strlen( clientcredentialIOT_THING_NAME );
    xConnectInfo.keepAliveSeconds = 60U;
    //MQTT_Connect( &globalMqttContext, &xConnectInfo, NULL, 1000U, &xSessionPresent );

     if(MQTT_Connect(&globalMqttContext, &xConnectInfo, NULL, 1000U, &xSessionPresent ) != MQTTSuccess)
    {
        restart_device("MQTT, CONNECT ERROR\n", 30000);
    }
    else
    {
        printf("MQTT Connect SUCCESS\n");
    }


}

void create_json_servers_aws()
{
  cJSON *json_obj_main;
  cJSON *json_obj_state;
  cJSON *json_obj_reported;
  cJSON *json_obj_desired;
  json_obj_state = cJSON_CreateObject();
  json_obj_reported = cJSON_CreateObject();
  json_obj_main = cJSON_CreateObject();
  json_obj_desired = cJSON_CreateObject();
  if(flag_accepted_comand == true)
  {
    json_obj_desired = cJSON_CreateNull();
    flag_accepted_comand = false;
  }
            //cJSON_AddNumberToObject(json_obj_reported, "type", user);
            cJSON_AddStringToObject(json_obj_reported, "type", "USER");
            //cJSON_AddNumberToObject(json_obj_reported, "sh", sh);             // SHADOW REPORT STATES IS HERE BOI!!!!!!!
            cJSON_AddNumberToObject(json_obj_reported, "vw", vw1_state);  
            cJSON_AddNumberToObject(json_obj_reported, "vn", vw2_state);



  cJSON_AddItemToObject(json_obj_state, "reported", json_obj_reported);
  cJSON_AddItemToObject(json_obj_state, "desired", json_obj_desired);
  cJSON_AddItemToObject(json_obj_main, "state", json_obj_state);
  char *msg_json = cJSON_Print(json_obj_main);
  memset(buffer_json_tx, 0, sizeof(buffer_json_tx));
  memcpy(buffer_json_tx, msg_json, strlen(msg_json));
  cJSON_Delete(json_obj_main);
}

// *************************************************************************************************************


int app_main( void )
{
    /* Perform any hardware initialization that does not require the RTOS to be
     * running.  */

    prvMiscInitialization();

    if( SYSTEM_Init() == pdPASS )
    {
        /* A simple example to demonstrate key and certificate provisioning in
         * microcontroller flash using PKCS#11 interface. This should be replaced
         * by production ready key provisioning mechanism. */
        vDevModeKeyProvisioning();

        gpio_pad_select_gpio(GPIO_NUM_2);
        gpio_pad_select_gpio(GPIO_NUM_2);
        gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

        gpio_pad_select_gpio(GPIO_NUM_4);
        gpio_pad_select_gpio(GPIO_NUM_4);
        gpio_set_direction(GPIO_NUM_4, GPIO_MODE_OUTPUT);


        //adc1_config_width(ADC_WIDTH_BIT_12);
        //adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_6);


        net_sock_mqtt_init_and_connect();

        MQTTStatus_t ret_stat;

        size_t my_subscriptionCount = 1;
        MQTTSubscribeInfo_t my_subs_list;
        my_subs_list.qos = MQTTQoS0;
        my_subs_list.pTopicFilter = "$aws/things/"clientcredentialIOT_THING_NAME"/shadow/update/accepted";
        my_subs_list.topicFilterLength = strlen("$aws/things/"clientcredentialIOT_THING_NAME"/shadow/update/accepted");
        uint16_t packetId_SUB = MQTT_PACKET_ID_INVALID;
        packetId_SUB = MQTT_GetPacketId( &globalMqttContext );
        ret_stat = MQTT_Subscribe( &globalMqttContext, &my_subs_list, my_subscriptionCount, packetId_SUB);
        
        // restart here*********************************
        if(ret_stat == MQTTSuccess)
        {
            printf("MQTT SUBSCRIBTION SUCCESS\r\n");
        }
        else{
            restart_device("MQTT SUBSCRIBTION ERROR\n", 30000);
        }
        //**********************************************


        MQTTPublishInfo_t PublishInfo1 = { 0 };
        PublishInfo1.qos = MQTTQoS0;
        PublishInfo1.retain = false;
        PublishInfo1.dup = false;
        PublishInfo1.pTopicName = "$aws/things/"clientcredentialIOT_THING_NAME"/shadow/update"; // topic name actually
        PublishInfo1.topicNameLength = strlen("$aws/things/"clientcredentialIOT_THING_NAME"/shadow/update"); //topic for subsribe enter Device name
        uint16_t packetId_PUB = MQTT_PACKET_ID_INVALID;

        MQTT_Publish( &globalMqttContext, &PublishInfo1, packetId_PUB );  // PuBlish once
        
        while(1)   ////////////////////////////////////////// LOGIC ///////////////////////////////////////////////////////////////////////////////
        {
            //test_num++;   // TEST NUM CHANGE TO SOIL HUMIDITY
            //sh = adc1_get_raw(ADC1_CHANNEL_7);
            //printf("sh = %d\n\n\n", sh);
            // ТОГДА ПОСТИТЬц

            //watchdog
            //esp_task_wdt_init(8, true); //initialize Watchdog 8 mins, panic
            //esp_task_wdt_add(NULL); 


            create_json_servers_aws();
            PublishInfo1.pPayload = &buffer_json_tx;
            PublishInfo1.payloadLength = strlen(buffer_json_tx);




            //if (abs(sh - sh_buff) > 2)
           // if (sh != sh_buff)
            //{ 
            //    MQTT_Publish( &globalMqttContext, &PublishInfo1, packetId_PUB );
          //  }
          //  sh_buff = sh;
            
            MQTT_ProcessLoop( &globalMqttContext, 0U );

            gpio_set_level(GPIO_NUM_2, vw1_state);
            gpio_set_level(GPIO_NUM_4, vw2_state);
            vTaskDelay(pdMS_TO_TICKS(30000));

            if((prvGetTimeMs() - timer_ping_broker) >= AWS_MQTT_TIMER_VALUE_PING_BROKER)
        {
            if(MQTT_Ping(&globalMqttContext) != MQTTSuccess)
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
        }

    }

    /* Start the scheduler.  Initialization that requires the OS to be running,
     * including the WiFi initialization, is performed in the RTOS daemon task
     * startup hook. */
    /* Following is taken care by initialization code in ESP IDF */
    /* vTaskStartScheduler(); */
    return 0;
}


//Restart Func********************

void restart_device(char* info_restart, int delay_value)
{
    printf(info_restart);
    vTaskDelay(pdMS_TO_TICKS(delay_value));
    esp_restart();
}
//********************************


/*-----------------------------------------------------------*/
extern void vApplicationIPInit( void );
static void prvMiscInitialization( void )
{
    int32_t uartRet;
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();

    if( ( ret == ESP_ERR_NVS_NO_FREE_PAGES ) || ( ret == ESP_ERR_NVS_NEW_VERSION_FOUND ) )
    {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );

    iot_uart_init();

    #if BLE_ENABLED
        NumericComparisonInit();
    #endif

    /* Create tasks that are not dependent on the WiFi being initialized. */
    xLoggingTaskInitialize( mainLOGGING_TASK_STACK_SIZE,
                            tskIDLE_PRIORITY + 5,
                            mainLOGGING_MESSAGE_QUEUE_LENGTH );

#if AFR_ESP_LWIP
    configPRINTF( ("Initializing lwIP TCP stack\r\n") );
    tcpip_adapter_init();
#else
    configPRINTF( ("Initializing FreeRTOS TCP stack\r\n") );
    vApplicationIPInit();
#endif
}

/*-----------------------------------------------------------*/

#if BLE_ENABLED

    #if CONFIG_NIMBLE_ENABLED == 1
        esp_err_t prvBLEStackInit( void )
        {
            return ESP_OK;
        }


        esp_err_t xBLEStackTeardown( void )
        {
            esp_err_t xRet;

            xRet = esp_bt_controller_mem_release( ESP_BT_MODE_BLE );

            return xRet;
        }

    #else /* if CONFIG_NIMBLE_ENABLED == 1 */

        static esp_err_t prvBLEStackInit( void )
        {
            return ESP_OK;
        }

        esp_err_t xBLEStackTeardown( void )
        {
            esp_err_t xRet = ESP_OK;

            if( esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED )
            {
                xRet = esp_bluedroid_disable();
            }

            if( xRet == ESP_OK )
            {
                xRet = esp_bluedroid_deinit();
            }

            if( xRet == ESP_OK )
            {
                if( esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED )
                {
                    xRet = esp_bt_controller_disable();
                }
            }

            if( xRet == ESP_OK )
            {
                xRet = esp_bt_controller_deinit();
            }

            if( xRet == ESP_OK )
            {
                xRet = esp_bt_controller_mem_release( ESP_BT_MODE_BLE );
            }

            if( xRet == ESP_OK )
            {
                xRet = esp_bt_controller_mem_release( ESP_BT_MODE_BTDM );
            }

            return xRet;
        }
    #endif /* if CONFIG_NIMBLE_ENABLED == 1 */
#endif /* if BLE_ENABLED */

/*-----------------------------------------------------------*/


#if BLE_ENABLED
/*-----------------------------------------------------------*/

    static void prvUartCallback( IotUARTOperationStatus_t xStatus,
                                      void * pvUserContext )
    {
        SemaphoreHandle_t xUartSem = ( SemaphoreHandle_t ) pvUserContext;
        configASSERT( xUartSem != NULL );
        xSemaphoreGive( xUartSem );
    }

  
    BaseType_t getUserMessage( INPUTMessage_t * pxINPUTmessage,
                               TickType_t xAuthTimeout )
    {
        BaseType_t xReturnMessage = pdFALSE;
        SemaphoreHandle_t xUartSem;
        int32_t status, bytesRead = 0;
        uint8_t *pucResponse;

        xUartSem = xSemaphoreCreateBinary();

        
        /* BLE Numeric comparison response is one character (y/n). */
        pucResponse = ( uint8_t * ) pvPortMalloc( sizeof( uint8_t ) );

        if( ( xUartSem != NULL ) && ( pucResponse != NULL ) )
        {
            iot_uart_set_callback( xConsoleUart, prvUartCallback, xUartSem );

            status = iot_uart_read_async( xConsoleUart, pucResponse, 1 );

            /* Wait for  auth timeout to get the input character. */
            xSemaphoreTake( xUartSem, xAuthTimeout );

            /* Cancel the uart operation if the character is received or timeout occured. */
            iot_uart_cancel( xConsoleUart );

            /* Reset the callback. */
            iot_uart_set_callback( xConsoleUart, NULL, NULL );

            iot_uart_ioctl( xConsoleUart, eGetRxNoOfbytes, &bytesRead );

            if( bytesRead == 1 )
            {
                pxINPUTmessage->pcData = pucResponse;
                pxINPUTmessage->xDataSize = 1;
                xReturnMessage = pdTRUE;
            }

            vSemaphoreDelete( xUartSem );
        }

        return xReturnMessage;
    }
#endif /* if BLE_ENABLED */

/*-----------------------------------------------------------*/

extern void esp_vApplicationTickHook();
void IRAM_ATTR vApplicationTickHook()
{
    esp_vApplicationTickHook();
}

/*-----------------------------------------------------------*/
extern void esp_vApplicationIdleHook();
void vApplicationIdleHook()
{
    esp_vApplicationIdleHook();
}

/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook( void )
{
}

#if !AFR_ESP_LWIP
/*-----------------------------------------------------------*/
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
    uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    system_event_t evt;

    if( eNetworkEvent == eNetworkUp )
    {
        /* Print out the network configuration, which may have come from a DHCP
         * server. */
        FreeRTOS_GetAddressConfiguration(
            &ulIPAddress,
            &ulNetMask,
            &ulGatewayAddress,
            &ulDNSServerAddress );

        evt.event_id = SYSTEM_EVENT_STA_GOT_IP;
        evt.event_info.got_ip.ip_changed = true;
        evt.event_info.got_ip.ip_info.ip.addr = ulIPAddress;
        evt.event_info.got_ip.ip_info.netmask.addr = ulNetMask;
        evt.event_info.got_ip.ip_info.gw.addr = ulGatewayAddress;
        esp_event_send( &evt );
    }
}
#endif
