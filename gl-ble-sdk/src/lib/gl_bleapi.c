/*****************************************************************************
 * @file  libglbleapi.c
 * @brief Shared library for API interface
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
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <errno.h>

#include "gl_bleapi.h"
#include "gl_dev_mgr.h"
#include "gl_log.h"
#include "gl_common.h"
#include "gl_hal.h"
#include "gl_methods.h"
#include "gl_thread.h"
#include "silabs_msg.h"
#include "silabs_evt.h"

gl_ble_cbs ble_msg_cb;

/************************************************************************************************************************************/

void *ble_driver_thread_ctx = NULL;
void *ble_watcher_thread_ctx = NULL;

static int *msqid = NULL;
static driver_param_t *_driver_param = NULL;
static watcher_param_t *_watcher_param = NULL;

/************************************************************************************************************************************/
GL_RET gl_ble_init(void)
{
	// err return if ble driver thread exist
	if ((NULL != _driver_param) || (NULL != ble_driver_thread_ctx))
	{
		return GL_ERR_INVOKE;
	}

	// init work thread param
	_driver_param = (driver_param_t *)malloc(sizeof(driver_param_t));

	// create an event message queue if it not exist
	if (NULL == msqid)
	{
		msqid = (int *)malloc(sizeof(int));
		*msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
		if (*msqid == -1)
		{
			log_err("create msg queue error!!!\n");
			return GL_UNKNOW_ERR;
		}
	}
	_driver_param->evt_msgid = *msqid;

	/* Init device manage */
	ble_dev_mgr_init();

	// init hal
	hal_init();

	// create a thread to recv module message
	int ret;
	ret = HAL_ThreadCreate(&ble_driver_thread_ctx, ble_driver, _driver_param, NULL, NULL);
	if (ret != 0)
	{
		log_err("pthread_create ble_driver_thread_ctx failed!\n");
		// free driver_param_t & driver ctx
		free(_driver_param);
		_driver_param = NULL;
		ble_driver_thread_ctx = NULL;

		// close hal fd
		hal_destroy();

		// destroy device list
		ble_dev_mgr_destroy();
		return GL_UNKNOW_ERR;
	}

	// reset ble module to make sure it is a usable mode
	gl_ble_hard_reset();

	return GL_SUCCESS;
}

GL_RET gl_ble_destroy(void)
{
	// close msg thread
	HAL_ThreadDelete(ble_driver_thread_ctx);
	ble_driver_thread_ctx = NULL;

	// free driver_param_t
	free(_driver_param);
	_driver_param = NULL;

	// close hal fd
	hal_destroy();

	// destroy device list
	ble_dev_mgr_destroy();

	// destroy evt msg queue
	if (-1 == msgctl(*msqid, IPC_RMID, NULL))
	{
		log_err("msgctl error");
		return GL_UNKNOW_ERR;
	}
	free(msqid);
	msqid = NULL;

	return GL_SUCCESS;
}

GL_RET gl_ble_subscribe(gl_ble_cbs *callback)
{
	if (NULL == callback)
	{
		return GL_ERR_PARAM;
	}

	// error return if watcher thread exist
	if ((NULL != _watcher_param) || (NULL != ble_watcher_thread_ctx))
	{
		return GL_ERR_INVOKE;
	}

	_watcher_param = (watcher_param_t *)malloc(sizeof(watcher_param_t));

	// create an event message queue if it not exist
	if (NULL == msqid)
	{
		msqid = (int *)malloc(sizeof(int));
		*msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
		if (*msqid == -1)
		{
			log_err("create msg queue error!!!\n");
			return GL_UNKNOW_ERR;
		}
	}
	_watcher_param->evt_msgid = *msqid;
	_watcher_param->cbs = callback;

	int ret;
	ret = HAL_ThreadCreate(&ble_watcher_thread_ctx, ble_watcher, _watcher_param, NULL, NULL);
	if (ret != 0)
	{
		log_err("pthread_create failed!\n");
		// free watcher_param_t
		free(_watcher_param);
		_watcher_param = NULL;
		return GL_UNKNOW_ERR;
	}

	return GL_SUCCESS;
}

GL_RET gl_ble_unsubscribe(void)
{
	HAL_ThreadDelete(ble_watcher_thread_ctx);
	ble_watcher_thread_ctx = NULL;

	// free watcher_param_t
	free(_watcher_param);
	_watcher_param = NULL;

	return GL_SUCCESS;
}

GL_RET gl_ble_enable(int32_t enable)
{
	return ble_enable(enable);
}

GL_RET gl_ble_hard_reset(void)
{
	return ble_hard_reset();
}

GL_RET gl_ble_get_mac(BLE_MAC mac)
{
	return ble_local_mac(mac);
}

GL_RET gl_ble_set_power(int power, int *current_power)
{
	return ble_set_power(power, current_power);
}

GL_RET gl_ble_adv_data(int flag, char *data)
{
	return ble_adv_data(flag, data);
}

GL_RET gl_ble_adv(int phys, int interval_min, int interval_max, int discover, int adv_conn)
{
	return ble_adv(phys, interval_min, interval_max, discover, adv_conn);
}

GL_RET gl_ble_stop_adv(void)
{
	return ble_stop_adv();
}

GL_RET gl_ble_send_notify(BLE_MAC address, int char_handle, char *value)
{
	return ble_send_notify(address, char_handle, value);
}

GL_RET gl_ble_discovery(int phys, int interval, int window, int type, int mode)
{
	return ble_discovery(phys, interval, window, type, mode);
}

GL_RET gl_ble_stop_discovery(void)
{
	return ble_stop_discovery();
}

GL_RET gl_ble_connect(BLE_MAC address, int address_type, int phy)
{
	return ble_connect(address, address_type, phy);
}

GL_RET gl_ble_disconnect(BLE_MAC address)
{
	return ble_disconnect(address);
}

GL_RET gl_ble_get_rssi(BLE_MAC address, int32_t *rssi)
{
	return ble_get_rssi(address, rssi);
}

GL_RET gl_ble_get_service(gl_ble_service_list_t *service_list, BLE_MAC address)
{
	return ble_get_service(service_list, address);
}

GL_RET gl_ble_get_char(gl_ble_char_list_t *char_list, BLE_MAC address, int service_handle)
{
	return ble_get_char(char_list, address, service_handle);
}

GL_RET gl_ble_read_char(BLE_MAC address, int char_handle)
{
	return ble_read_char(address, char_handle);
}

GL_RET gl_ble_write_char(BLE_MAC address, int char_handle, char *value, int res)
{
	return ble_write_char(address, char_handle, value, res);
}

GL_RET gl_ble_set_notify(BLE_MAC address, int char_handle, int flag)
{
	return ble_set_notify(address, char_handle, flag);
}

GL_RET gl_ble_sw_reset(uint8_t mode)
{
	return ble_sw_reset(mode);
}

GL_RET gl_ble_dfu_uart_flash_upload(uint8_t *file_path)
{
	return ble_dfu_uart_flash_upload(file_path);
}
