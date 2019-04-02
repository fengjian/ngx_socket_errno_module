#include <nginx.h>
#include <ngx_http.h>
#include <ngx_core.h>



#define NGX_HTTP_UNKNOWN_ERROR             520
#define NGX_HTTP_HOST_IS_DOWN              521
#define NGX_HTTP_CONNECTION_TIMED_OUT      522
#define NGX_HTTP_CONNECTION_RESET_BY_PEER  523
#define NGX_HTTP_CONNECTION_REFUSED        524
#define NGX_HTTP_HOST_UNREACH              525


typedef struct {
	ngx_chain_t         *free;
	ngx_chain_t         *busy;
} ngx_http_socket_errno_ctx_t;

typedef struct {
	ngx_flag_t  enabled;
} ngx_http_socket_errno_loc_conf_t;

typedef struct {
	ngx_flag_t   enabled;
	u_char      *enable_file;
	ngx_uint_t   enable_line;
} ngx_http_socket_errno_main_conf_t;


static ngx_int_t ngx_http_replace_filter_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_replace_header_filter(ngx_http_request_t *r);
static ngx_int_t ngx_http_replace_body_filter(ngx_http_request_t *r, ngx_chain_t *in);
static void* ngx_http_socket_errno_create_main_conf(ngx_conf_t *cf);
static char* ngx_http_socket_errno_init_main_conf(ngx_conf_t *cf, void *conf);
static void* ngx_http_socket_errno_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_socket_errno_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char* ngx_http_socket_errno_enable(ngx_conf_t *cf, void *data, void *conf);


static ngx_conf_post_t  ngx_http_socket_errno_post_enable = {ngx_http_socket_errno_enable};

static ngx_command_t  ngx_http_socket_errno_commands[] = {
	{
		ngx_string("socket_errno"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LMT_CONF
		|NGX_CONF_TAKE1,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_socket_errno_loc_conf_t, enabled),
		&ngx_http_socket_errno_post_enable
	},

	ngx_null_command
};

static ngx_http_module_t  ngx_http_socket_errno_module_ctx = {
	NULL,
	ngx_http_replace_filter_init,          /* postconfiguration */
	ngx_http_socket_errno_create_main_conf,
	ngx_http_socket_errno_init_main_conf,

	NULL,
	NULL,

	ngx_http_socket_errno_create_loc_conf,
	ngx_http_socket_errno_merge_loc_conf
};

ngx_module_t  ngx_http_socket_errno_module = {
	NGX_MODULE_V1,
	&ngx_http_socket_errno_module_ctx,
	ngx_http_socket_errno_commands,
	NGX_HTTP_MODULE,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

static volatile ngx_cycle_t  *ngx_http_socket_errno_cycle = NULL;


static void* ngx_http_socket_errno_create_main_conf(ngx_conf_t *cf)
{
	ngx_http_socket_errno_main_conf_t  *imcf;

	imcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_socket_errno_main_conf_t));
	if (imcf == NULL) {
		return NULL;
	}

	return imcf;
}

static char* ngx_http_socket_errno_init_main_conf(ngx_conf_t *cf, void *data)
{
	ngx_http_socket_errno_main_conf_t  *imcf = data;
	ngx_pool_cleanup_t                 *cln;

	return NGX_CONF_OK;
}

static void* ngx_http_socket_errno_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_socket_errno_loc_conf_t  *conf;

	conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_socket_errno_loc_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->enabled = NGX_CONF_UNSET;
	return conf;
}


static char* ngx_http_socket_errno_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_socket_errno_loc_conf_t  *prev = parent;
	ngx_http_socket_errno_loc_conf_t  *conf = child;

	ngx_conf_merge_value(conf->enabled, prev->enabled, 0);

	return NGX_CONF_OK;
}

static char* ngx_http_socket_errno_enable(ngx_conf_t *cf, void *data, void *conf)
{
	ngx_flag_t enabled = *((ngx_flag_t *)conf);
	ngx_http_socket_errno_main_conf_t  *imcf;

	if (enabled) {
		imcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_socket_errno_module);
		imcf->enabled = 1;
		imcf->enable_file = cf->conf_file->file.name.data;
		imcf->enable_line = cf->conf_file->line;
	}

	return NGX_CONF_OK;
}

static ngx_int_t ngx_http_replace_filter_init(ngx_conf_t *cf)
{
	ngx_http_socket_errno_main_conf_t  *imcf;
	int                                 multi_http_blocks;

	imcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_socket_errno_module);

	if (ngx_http_socket_errno_cycle != ngx_cycle) {
		ngx_http_socket_errno_cycle = ngx_cycle;
		multi_http_blocks = 0;
	} else {
		multi_http_blocks = 1;
	}

	if (multi_http_blocks || imcf->enabled) {
		ngx_http_next_header_filter = ngx_http_top_header_filter;
		ngx_http_top_header_filter = ngx_http_replace_header_filter;
		ngx_http_next_body_filter = ngx_http_top_body_filter;
		ngx_http_top_body_filter = ngx_http_replace_body_filter;
	}

	return NGX_OK;
}


static ngx_int_t ngx_http_replace_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
	ngx_http_socket_errno_loc_conf_t  *cf;

	cf = ngx_http_get_module_loc_conf(r, ngx_http_socket_errno_module);
	if (cf == NULL || !cf->enabled || in == NULL || r->header_only) {
		return ngx_http_next_body_filter(r, in);
	}

	if (r->err_status >= NGX_HTTP_UNKNOWN_ERROR) {


	}

	return ngx_http_next_body_filter(r, in);
}

static ngx_int_t ngx_http_replace_header_filter(ngx_http_request_t *r)
{
	ngx_http_socket_errno_loc_conf_t  *cf;
	int err = 0;
	ngx_peer_connection_t pc;
	ngx_http_upstream_t *up = r->upstream;


	if (up) {
		pc = up->peer;
		err = pc.socket_errno;
		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "errno %d", err);
	}

	cf = ngx_http_get_module_loc_conf(r, ngx_http_socket_errno_module);
	if (cf == NULL || !cf->enabled) {
		return ngx_http_next_header_filter(r);
	}

	switch (err) {
	case NGX_ETIMEDOUT:
		r->err_status = NGX_HTTP_CONNECTION_TIMED_OUT;
		r->headers_out.status = NGX_HTTP_CONNECTION_TIMED_OUT;
		break;
	case NGX_ECONNREFUSED:
		r->err_status = NGX_HTTP_CONNECTION_REFUSED;
		r->headers_out.status = NGX_HTTP_CONNECTION_REFUSED;
		break;
	case NGX_ECONNRESET:
		r->err_status = NGX_HTTP_CONNECTION_RESET_BY_PEER;
		r->headers_out.status = NGX_HTTP_CONNECTION_RESET_BY_PEER;
		break;
	case NGX_EHOSTDOWN:
		r->err_status = NGX_HTTP_HOST_IS_DOWN;
		r->headers_out.status = NGX_HTTP_HOST_IS_DOWN;
		break;
	case NGX_EHOSTUNREACH:
		r->err_status = NGX_HTTP_HOST_UNREACH;
		r->headers_out.status = NGX_HTTP_HOST_UNREACH;
		break;
	}

	return ngx_http_next_header_filter(r);
}


