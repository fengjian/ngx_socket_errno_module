#  Cloudflare's extension for HTTP status codes

Introducing a GitHub project aimed at modifying the NGINX source code to implement Cloudflare's extension for HTTP status codes at the socket level. This project focuses on enhancing NGINX's capabilities to handle socket-level errors and respond with appropriate HTTP status codes, as specified by Cloudflare's extension. By integrating Cloudflare's functionality into NGINX, this project enhances web server reliability and provides more accurate error responses to clients, improving overall user experience. Developers and system administrators seeking to enhance NGINX's error handling capabilities will find this project valuable for implementing Cloudflare's socket-level error handling features. Explore the modified NGINX source code and leverage Cloudflare's extension to enhance your web server's error handling capabilities.


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
cp ngx_patch/ngx_event_connect.h  openresty-1.13.6.1/build/nginx-1.13.6/src/event/ngx_event_connect.h
cp ngx_patch/ngx_http_upstream.c  openresty-1.13.6.1/build/nginx-1.13.6/src/http/ngx_http_upstream.c

cd openresty-1.13.6.1

./configure  --add-dynamic-module=/xxxpath/ngx_socket_errno_module

make 

make install

```

# usage

```
#nginx.conf


load_module modules/ngx_http_socket_errno_module.so;

http {
	socket_errno on;
...

* Rebuilt URL to: 127.0.0.1/
*   Trying 127.0.0.1...
* TCP_NODELAY set
* Connected to 127.0.0.1 (127.0.0.1) port 80 (#0)
> GET / HTTP/1.1
> Host:www.test.com
> User-Agent: curl/7.51.0-DEV
> Accept: */*
>
< HTTP/1.1 522
< Date: Thu, 11 Apr 2019 03:57:16 GMT
< Content-Type: text/html
< Transfer-Encoding: chunked
< Connection: keep-alive
< Server: ngx
<
* Curl_http_done: called premature == 0
* Connection #0 to host 127.0.0.1 left intact
<html><head><title>522 Connection Time Out</title></head><body bgcolor="white"><center><h1>522 Connection Time Out</h1></center><hr><center>socket errno</center></body></html>522#

```


