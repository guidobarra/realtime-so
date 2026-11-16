#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <string.h>
#include <math.h>
#include "esp_check.h"
#include "driver/dac_continuous.h"
#include "driver/dac_cosine.h"

#define UART_NUM UART_NUM_0
#define UART_BAUD_RATE 115200
#define UART_TX_PIN 17 // Pin TX
#define UART_RX_PIN 16 // Pin RX
#define BUF_SIZE (1024)

#define BUTTON_GPIO_22 22 // pulsador
#define BUTTON_GPIO_23 23 // pulsador
#define BUTTON_GPIO_34 34 // pulsador

#define TAM_ARRAY 400

static const char *TAG = "UART";

typedef struct
{
    uint8_t type;
    uint8_t ampl;
    uint16_t freq;
    uint16_t phase;
} signal_config;

signal_config signal;
uint8_t wave[TAM_ARRAY];

QueueHandle_t queue;

// Function to initialize UART
void uart_init()
{
    const uart_config_t uartConfig = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM, &uartConfig);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE, BUF_SIZE, 0, NULL, 0);
}

// Task to detect button press
void button_task(void *pvParameters)
{
    uint8_t state = 0;
    uint32_t pinNumber = (uint32_t)pvParameters;
    while (1)
    {
        vTaskDelay(20 / portTICK_PERIOD_MS);
        if (state == 0 && gpio_get_level(pinNumber))
        {
            state = 1;
            xQueueSend(queue, &pinNumber, 0);
            // ESP_LOGI(TAG, "Button pressed. Sent to queue.");
        }
        if (state == 1 && !gpio_get_level(pinNumber))
        {
            state = 0;
        }
    }
}

// Task to send data via UART when an event is detected
void uart_send_task(void *pvParameters)
{
    uint32_t pinNumber;
    const char *msgForward = "adelante";
    const char *msgEnter   = "enter";
    const char *msgBack    = "atras";

    while (1)
    {
        // Check if there are elements in the queue
        uint8_t count = uxQueueMessagesWaiting(queue);
        if (count > 0)
        {
            if (xQueueReceive(queue, &pinNumber, 0) == pdPASS)
            { // 0, do not block if queue is empty
                // ESP_LOGI(TAG, "Event received: %d", pinNumber);
                if (pinNumber == BUTTON_GPIO_22)
                {
                    uart_write_bytes(UART_NUM, msgForward, strlen(msgForward));
                    // ESP_LOGI(TAG, "Message sent via uart: %s", msgForward);
                }
                else if (pinNumber == BUTTON_GPIO_23)
                {
                    uart_write_bytes(UART_NUM, msgEnter, strlen(msgEnter));
                    // ESP_LOGI(TAG, "Message sent via uart: %s", msgEnter);
                }
                else if (pinNumber == BUTTON_GPIO_34)
                {
                    uart_write_bytes(UART_NUM, msgBack, strlen(msgBack));
                    // ESP_LOGI(TAG, "Message sent via uart: %s", msgBack);
                }
            }
        }
        else
        {
            // ESP_LOGI(TAG, "Empty queue, waiting for events...");
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

dac_continuous_handle_t dac_continuous_handle;
dac_continuous_config_t dacConfig = {
    .chan_mask = DAC_CHANNEL_MASK_ALL,
    .desc_num = 8,
    .buf_size = TAM_ARRAY,
    .freq_hz = TAM_ARRAY * 1000,
    .offset = 0,
    .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
    .chan_mode = DAC_CHANNEL_MODE_SIMUL,
};

// Generates digital samples for four wave types
void generate_waveform(uint8_t signalType, uint8_t amplitudeVolts, uint16_t phase)
{
    uint32_t size = TAM_ARRAY;
    uint8_t amplitude = (uint8_t)((amplitudeVolts / 3.3) * 255); // Convert amplitude to 8-bit value
    for (int i = 0; i < size; i++)
    {
        switch (signalType)
        {
        case 0: // Sine wave
            wave[i] = (uint8_t)((sin(i * 2 * M_PI / size + (phase / sizeof(uint16_t)) * 2 * M_PI / 360) + 1) * (double)(amplitude) / 2 + 0.5);
            break;
        case 1: // Square wave
            wave[i] = (i < size / 2) ? amplitude : 0;
            break;
        case 2: // Sawtooth wave
            wave[i] = (i * amplitude) / size;
            break;
        case 3: // Triangular wave
            wave[i] = (i < size / 2) ? (2 * amplitude * i / size) : (2 * amplitude * (size - i) / size);
            break;
        default: // Default square wave
            wave[i] = (i < size / 2) ? amplitude : 0;
            break;
        }
    }
}

void update_dac_config()
{
    generate_waveform(signal.type, signal.ampl, signal.phase);
    dacConfig.freq_hz = signal.freq * TAM_ARRAY;

    // Reinitialize DAC with new configuration
    if (dac_continuous_disable(dac_continuous_handle) != ESP_OK)
        ESP_LOGW(TAG, "Could not disable DAC");

    if (dac_continuous_del_channels(dac_continuous_handle) != ESP_OK)
        ESP_LOGW(TAG, "Could not release DAC channel");

    esp_err_t status = dac_continuous_new_channels(&dacConfig, &dac_continuous_handle);
    if (status == ESP_OK)
    {
        status = dac_continuous_enable(dac_continuous_handle);
        if (status != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable DAC: %s", esp_err_to_name(status));
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create DAC channel: %s", esp_err_to_name(status));
    }
}

static void uart_receive_task(void *pvParameters)
{
    char rxBuffer[BUF_SIZE] = {0};

    while (1)
    {
        int bytesRead = uart_read_bytes(UART_NUM, rxBuffer, BUF_SIZE - 1, 100 / portTICK_PERIOD_MS);
        if (bytesRead > 0)
        {
            rxBuffer[bytesRead] = '\0'; // Ensure valid string

            ESP_LOGI(TAG, "Message received: %s", rxBuffer);

            int freq = 0;
            int amp = 0;
            int signalType = 0;

            char *token = strtok(rxBuffer, ",");
            // Extract first value (frequency)
            if (token != NULL)
            {
                freq = atoi(token); // Convert string to integer
                ESP_LOGI(TAG, "Frequency: %d", freq);
            }

            // Extract second value (amplitude)
            token = strtok(NULL, ","); // Get next token after comma
            if (token != NULL)
            {
                amp = atoi(token);
                ESP_LOGI(TAG, "Amplitude: %d", amp);
            }

            token = strtok(NULL, ",");
            if (token != NULL)
            {
                signalType = atoi(token);
                ESP_LOGI(TAG, "Signal type: %d", signalType);
            }

            // Validate values before updating DAC
            // freq=0 causes DAC driver failure; amp and type must be coherent
            bool isValid = (freq > 0) && (freq <= 20000) &&
                           (amp > 0) && (amp <= 3) &&
                           (signalType >= 0) && (signalType <= 3);

            if (isValid)
            {
                signal.type  = (uint8_t)signalType;
                signal.freq  = (uint16_t)freq;
                signal.ampl  = (uint8_t)amp;
                signal.phase = 0;

                update_dac_config();
            }
            else
            {
                ESP_LOGW(TAG, "Invalid data ignored: freq=%d amp=%d type=%d", freq, amp, signalType);
            }
        }

        dac_continuous_write_cyclically(dac_continuous_handle, (uint8_t *)wave, TAM_ARRAY, NULL);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    uart_init();

    gpio_reset_pin(BUTTON_GPIO_22);
    gpio_set_direction(BUTTON_GPIO_22, GPIO_MODE_INPUT);

    gpio_reset_pin(BUTTON_GPIO_23);
    gpio_set_direction(BUTTON_GPIO_23, GPIO_MODE_INPUT);

    gpio_reset_pin(BUTTON_GPIO_34);
    gpio_set_direction(BUTTON_GPIO_34, GPIO_MODE_INPUT);

    queue = xQueueCreate(20, sizeof(unsigned int));
    BaseType_t xReturned1, xReturned2, xReturned3;
    TaskHandle_t xHandle1, xHandle2, xHandle3;
    xReturned1 = xTaskCreate(button_task, "DetectButtonPin22", 4096, (void *)BUTTON_GPIO_22, 15, &xHandle1);
    xReturned2 = xTaskCreate(button_task, "DetectButtonPin23", 4096, (void *)BUTTON_GPIO_23, 15, &xHandle2);
    xReturned3 = xTaskCreate(button_task, "DetectButtonPin34", 4096, (void *)BUTTON_GPIO_34, 15, &xHandle3);

    xTaskCreate(uart_send_task, "UartSendTask", 4096, NULL, 10, NULL);

    if (xReturned1 != pdPASS)
    {
        ESP_LOGI(TAG, "ERROR! Could not create task");
        while (1)
            ;
    }

    if (xReturned2 != pdPASS)
    {
        ESP_LOGI(TAG, "ERROR! Could not create task");
        while (1)
            ;
    }

    // Generate initial signal
    generate_waveform(1, 1, 0);
    dac_continuous_new_channels(&dacConfig, &dac_continuous_handle);
    dac_continuous_enable(dac_continuous_handle);

    xTaskCreate(uart_receive_task, "UartReceiveTask", 4096, NULL, 5, NULL);
}
