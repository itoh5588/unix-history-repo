/*
 * Copyright (c) 1997 - 1999 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "kadm5_locl.h"

RCSID("$Id: get_princs_c.c,v 1.3 1999/12/02 17:05:06 joda Exp $");

kadm5_ret_t
kadm5_c_get_principals(void *server_handle, 
		       const char *exp,
		       char ***princs, 
		       int *count)
{
    kadm5_client_context *context = server_handle;
    kadm5_ret_t ret;
    krb5_storage *sp;
    unsigned char buf[1024];
    int32_t tmp;
    krb5_data reply;

    sp = krb5_storage_from_mem(buf, sizeof(buf));
    if (sp == NULL)
	return ENOMEM;
    krb5_store_int32(sp, kadm_get_princs);
    krb5_store_int32(sp, exp != NULL);
    if(exp)
	krb5_store_string(sp, exp);
    ret = _kadm5_client_send(context, sp);
    krb5_storage_free(sp);
    ret = _kadm5_client_recv(context, &reply);
    if(ret)
	return ret;
    sp = krb5_storage_from_data (&reply);
    if (sp == NULL) {
	krb5_data_free (&reply);
	return ENOMEM;
    }
    krb5_ret_int32(sp, &tmp);
    ret = tmp;
    if(ret == 0) {
	int i;
	krb5_ret_int32(sp, &tmp);
	*princs = calloc(tmp + 1, sizeof(**princs));
	if (*princs == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	for(i = 0; i < tmp; i++)
	    krb5_ret_string(sp, &(*princs)[i]);
	*count = tmp;
    }
out:
    krb5_storage_free(sp);
    krb5_data_free (&reply);
    return ret;
}
