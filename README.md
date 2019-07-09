# Fork note

Rudely modified to achive multi-processor performance.

```
./crackvim -C 4 test.txt
```

# new implementation `zipforce`

The new implementation detects whether the content is **utf-8** encoded, and use a multi-thread solution, performance is greatly (x40~) increased.


|            | 1 thread     | 4 threads     |
| ---------- | ------------ | ------------- |
| `crackvim` | 250k words/s | 1.1m words/s  |
| `zipforce` | 4.2m words/s | 11.3m words/s |

_Note: tested on same 1.4 GHz Intel Core i7_

# crackvim

cracks vim encrypted text files. only supports default encryption level (zip).

## Help screen

    $ ./crackvim
    crackvim: [options] [filename]

    Options:
    	-b nbytes (default: 128)
    	-d dict_file
    	-p start_password (default: empty string)
    	-C charset (default: 0)
    	-l max_passwd_len (default: 6)
    	-c crib

    $

## Brute Force Example

    $ ./crackvim test.txt
    loaded test.txt: 40 bytes
    searching for ascii text files
    using brute force
    max password length: 6
    charset: 0

    Possible password: 'lenin'
    Plaintext: meet at the park on tuesday

    $

## Dictionary Example

    $ ./crackvim -d /usr/share/dict/words dict_test.txt
    loaded dict_test.txt: 54 bytes
    searching for ascii text files
    using dictionary file: /usr/share/dict/words

    Possible password: 'unobjectionableness'
    Plaintext: sell all shares before the board meeting.

    $
