```
$ wrk -t4 -c400 -d30s --latency http://127.0.0.1:9877
```

I test the server(`tcpserv_poll.c`) performance using wrk:

```
Running 30s test @ http://127.0.0.1:9877
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   825.34us    1.10ms 104.80ms   99.52%
    Req/Sec   125.20k    11.50k  143.08k    71.75%
  Latency Distribution
     50%  724.00us
     75%  797.00us
     90%    1.05ms
     99%    1.71ms
  14944941 requests in 30.09s, 1.61GB read
Requests/sec: 496739.74
Transfer/sec:     54.95MB
```

It seems that the server can handle about 496k requests per second with 400 concurrent connections. The average latency is around 825 microseconds, which indicates good performance for a simple HTTP server serving static files.

It's worth mentioning that I used the function `poll()` for I/O multiplexing in this server implementation. Before this, I had implemented a version using `select()` and it seems that it can provide outstanding performance as well. However, at the moment the test ends, the server using `select()` will crash.

``` 
Running 30s test @ http://127.0.0.1:9877 
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     8.89ms   10.74ms  64.79ms   83.43%
    Req/Sec     5.40k     7.55k   31.22k    84.55%
  Latency Distribution
     50%    3.34ms
     75%    9.48ms
     90%   26.61ms
     99%   45.01ms
  629423 requests in 30.10s, 58.83MB read
Requests/sec:  20912.12
Transfer/sec:      1.95MB
```