/*****************************************************************************
 * @file
 * @brief Bluetooth driver for silabs EFR32
 *******************************************************************************
 Copyright 2020 GL-iNet. https://www.gl-inet.com/

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sl_bt_api.h"
#include "gl_uart.h"
#include "gl_hal.h"
// #include "bg_types.h"
#include "gl_errno.h"
#include "gl_type.h"
#include "silabs_bleapi.h"
#include "gl_common.h"
#include "silabs_msg.h"
#include "gl_dev_mgr.h"

extern struct sl_bt_packet *evt;
extern bool wait_reset_flag;
extern bool appBooted;

GL_RET silabs_ble_enable(int enable)
{
    if (enable)
    {
        system(rston);
    }
    else
    {
        // wait sub thread recv end
        wait_reset_flag = true;
        // usleep(100*1000);
        while (wait_reset_flag)
        {
            usleep(10 * 1000);
        }
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_hard_reset(void)
{
    int reset_time = 0;
    int wait_s;
    int wait_off;

    // max retry 3 times
    while (reset_time < 3)
    {
        wait_s = 30;    // 3s = 30 * 100ms
        wait_off = 300; // 3s = 300 * 10ms

        wait_reset_flag = true;
        // wait for turn off ble module
        while ((wait_reset_flag) && (wait_off > 0))
        {
            wait_off--;
            usleep(10 * 1000);
        }

        // check turn off end
        if ((appBooted) || (wait_off <= 0))
        {
            // error
            reset_time++;
            continue;
        }

        // wait 300 ms
        usleep(300 * 1000);

        // turn on ble module
        system(rston);

        // wait for ble module start
        while ((!appBooted) && (wait_s > 0))
        {
            wait_s--;
            usleep(100 * 1000);
        }

        // if ble module start success, break loop
        if (appBooted)
        {
            break;
        }

        // ble module start timeout
        reset_time++;
    }

    if (reset_time < 3)
    {
        return GL_SUCCESS;
    }
    else
    {
        return GL_UNKNOW_ERR;
    }
}

GL_RET silabs_ble_local_mac(BLE_MAC mac)
{
    uint8_t type = 0; // Not open to the user layer
    bzero(mac, sizeof(BLE_MAC));

    sl_status_t status = sl_bt_system_get_identity_address((bd_addr *)mac, &type);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_discovery(int phys, int interval, int window, int type, int mode)
{

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_scanner_set_timing((uint8_t)phys, (uint16_t)interval, (uint16_t)window);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    status = SL_STATUS_FAIL;
    status = sl_bt_scanner_set_mode((uint8_t)phys, (uint8_t)type);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    status = SL_STATUS_FAIL;
    status = sl_bt_scanner_start((uint8_t)phys, (uint8_t)mode);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_stop_discovery(void)
{
    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_scanner_stop();
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

uint8_t handle = 0xff;

GL_RET silabs_ble_adv(int phys, int interval_min, int interval_max, int discover, int adv_conn)
{

    sl_status_t status = SL_STATUS_FAIL;

    if (handle == 0xff)
    {
        status = sl_bt_advertiser_create_set(&handle);
        if (status != SL_STATUS_OK)
        {
            return GL_UNKNOW_ERR;
        }
    }

    status = sl_bt_advertiser_set_phy(handle, (uint8_t)phys, (uint8_t)phys);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    status = SL_STATUS_FAIL;
    status = sl_bt_advertiser_set_timing(handle, (uint32_t)interval_min, (uint32_t)interval_max, 0, 0);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    status = SL_STATUS_FAIL;
    status = sl_bt_advertiser_start(handle, (uint8_t)discover, (uint8_t)adv_conn);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_adv_data(int flag, char *data)
{
    if ((!data) || (strlen(data) % 2))
    {
        return GL_ERR_PARAM;
    }

    size_t len = strlen(data) / 2;
    uint8_t *adv_data = (uint8_t *)calloc(len, sizeof(uint8_t));
    str2array(adv_data, data, len);

    sl_status_t status = SL_STATUS_FAIL;

    if (handle == 0xff)
    {
        status = sl_bt_advertiser_create_set(&handle);
        if (status != SL_STATUS_OK)
        {
            return GL_UNKNOW_ERR;
        }
    }

    status = sl_bt_advertiser_set_data(handle, (uint8_t)flag, (size_t)len, (uint8_t *)adv_data);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_stop_adv(void)
{
    sl_status_t status = SL_STATUS_FAIL;

    if (handle != 0xff)
    {
        status = sl_bt_advertiser_stop(handle);
        if (status != SL_STATUS_OK)
        {
            return GL_UNKNOW_ERR;
        }
        handle = 0xff;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_send_notify(BLE_MAC address, int char_handle, char *value)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    if ((!value) || (strlen(value) % 2))
    {
        return GL_ERR_PARAM;
    }

    size_t len = strlen(value) / 2;
    uint8_t *hex_value = (uint8_t *)calloc(len + 1, sizeof(uint8_t));
    str2array(hex_value, value, len);

    sl_status_t status = SL_STATUS_FAIL;
    uint16_t send_len = 0;

    status = sl_bt_gatt_write_characteristic_value_without_response((uint8_t)connection, (uint16_t)char_handle, (size_t)len, (uint8_t *)hex_value, (uint16_t *)&send_len);
    if (status != SL_STATUS_OK)
    {
        free(hex_value);
        return GL_UNKNOW_ERR;
    }
    if (send_len == 0)
    {
        free(hex_value);
        return GL_UNKNOW_ERR;
    }

    free(hex_value);
    return GL_SUCCESS;
}

GL_RET silabs_ble_connect(BLE_MAC address, int address_type, int phy)
{
    if (!address)
    {
        return GL_ERR_PARAM;
    }

    uint8_t connection = 0;

    bd_addr addr;
    memcpy(addr.addr, address, 6);
    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_connection_open(addr, (uint8_t)address_type, (uint8_t)phy, (uint8_t *)&connection);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    ble_dev_mgr_add(address_str, (uint16_t)connection);

    return GL_SUCCESS;
}

GL_RET silabs_ble_disconnect(BLE_MAC address)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_connection_close((uint8_t)connection);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_get_rssi(BLE_MAC address, int32_t *rssi)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_connection_get_rssi((uint8_t)connection);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    uint32_t evt_id = sl_bt_evt_connection_rssi_id;
    if (wait_rsp_evt(evt_id, 300) == 0)
    {
        *rssi = evt->data.evt_connection_rssi.rssi;
    }
    else
    {
        return GL_ERR_EVENT_MISSING;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_get_service(gl_ble_service_list_t *service_list, BLE_MAC address)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_gatt_discover_primary_services((uint8_t)connection);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    int i = 0;
    uint32_t evt_id = sl_bt_evt_gatt_procedure_completed_id;
    if (wait_rsp_evt(evt_id, 600) == 0)
    {
        service_list->list_len = special_evt_num;
        while (i < special_evt_num)
        {
            struct sl_bt_packet *e = &special_evt[i];
            if (SL_BT_MSG_ID(e->header) == sl_bt_evt_gatt_service_id && e->data.evt_gatt_service.connection == connection)
            {
                service_list->list[i].handle = e->data.evt_gatt_service.service;
                reverse_endian(e->data.evt_gatt_service.uuid.data, e->data.evt_gatt_service.uuid.len);
                hex2str(e->data.evt_gatt_service.uuid.data, e->data.evt_gatt_service.uuid.len, service_list->list[i].uuid);
            }
            i++;
        }
    }
    else
    {
        special_evt_num = 0;
        // evt->header = 0;
        return GL_ERR_EVENT_MISSING;
    }

    // clean evt count
    special_evt_num = 0;

    if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_procedure_completed_id)
    {
        evt->header = 0;
        return GL_SUCCESS;
    }
    else
    {
        return GL_ERR_EVENT_MISSING;
    }
}

GL_RET silabs_ble_get_char(gl_ble_char_list_t *char_list, BLE_MAC address, int service_handle)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_gatt_discover_characteristics((uint8_t)connection, (uint32_t)service_handle);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    int i = 0;
    uint32_t evt_id = sl_bt_evt_gatt_procedure_completed_id;
    if (wait_rsp_evt(evt_id, 600) == 0)
    {
        char_list->list_len = special_evt_num;
        while (i < special_evt_num)
        {
            struct sl_bt_packet *e = &special_evt[i];
            if (SL_BT_MSG_ID(e->header) == sl_bt_evt_gatt_characteristic_id && e->data.evt_gatt_characteristic.connection == connection)
            {
                char_list->list[i].handle = e->data.evt_gatt_characteristic.characteristic;
                char_list->list[i].properties = e->data.evt_gatt_characteristic.properties;
                reverse_endian(e->data.evt_gatt_characteristic.uuid.data, e->data.evt_gatt_characteristic.uuid.len);
                hex2str(e->data.evt_gatt_characteristic.uuid.data, e->data.evt_gatt_characteristic.uuid.len, char_list->list[i].uuid);
            }
            i++;
        }
    }
    else
    {
        special_evt_num = 0;
        // evt->header = 0;
        return GL_ERR_EVENT_MISSING;
    }

    // clean evt count
    special_evt_num = 0;

    if (SL_BT_MSG_ID(evt->header) == sl_bt_evt_gatt_procedure_completed_id)
    {
        evt->header = 0;
        return GL_SUCCESS;
    }
    else
    {
        return GL_ERR_EVENT_MISSING;
    }
}

GL_RET silabs_ble_set_power(int power, int *current_power)
{
    sl_status_t status = SL_STATUS_FAIL;
    int16_t set_min = 0, set_max = 0;

    status = sl_bt_system_set_tx_power(0, (int16_t)power, (int16_t *)&set_min, (int16_t *)&set_max);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    *current_power = set_max;
    return GL_SUCCESS;
}

GL_RET silabs_ble_read_char(BLE_MAC address, int char_handle)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_gatt_read_characteristic_value((uint8_t)connection, (uint16_t)char_handle);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_write_char(BLE_MAC address, int char_handle, char *value, int res)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    if ((!value) || (strlen(value) % 2))
    {
        return GL_ERR_PARAM;
    }
    size_t len = strlen(value) / 2;
    unsigned char data[256];
    str2array(data, value, len);

    sl_status_t status = SL_STATUS_FAIL;

    if (res)
    {
        status = sl_bt_gatt_write_characteristic_value((uint8_t)connection, (uint16_t)char_handle, (size_t)len, (uint8_t *)data);
        if (status != SL_STATUS_OK)
        {
            return GL_UNKNOW_ERR;
        }
    }
    else
    {
        uint16_t sent_len = 0;
        status = sl_bt_gatt_write_characteristic_value_without_response((uint8_t)connection, (uint16_t)char_handle, (size_t)len, (uint8_t *)data, (uint16_t *)&sent_len);
        if (status != SL_STATUS_OK)
        {
            return GL_UNKNOW_ERR;
        }
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_set_notify(BLE_MAC address, int char_handle, int flag)
{
    int connection = 0;
    char address_str[BLE_MAC_LEN] = {0};
    addr2str(address, address_str);
    GL_RET ret = ble_dev_mgr_get_connection(address_str, &connection);
    if (ret != GL_SUCCESS)
    {
        return GL_ERR_PARAM;
    }

    sl_status_t status = SL_STATUS_FAIL;

    status = sl_bt_gatt_set_characteristic_notification((uint8_t)connection, (uint16_t)char_handle, (uint8_t)flag);
    if (status != SL_STATUS_OK)
    {
        return GL_UNKNOW_ERR;
    }

    return GL_SUCCESS;
}

GL_RET silabs_ble_sw_reset(uint8_t mode)
{
    sl_bt_system_reset(mode);
    return GL_SUCCESS;
}

GL_RET silabs_ble_dfu_uart_flash_upload(uint8_t *file_path)
{
    if (file_path == NULL)
    {
        return GL_UNKNOW_ERR;
    }

    if (strstr((char *)file_path, ".gbl") == NULL)
    {
        return GL_UNKNOW_ERR;
    }

    sl_bt_system_reset(1);

    sl_status_t status = SL_STATUS_FAIL;

    sleep(1);
    status = sl_bt_dfu_flash_set_address(0);
    if (status != SL_STATUS_OK)
    {
        printf("set_address status - %d \n", status);
        return GL_UNKNOW_ERR;
    }

    sleep(1);
    FILE *fp = NULL;

    fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        printf("not find file\r\n");
        return GL_UNKNOW_ERR;
    }

    uint8_t buffer[1024 * 1024];
    size_t read_size;
    read_size = fread(buffer, 1, 1024 * 1024, fp); /* 每次读取1个字节，最多读取10个，这样返回得read_size才是读到的字节数 */
    printf("file size:%d\n", read_size);
#define MAX_PACKAGE_LEN (128)

    size_t i = 0;
    while (read_size != 0)
    {
        status = SL_STATUS_FAIL;
        if (read_size >= MAX_PACKAGE_LEN)
        {
            status = sl_bt_dfu_flash_upload(128, buffer + i);
            if (status != SL_STATUS_OK)
            {
                printf("dfu_flash_upload status - %d \n", status);
            }
            read_size -= MAX_PACKAGE_LEN;
            i += MAX_PACKAGE_LEN;
        }
        else
        {
            status = sl_bt_dfu_flash_upload(read_size, buffer + i);
            if (status != SL_STATUS_OK)
            {
                printf("dfu_flash_upload status - %d \n", status);
            }
            i += read_size;
            read_size = 0;
        }
        printf("remaining_size:%d,completed_size:%d\n", read_size, i);
        usleep(1000 * 50);
    }
    sleep(2);
    status = sl_bt_dfu_flash_upload_finish();
    printf("flash_upload_finish - %s \n", status == SL_STATUS_OK ? "Succeed" : "failure");
    sleep(2);
    sl_bt_system_reset(0);

    return GL_SUCCESS;
}
