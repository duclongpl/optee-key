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

#define STR_TRACE_USER_TA "HELLO_WORLD"

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include "optee_key.h"

#include <string.h>
#include <stdlib.h>
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
	DMSG("Hello World!\n");

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

static TEE_Result inc_value(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
			TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	 char tmp[] = "-----BEGIN PUBLIC KEY-----\n\
              	MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAyIO1L3j9y8FP9Pc7vRZ0\n\
              	zBU5lW67WMMWiIaNjUgWEjG9we9Agdfn9F6Q/REwINWhBrvJDWZTjObLP/SeS6Nj\n\
              	8ejKvK0zfXs72Hab3f3KknF+HRSu2PsXiWrlqSWhPzN907fCA3JhkBrnaA6X1uDK\n\
              	f13n4s6gyIVgzOy4UfLVtc6Tjf9kzgBpFhGcjWERc05ilPwDOM5zIcUr79SJKZTH\n\
              	t1Vug3RQmuesgfoyLaB0DIH7iD+iY1Pqfw3JfeiZ8vT1x1aQU6ArCr81GDfP+kn0\n\
              	+DGegTHJzrrICH4j6NId4xAOOiH7zh94XG9MGEi43BHz94ModIrYYEVErkseVU3A\n\
              	FQIDAQAB\n\
                -----END PUBLIC KEY-----\n";
	void *dst;
	uint32_t siz_of_key ;
	DMSG("has been called %s", tmp );
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;
	DMSG("[TA] GOT IN TA BUFFER : %s", (char *)params[0].memref.buffer);

	siz_of_key = sizeof(tmp);
	DMSG("[TA] Size of key is  : %u", siz_of_key);

	dst = TEE_Malloc(siz_of_key, TEE_MALLOC_FILL_ZERO);

	TEE_MemMove(dst, tmp, siz_of_key);

	TEE_MemMove(params[0].memref.buffer, dst, siz_of_key);
	params[0].memref.size = siz_of_key;
	DMSG("[TA] SENDING TO HOST : %s", (char *)params[0].memref.buffer);

	return TEE_SUCCESS;
}

/*
 * Called when a TA is invoked. sess_ctx hold that value that was
 * assigned by TA_OpenSessionEntryPoint(). The rest of the paramters
 * comes from normal world.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx; /* Unused parameter */
	DMSG("**************Invoke command EntryPoint");
	switch (cmd_id) {
	case TA_OPTEE_KEY_CMD_GET_KEY:
		return inc_value(param_types, params);
#if 0
	case TA_OPTEE_KEY_CMD_GET_KEY_XXX:
		return ...
		break;
#endif
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

