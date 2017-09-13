/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define STR_TRACE_USER_TA "SWUPDATE"

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "optee_key.h"

#include <string.h>
#include <stdlib.h>

static uint8_t filename[] = "swupdate-public.pem";


/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("has been called");
	return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed if the TA has not
 * crashed or panicked. This is the last call in the TA.
 */
void TA_DestroyEntryPoint(void)
{
	DMSG("has been called");
}

/*
 * Called when a new session is opened to the TA. *sess_ctx can be updated
 * with a value to be able to identify this session in subsequent calls to the
 * TA. In this function you will normally do the global initialization for the
 * TA.
 */
TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;
	(void)&sess_ctx;

	/*
	 * The DMSG() macro is non-standard, TEE Internal API doesn't
	 * specify any means to logging from a TA.
	 */
	DMSG("Hello. You are in Secure World!\n");

	/* If return value != TEE_SUCCESS the session will not be created. */
	return TEE_SUCCESS;
}

/*
 * Called when a session is closed, sess_ctx hold the value that was
 * assigned by TA_OpenSessionEntryPoint().
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx; /* Unused parameter */
	DMSG("Goodbye!\n");
}

static inline uint32_t tee_time_to_ms(TEE_Time t)
{
	return t.seconds * 1000 + t.millis;
}

static inline uint32_t get_delta_time_in_ms(TEE_Time start, TEE_Time stop)
{
	return tee_time_to_ms(stop) - tee_time_to_ms(start);
}

static TEE_Result prepare_file_to_write(size_t data_size, uint8_t *chunk_buf,
				size_t chunk_size)
{
	size_t remain_bytes = data_size;
	TEE_Result res = TEE_SUCCESS;
	TEE_ObjectHandle object;

	res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
			filename, sizeof(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE |
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			NULL, NULL, 0, &object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to create persistent object, res=0x%08x",
				res);
		goto exit;
	}

	while (remain_bytes) {
		size_t write_size;

		if (remain_bytes < chunk_size)
			write_size = remain_bytes;
		else
			write_size = chunk_size;
		res = TEE_WriteObjectData(object, chunk_buf, write_size);
		if (res != TEE_SUCCESS) {
			EMSG("Failed to write data, res=0x%08x", res);
			goto exit_close_object;
		}
		remain_bytes -= write_size;
	}
exit_close_object:
	TEE_CloseObject(object);
exit:
	return res;
}


static TEE_Result write_file_secure(TEE_ObjectHandle object, size_t data_size,
		uint8_t *chunk_buf, size_t chunk_size,
		uint32_t *spent_time_in_ms)
{
	TEE_Time start_time, stop_time;
	size_t remain_bytes = data_size;
	TEE_Result res = TEE_SUCCESS;

	TEE_GetSystemTime(&start_time);

	while (remain_bytes) {
		size_t write_size;

		DMSG("Write data, remain bytes: %zu", remain_bytes);
		if (chunk_size > remain_bytes)
			write_size = remain_bytes;
		else
			write_size = chunk_size;
		res = TEE_WriteObjectData(object, chunk_buf, write_size);
		if (res != TEE_SUCCESS) {
			EMSG("Failed to write data, res=0x%08x", res);
			goto exit;
		}
		remain_bytes -= write_size;
	}

	TEE_GetSystemTime(&stop_time);

	*spent_time_in_ms = get_delta_time_in_ms(start_time, stop_time);

	IMSG("start: %u.%u(s), stop: %u.%u(s), delta: %u(ms)",
			start_time.seconds, start_time.millis,
			stop_time.seconds, stop_time.millis,
			*spent_time_in_ms);

exit:
	return res;
}

static TEE_Result read_file_secure(TEE_ObjectHandle object, size_t data_size,
		uint8_t *chunk_buf, size_t chunk_size,
		uint32_t *spent_time_in_ms)
{
	TEE_Time start_time, stop_time;
	size_t remain_bytes = data_size;
	TEE_Result res = TEE_SUCCESS;
	uint32_t read_bytes = 0;

	TEE_GetSystemTime(&start_time);

	while (remain_bytes) {
		size_t read_size;

		DMSG("Read data, remain bytes: %zu", remain_bytes);
		if (remain_bytes < chunk_size)
			read_size = remain_bytes;
		else
			read_size = chunk_size;
		res = TEE_ReadObjectData(object, chunk_buf, read_size,
				&read_bytes);
		if (res != TEE_SUCCESS) {
			EMSG("Failed to read data, res=0x%08x", res);
			goto exit;
		}

		remain_bytes -= read_size;
	}

	TEE_GetSystemTime(&stop_time);

	*spent_time_in_ms = get_delta_time_in_ms(start_time, stop_time);

	IMSG("start: %u.%u(s), stop: %u.%u(s), delta: %u(ms)",
			start_time.seconds, start_time.millis,
			stop_time.seconds, stop_time.millis,
			*spent_time_in_ms);

exit:
	return res;
}

static TEE_Result write_key(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
               TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	TEE_Result res;
	TEE_ObjectHandle object = TEE_HANDLE_NULL;
	uint8_t *chunk_buf;
	uint32_t *spent_time_in_ms = &params[2].value.a;

	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	DMSG("[TA] GOT IN TA BUFFER : %s", (char *)params[0].memref.buffer);


	chunk_buf = TEE_Malloc(DEFAULT_CHUNK_SIZE, TEE_MALLOC_FILL_ZERO);
	if (!chunk_buf) {
		EMSG("Failed to allocate memory");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

	TEE_MemMove(chunk_buf, (uint8_t *)params[0].memref.buffer, DEFAULT_DATA_SIZE);

	DMSG("message write chunk is : %s", chunk_buf);

	res = prepare_file_to_write(DEFAULT_DATA_SIZE, chunk_buf, DEFAULT_CHUNK_SIZE);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to create test file, res=0x%08x",
				res);
		goto exit_free_chunk_buf;
	}

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
			filename, sizeof(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE |
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			&object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to open persistent object, res=0x%08x",
				res);
		goto exit_remove_object;
	}

	res = write_file_secure(object, DEFAULT_DATA_SIZE, chunk_buf,
			DEFAULT_CHUNK_SIZE, spent_time_in_ms);

exit_remove_object:
	TEE_CloseObject(object);
exit_free_chunk_buf:
	TEE_Free(chunk_buf);
exit:
	return res;
}

static TEE_Result read_key(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
			          TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	TEE_Result res;
	TEE_ObjectHandle object = TEE_HANDLE_NULL;
	uint8_t *chunk_buf;
	uint32_t *spent_time_in_ms = &params[2].value.a;
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	DMSG("Start to read test storage");

	chunk_buf = TEE_Malloc(DEFAULT_CHUNK_SIZE, TEE_MALLOC_FILL_ZERO);
	if (!chunk_buf) {
		EMSG("Failed to allocate memory");
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto exit;
	}

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
			filename, sizeof(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE |
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			&object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to open persistent object, res=0x%08x",
				res);
		goto exit_remove_object;
	}
	res = read_file_secure(object, DEFAULT_DATA_SIZE, chunk_buf,
			DEFAULT_CHUNK_SIZE, spent_time_in_ms);

	DMSG("message read from secure file is : %s", chunk_buf);

	TEE_MemMove(params[0].memref.buffer, chunk_buf, DEFAULT_DATA_SIZE);
	params[0].memref.size = DEFAULT_DATA_SIZE;
	DMSG("[TA] SENDING TO HOST : %s", (char *)params[0].memref.buffer);
 
exit_remove_object:
	TEE_CloseObject(object);
	TEE_Free(chunk_buf);
 	return TEE_SUCCESS; 
exit:
	return res;
}

TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx; /* Unused parameter */
	DMSG("**************Invoke command EntryPoint");
	switch (cmd_id) {
	case TA_OPTEE_KEY_CMD_GET_KEY:
		return read_key(param_types, params);
 	case TA_OPTEE_KEY_CMD_WRITE_KEY:
		return write_key(param_types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
