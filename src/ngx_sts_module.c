/***************************************************************************
 *
 * Copyright (C) 2018-2019 - ZmartZone Holding BV - www.zmartzone.eu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @Author: Hans Zandbelt - hans.zandbelt@zmartzone.eu
 *
 **************************************************************************/

#include <stdlib.h>

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_config.h>

#include <oauth2/cfg.h>
#include <oauth2/mem.h>
#include <oauth2/version.h>

#include "oauth2/nginx.h"
#include "oauth2/sts.h"

typedef struct ngx_sts_config {
	oauth2_sts_cfg_t *cfg;
	ngx_http_complex_value_t source_token;
	ngx_str_t target_token;
} ngx_sts_config;

#define NGINX_STS_FUNC_ARGS(nargs, primitive)                                  \
	OAUTH2_NGINX_CFG_FUNC_ARGS##nargs(ngx_sts_config, cfg, sts_cfg,        \
					  primitive)

NGINX_STS_FUNC_ARGS(1, type)
NGINX_STS_FUNC_ARGS(1, ssl_validation)
NGINX_STS_FUNC_ARGS(1, http_timeout)
NGINX_STS_FUNC_ARGS(1, wstrust_endpoint)
NGINX_STS_FUNC_ARGS(2, wstrust_endpoint_auth)
NGINX_STS_FUNC_ARGS(1, wstrust_applies_to)
NGINX_STS_FUNC_ARGS(1, wstrust_token_type)
NGINX_STS_FUNC_ARGS(1, wstrust_value_type)
NGINX_STS_FUNC_ARGS(1, ropc_endpoint)
NGINX_STS_FUNC_ARGS(2, ropc_endpoint_auth)
NGINX_STS_FUNC_ARGS(1, ropc_client_id)
NGINX_STS_FUNC_ARGS(1, ropc_username)
NGINX_STS_FUNC_ARGS(1, otx_endpoint)
NGINX_STS_FUNC_ARGS(2, otx_endpoint_auth)
NGINX_STS_FUNC_ARGS(1, otx_client_id)
NGINX_STS_FUNC_ARGS(1, cache_expiry_s)
NGINX_STS_FUNC_ARGS(2, cache)
NGINX_STS_FUNC_ARGS(2, request_parameters)

static ngx_int_t ngx_sts_target_token_request_variable(
    ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
	ngx_sts_config *cfg = (ngx_sts_config *)data;

	ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
		       "sts request variable");

	v->len = cfg->target_token.len;
	v->data = cfg->target_token.data;

	if (v->len) {
		v->valid = 1;
		v->no_cacheable = 0;
		v->not_found = 0;
	} else {
		v->not_found = 1;
	}

	return NGX_OK;
}

static char *ngx_sts_variables_command(ngx_conf_t *cf, ngx_command_t *cmd,
				       void *conf)
{
	char *rv = NGX_CONF_ERROR;
	// ngx_http_core_loc_conf_t *clcf = NULL;
	ngx_sts_config *cfg = (ngx_sts_config *)conf;
	ngx_http_compile_complex_value_t ccv;
	ngx_str_t *value = NULL;
	ngx_http_variable_t *v;

	//	clcf = ngx_http_conf_get_module_loc_conf(cf,
	// ngx_http_core_module); 	if ((clcf == NULL) || (cfg == NULL)) {
	// rv = "internal error: ngx_http_core_loc_conf_t or "
	//"ngx_sts_config is null..."; 		goto end;
	//	}
	//	clcf->handler = ngx_sts_handler;

	value = cf->args->elts;
	ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
	ccv.cf = cf;
	ccv.value = &value[1];
	ccv.complex_value = &cfg->source_token;

	if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
		rv = "ngx_http_compile_complex_value failed";
		goto end;
	}

	if (value[2].data[0] != '$') {
		ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
				   "invalid variable name \"%V\"", &value[2]);
		goto end;
	}

	value[2].len--;
	value[2].data++;

	v = ngx_http_add_variable(cf, &value[2], 0);
	if (v == NULL) {
		rv = "ngx_http_add_variable failed";
		goto end;
	}

	v->get_handler = ngx_sts_target_token_request_variable;
	v->data = (uintptr_t)cfg;

	rv = NGX_CONF_OK;

end:

	return rv;
}

#define NGINX_STS_CMD_TAKE(nargs, primitive, member)                           \
	OAUTH2_NGINX_CMD_TAKE##nargs(sts_cfg, primitive, member)

// clang-format off
static ngx_command_t ngx_sts_commands[] = {
	NGINX_STS_CMD_TAKE( 1, STSType, type),
	NGINX_STS_CMD_TAKE( 1, STSSSLValidateServer, ssl_validation),
	NGINX_STS_CMD_TAKE( 1, STSHTTPTimeOut, http_timeout),
	NGINX_STS_CMD_TAKE( 1, STSWSTrustEndpoint, wstrust_endpoint),
	NGINX_STS_CMD_TAKE(12, STSWSTrustEndpointAuth, wstrust_endpoint_auth),
	NGINX_STS_CMD_TAKE( 1, STSWSTrustAppliesTo, wstrust_applies_to),
	NGINX_STS_CMD_TAKE( 1, STSWSTrustTokenType, wstrust_token_type),
	NGINX_STS_CMD_TAKE( 1, STSWSTrustValueType, wstrust_value_type),
	NGINX_STS_CMD_TAKE( 1, STSROPCEndpoint, ropc_endpoint),
	NGINX_STS_CMD_TAKE(12, STSROPCEndpointAuth, ropc_endpoint_auth),
	NGINX_STS_CMD_TAKE( 1, STSROPCClientID, ropc_client_id),
	NGINX_STS_CMD_TAKE( 1, STSROPCUsername, ropc_username),
	NGINX_STS_CMD_TAKE( 1, STSOTXEndpoint, otx_endpoint),
	NGINX_STS_CMD_TAKE(12, STSOTXEndpointAuth, otx_endpoint_auth),
	NGINX_STS_CMD_TAKE( 1, STSOTXClientID, otx_client_id),
	NGINX_STS_CMD_TAKE( 1, STSCacheExpiresIn, cache_expiry_s),
	NGINX_STS_CMD_TAKE(12, STSCache, cache),
	NGINX_STS_CMD_TAKE(12, STSRequestParameter, request_parameters),
	{
			// TODO: do this nicer...
		ngx_string("STSVariables"),
		NGX_HTTP_LOC_CONF | NGX_CONF_TAKE2,
		ngx_sts_variables_command,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL
	},
	ngx_null_command
};
// clang-format on

static void ngx_sts_cleanup(void *data)
{
	ngx_sts_config *conf = (ngx_sts_config *)data;
	oauth2_sts_cfg_free(NULL, conf->cfg);
}

static void *ngx_sts_create_loc_conf(ngx_conf_t *cf)
{
	ngx_sts_config *conf = NULL;
	ngx_pool_cleanup_t *cln = NULL;

	char path[255];

	conf = ngx_pnalloc(cf->pool, sizeof(ngx_sts_config));

	// TODO: path?
	sprintf(path, "%p", conf);

	//	oauth2_log_sink_t *log_sink_nginx =
	// oauth2_mem_alloc(sizeof(oauth2_log_sink_t));
	// log_sink_nginx->callback = oauth2_log_nginx;
	//	//	// TODO: get the log level from NGINX...
	//	log_sink_nginx->level = LMO_LOG_TRACE1;
	//	log_sink_nginx->ctx = cf->log;
	//	oauth2_log_t *log = oauth2_log_init(log_sink_nginx->level,
	// log_sink_nginx);
	oauth2_log_t *log = oauth2_log_init(OAUTH2_LOG_TRACE1, NULL);

	conf->cfg = oauth2_sts_cfg_create(log, path);

	cln = ngx_pool_cleanup_add(cf->pool, 0);
	if (cln == NULL)
		goto end;

	cln->handler = ngx_sts_cleanup;
	cln->data = conf;

	// ngx_memzero(&conf->source_token, sizeof(ngx_http_complex_value_t));
	// ngx_memzero(&conf->target_token, sizeof(ngx_http_complex_value_t));

	// fprintf(stderr, " ## ngx_sts_create_loc_conf: %p (log=%p)\n", conf,
	//	cf->log);

	// ngx_log_error_core(NGX_LOG_NOTICE, cf->log, 0, "# %s: %s",
	// "ngx_sts_create_loc_conf: %p", conf);

end:

	return conf;
}

static char *ngx_sts_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_sts_config *prev = parent;
	ngx_sts_config *conf = child;

	oauth2_sts_cfg_merge(NULL, conf->cfg, prev->cfg, conf->cfg);

	// TODO: merge conf->source_token and conf->target_token?
	// ngx_memzero(&conf->source_token, sizeof(ngx_http_complex_value_t));
	// ngx_memzero(&conf->target_token, sizeof(ngx_http_complex_value_t));

	// ngx_log_error_core(NGX_LOG_NOTICE, cf->log, 0, "# %s: %s",
	// "ngx_sts_merge_loc_conf: %p->%p", prev, conf);

	// fprintf(stderr, " ## ngx_sts_merge_loc_conf: %p->%p (log=%p)\n",
	// prev, 	conf, cf->log);

	return NGX_CONF_OK;
}

static ngx_int_t ngx_sts_post_config(ngx_conf_t *cf);

// clang-format off
static ngx_http_module_t ngx_sts_module_ctx = {
		NULL,						/* preconfiguration              */
		ngx_sts_post_config,		/* postconfiguration             */

		NULL,						/* create main configuration     */
		NULL,						/* init main configuration       */

		NULL,						/* create server configuration   */
		NULL,						/* merge server configuration    */

		ngx_sts_create_loc_conf,	/* create location configuration */
		ngx_sts_merge_loc_conf		/* merge location configuration  */
};

ngx_module_t ngx_sts_module = {
		NGX_MODULE_V1,
		&ngx_sts_module_ctx,	/* module context    */
		ngx_sts_commands,		/* module directives */
		NGX_HTTP_MODULE,		/* module type       */
		NULL,					/* init master       */
		NULL,					/* init module       */
		NULL,					/* init process      */
		NULL,					/* init thread       */
		NULL,					/* exit thread       */
		NULL,					/* exit process      */
		NULL,					/* exit master       */
		NGX_MODULE_V1_PADDING
};
// clang-format on

static ngx_int_t ngx_sts_handler(ngx_http_request_t *r);

static ngx_int_t ngx_sts_post_config(ngx_conf_t *cf)
{
	ngx_int_t rv = NGX_ERROR;
	ngx_http_handler_pt *h = NULL;
	ngx_http_core_main_conf_t *cmcf = NULL;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
	if (h == NULL)
		goto end;

	*h = ngx_sts_handler;

	rv = NGX_OK;

end:

	return rv;
}

static ngx_int_t ngx_sts_handler(ngx_http_request_t *r)
{
	ngx_int_t rv = NGX_DECLINED;
	bool rc = false;
	oauth2_nginx_request_context_t *ctx = NULL;
	ngx_sts_config *cfg = NULL;
	ngx_str_t ngx_source_token;
	char *source_token = NULL, *target_token = NULL;

	if (r != r->main)
		goto end;

	cfg = (ngx_sts_config *)ngx_http_get_module_loc_conf(r, ngx_sts_module);
	if (cfg == NULL) {
		oauth2_warn(ctx->log,
			    "ngx_http_get_module_loc_conf returned NULL");
		goto end;
	}

	ctx = oauth2_nginx_request_context_init(r);
	if (ctx == NULL) {
		oauth2_warn(ctx->log,
			    "oauth2_nginx_request_context_init returned NULL");
		goto end;
	}

	if (sts_cfg_get_type(cfg->cfg) == STS_TYPE_DISABLED) {
		oauth2_debug(ctx->log, "disabled");
		goto end;
	}

	if (ngx_http_complex_value(r, &cfg->source_token, &ngx_source_token) !=
	    NGX_OK) {
		oauth2_warn(
		    ctx->log,
		    "ngx_http_complex_value failed to obtain source_token");
		goto end;
	}

	if (ngx_source_token.len == 0) {
		oauth2_warn(ctx->log,
			    "ngx_http_complex_value ngx_source_token.len=0");
		goto end;
	}

	source_token = oauth2_strndup((const char *)ngx_source_token.data,
				      ngx_source_token.len);

	oauth2_debug(ctx->log, "enter: source_token=%s, initial_request=%d",
		     source_token, (r != r->main));

	rc = sts_handler(ctx->log, cfg->cfg, source_token, &target_token);

	oauth2_debug(ctx->log, "target_token=%s (rc=%d)", target_token, rc);

	if (target_token == NULL)
		goto end;

	cfg->target_token.len = strlen(target_token);
	cfg->target_token.data = ngx_palloc(r->pool, cfg->target_token.len);
	ngx_memcpy(cfg->target_token.data, (unsigned char *)target_token,
		   cfg->target_token.len);

	rv = rc ? NGX_OK : NGX_ERROR;

end:

	if (source_token)
		oauth2_mem_free(source_token);
	if (target_token)
		oauth2_mem_free(target_token);

	// hereafter we destroy the log object...
	oauth2_debug(ctx->log, "leave: %d", rv);

	if (ctx)
		oauth2_nginx_request_context_free(ctx);

	return rv;
}
