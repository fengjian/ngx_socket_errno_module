# add 5xx-Server-Error

```
/*
cloudflare 52x Error
https://support.cloudflare.com/hc/en-us/articles/115003011431-5xx-Server-Error
*/

#define NGX_HTTP_UNKNOWN_ERROR             520
#define NGX_HTTP_HOST_IS_DOWN              521
#define NGX_HTTP_CONNECTION_TIMED_OUT      522
#define NGX_HTTP_CONNECTION_RESET_BY_PEER  523
#define NGX_HTTP_CONNECTION_REFUSED        524
#define NGX_HTTP_HOST_UNREACH              525

```

# build

```
cp ngx_event_connect.h  openresty-1.13.6.1/build/nginx-1.13.6/src/event/ngx_event_connect.h
cp ngx_http_upstream.c  openreshty-1.13.6.1/build/nginx-1.13.6/src/http/ngx_http_upstream.h

cd openresty-1.13.6.1

./configure --prefix=/opt/openresty --with-pcre=../pcre-8.42 --with-openssl=../openssl-1.0.2o --with-luajit --with-pcre-jit --with-stream --with-stream_ssl_module --with-http_v2_module --with-http_stub_status_module --with-http_sub_module --with-http_iconv_module  --with-ipv6 --add-dynamic-module=/root/ngx_http_geoip2_module --with-http_realip_module --add-dynamic-module=/root/ip2location-nginx --add-dynamic-module=/root/ngx_socket_errno_module

make 

make install

```

# use

```
#nginx.conf
http {
	socket_errno on;
...
```

