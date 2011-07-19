
/*
 * Copyright (C) Maxim Dounin
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {

    ngx_http_upstream_init_pt          original_init_upstream;
    ngx_http_upstream_init_peer_pt     original_init_peer;

} ngx_http_upstream_http_connect_srv_conf_t;



static ngx_int_t ngx_http_upstream_init_http_connect_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us);
static void *ngx_http_upstream_http_connect_create_conf(ngx_conf_t *cf);
static char *ngx_http_upstream_http_connect(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);


static ngx_command_t  ngx_http_upstream_http_connect_commands[] = {

    { ngx_string("http_connect"),
      NGX_HTTP_UPS_CONF|NGX_CONF_NOARGS,
      ngx_http_upstream_http_connect,
      NGX_HTTP_SRV_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_upstream_http_connect_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_http_upstream_http_connect_create_conf, /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_upstream_http_connect_module = {
    NGX_MODULE_V1,
    &ngx_http_upstream_http_connect_module_ctx, /* module context */
    ngx_http_upstream_http_connect_commands,    /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


ngx_int_t
ngx_http_upstream_init_http_connect_upstream(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_http_connect_srv_conf_t  *ccf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, cf->log, 0,
                   "init http_connect upstream");

    ccf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_http_connect_module);

    if (ccf->original_init_upstream(cf, us) != NGX_OK) {
        return NGX_ERROR;
    }

    ccf->original_init_peer = us->peer.init;

    us->peer.init = ngx_http_upstream_init_http_connect_peer;

    return NGX_OK;
}


static ngx_int_t
ngx_http_upstream_init_http_connect_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_http_upstream_http_connect_srv_conf_t   *ccf;
    ngx_http_upstream_t *ups;
    ngx_buf_t           *b;
    ngx_chain_t         *cl;    
    ngx_uint_t          len;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "init http connect peer");
  //    ccf = ngx_http_get_module_srv_conf(r, ngx_http_upstream_http_connect_module);
    ccf = ngx_http_conf_upstream_srv_conf(us,
                                          ngx_http_upstream_http_connect_module);


    if (ccf->original_init_peer(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    ups = r->upstream;
    len = sizeof("CONNECT ") + (2 * r->headers_in.host->value.len) + sizeof(" HTTP/1.1") 
          + sizeof("Host: ") + sizeof("X-HTTP-EMBEDDED-REQ-START: ") +  (4 * sizeof(CRLF)) + 8;
    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    cl->buf = b;

    b->last = ngx_copy(b->last, "CONNECT ", sizeof("CONNECT ") - 1);
    b->last = ngx_copy(b->last, r->headers_in.host->value.data, r->headers_in.host->value.len);
    b->last = ngx_copy(b->last, " HTTP/1.1", sizeof(" HTTP/1.1") - 1);
    *b->last++ = CR; *b->last++ = LF;
    b->last = ngx_copy(b->last, "Host: ", sizeof("Host: ") - 1);
    b->last = ngx_copy(b->last, r->headers_in.host->value.data, r->headers_in.host->value.len);
    *b->last++ = CR; *b->last++ = LF;
    b->last = ngx_copy(b->last, "X-HTTP-EMBEDDED-REQ-START: ", sizeof("X-HTTP-EMBEDDED-REQ-START: ") - 1);
    b->last = ngx_sprintf(b->last, "%08X", len);

    *b->last++ = CR; *b->last++ = LF;
    *b->last++ = CR; *b->last++ = LF;

    cl->next = ups->request_bufs;
    ups->request_bufs = cl;
    
    return NGX_OK;
}


static void *
ngx_http_upstream_http_connect_create_conf(ngx_conf_t *cf)
{
    ngx_http_upstream_http_connect_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool,
                       sizeof(ngx_http_upstream_http_connect_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->original_init_upstream = NULL;
     *     conf->original_init_peer = NULL;
     */

    return conf;
}


static char *
ngx_http_upstream_http_connect(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_upstream_srv_conf_t            *uscf;
    ngx_http_upstream_http_connect_srv_conf_t  *ccf;


    uscf = ngx_http_conf_get_module_srv_conf(cf, ngx_http_upstream_module);

    ccf = ngx_http_conf_upstream_srv_conf(uscf,
                                          ngx_http_upstream_http_connect_module);

    ccf->original_init_upstream = uscf->peer.init_upstream
                                  ? uscf->peer.init_upstream
                                  : ngx_http_upstream_init_round_robin;

    uscf->peer.init_upstream = ngx_http_upstream_init_http_connect_upstream;

    return NGX_CONF_OK;

}
