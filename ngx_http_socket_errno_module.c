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
	ngx_uint_t	 last;
	ngx_uint_t	 count;
	ngx_uint_t	 found;
	ngx_uint_t	 index;
	ngx_chain_t	 *free;
	ngx_chain_t	 *busy;
	ngx_chain_t	 *out;
	ngx_chain_t	 *in;
	ngx_chain_t	 **last_out;
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



#define ERR_PAGE_INDEX(x) ((x) - NGX_HTTP_UNKNOWN_ERROR)
static const ngx_str_t err_page[] = {
	ngx_string("<html>"
		   "<head><title>520 Unknown Error</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>Unknown Error 520</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>"),

	ngx_string("<html>"
		   "<head><title>521 Host is Down</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>521 Host is Down</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>"),

	ngx_string("<html>"
		   "<head><title>522 Connection Time Out</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>522 Connection Time Out</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>"),

	ngx_string("<html>"
		   "<head><title>523 Connection Reset By Peer</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>523 Connection Reset By Peer</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>"),

	ngx_string("<html>"
		   "<head><title>524 Connection Refused</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>524 Connection Refused</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>"),

	ngx_string("<html>"
		   "<head><title>525 Host Unreach</title></head>"
		   "<body bgcolor=\"white\">"
		   "<center><h1>525 Host Unreach</h1></center>"
		   "<hr><center>socket errno</center>"
		   "</body></html>")

};


static ngx_int_t ngx_http_replace_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
	ngx_http_socket_errno_loc_conf_t  *cf;
	ngx_http_socket_errno_ctx_t *ctx;
	ngx_chain_t  *cl;
	ngx_buf_t  *b;
	ngx_int_t rc;


	cf = ngx_http_get_module_loc_conf(r, ngx_http_socket_errno_module);
	ctx = ngx_http_get_module_ctx(r, ngx_http_socket_errno_module);

	if (cf == NULL || ctx == NULL || !cf->enabled || in == NULL || r->header_only) {
		return ngx_http_next_body_filter(r, in);
	}


	if (r->err_status >= NGX_HTTP_UNKNOWN_ERROR) {
		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "body errno %d", r->err_status);
		if (ngx_chain_add_copy(r->pool, &ctx->in, in) != NGX_OK) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				      "[ngx_http_socket_errno]: unable to copy"
				      " input chain - in");
			return NGX_ERROR;
		}

		ctx->last_out = &ctx->out;

		cl = ngx_chain_get_free_buf(r->pool, &ctx->free);
		if (cl == NULL) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				      "[ngx_http_socket_errno]: "
				      "unable to allocate output chain");
			return NGX_ERROR;
		}

		b = cl->buf;
		ngx_memzero(b, sizeof(ngx_buf_t));

		b->tag = (ngx_buf_tag_t)&ngx_http_socket_errno_module;
		b->memory = 1;
		b->pos = err_page[ERR_PAGE_INDEX(r->err_status)].data;
		b->last = b->pos + err_page[ERR_PAGE_INDEX(r->err_status)].len;

		if (r == r->main) {
			b->last_buf = 1;
		} else {
			b->last_in_chain = 1;
		}

		ctx->out = cl;
		ctx->out->next = NULL;
		r->keepalive = 0;

		rc = ngx_http_next_body_filter(r, ctx->out);
		ngx_chain_update_chains(r->pool, &ctx->free, &ctx->busy, &ctx->out,
					(ngx_buf_tag_t)&ngx_http_socket_errno_module);
		ctx->in = NULL;
		return rc;
	}


	return ngx_http_next_body_filter(r, in);
}


static void ngx_set_err_status(ngx_http_request_t *r, ngx_uint_t status_code)
{
	r->err_status = status_code;
	r->headers_out.status = status_code;
	if (r == r->main) {
		ngx_http_clear_content_length(r);
	}
}

static ngx_int_t ngx_http_replace_header_filter(ngx_http_request_t *r)
{
	ngx_http_socket_errno_ctx_t *ctx;
	ngx_http_socket_errno_loc_conf_t  *cf;
	int err = 0;
	ngx_http_upstream_t *up = r->upstream;


	if (up) {
		err = up->peer.socket_errno;
		ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0, "errno %d", err);
	}

	cf = ngx_http_get_module_loc_conf(r, ngx_http_socket_errno_module);
	if (cf == NULL || !cf->enabled) {
		return ngx_http_next_header_filter(r);
	}

	ctx = ngx_http_get_module_ctx(r, ngx_http_socket_errno_module);
	if (ctx == NULL) {
		ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_socket_errno_module));
		if (ctx == NULL) {
			ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
				      "[socket errno]: cannot allocate ctx"
				      " memory");
			return ngx_http_next_header_filter(r);
		}

		ngx_http_set_ctx(r, ctx, ngx_http_socket_errno_module);
	}

	switch (err) {
	case NGX_ETIMEDOUT:
		ngx_set_err_status(r, NGX_HTTP_CONNECTION_TIMED_OUT);
		break;
	case NGX_ECONNREFUSED:
		ngx_set_err_status(r, NGX_HTTP_CONNECTION_REFUSED);
		break;
	case NGX_ECONNRESET:
		ngx_set_err_status(r, NGX_HTTP_CONNECTION_RESET_BY_PEER);
		break;
	case NGX_EHOSTDOWN:
		ngx_set_err_status(r, NGX_HTTP_HOST_IS_DOWN);
		break;
	case NGX_EHOSTUNREACH:
		ngx_set_err_status(r, NGX_HTTP_HOST_UNREACH);
		break;
	}

	return ngx_http_next_header_filter(r);
}


