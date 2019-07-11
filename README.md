# Fork note

Rudely modified to achive multi-processor performance.

To run w/ GPU:
```
./a.out test.txt
```

To run w/ CPU:

```
TH=4 ./a.out test.txt
```

# new implementation

The new implementation detects whether the content is **utf-8** encoded, and use a multi-thread solution, performance is greatly (x40~) increased.

|                     | 1 thread     | 4 threads     |
| ------------------- | ------------ | ------------- |
| `original crackvim` | 250k words/s | 1.1m words/s  |
| `check u8 update`   | 4.2m words/s | 12.3m words/s |
| `opencl update`     | 35m words/s  | 35m words/s   |

_Note: tested on same 1.4 GHz Intel Core i7, w/ intel HD graphics 615_
