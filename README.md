## Yiimp Stratum (Equihash Implementation)

Long-awaited implementation of equihash (200.9) protocol for [Yiimp](https://github.com/tpruvot/yiimp) crypto mining pool. Based on original [stratum sources](https://github.com/tpruvot/yiimp/tree/next/stratum) forked on [this](https://github.com/tpruvot/yiimp/commit/eec1befbd3fba1614db023674361e995e6a62829) commit.

Currently app is in developement state (!), it able to receive `getblocktemplate` from daemons, broadcast job to miners, accept and validate solutions, ~~but it still not able to acquire current network diff to determine when accepted share is a block and when it should be send via `submitblock` to daemon~~. Work in progress, so, follow the updates.

### Features

- Equihash 200.9 support with validating solutions via `verifyEH` function.
- Support of so-called "local mode", which allows stratum binary work without Yiimp installed. In this mode it don't need yimmp mysql database and acts as a proxy between daemon `getblocktemplate` and stratum protocol for miners. To enable this mode use `CFLAGS += -DNO_MYSQL` flag in `Makefile`. Coin daemons credentials should be hardcoded in `coins_data[NUM_COINS][NUM_PARAMS]` array in `db.cpp`.
- Supporting of selecting coins via pass `-c=` param in password field in local (non mysql mode).
- Sapling compatible. This implementation of stratum don't construct coinbase tx by itself, instead of that it simply copies coinbase given by daemon in `coinbasetxn` field of `getblocktemplate`. So, if coin has sapling active coinbase tx will be with version 4 and needed `versiongroupid` set.

### Small Q/A

- How can i enable additional logging to see blockhashes, diffs, etc.?

    Add the following lines in your `equihash.conf`:
    ```
    [DEBUGLOG]
    client = 1
    hash = 1
    socket = 1
    rpc = 0
    list = 1
    remote = 1
    ```
- How can i start Stratum in solo mode?

    Compile it with `NO_MYSQL` flag as mentioned above, then place your `equihash.conf` in same directory with `stratum` binary and start it like `./stratum equihash`. Don't forget to fill coins array in `db.cpp` for solo mode.

### Useful docs

- https://en.bitcoin.it/wiki/Stratum_mining_protocol
- https://github.com/slushpool/poclbm-zcash/wiki/Stratum-protocol-changes-for-ZCash
- https://github.com/yqsy/notes/blob/58cd486426e474157ac8ef1c17a934d25401b8f1/business/blockchain/bitcoin/%E7%A5%9E%E5%A5%87%E7%9A%84nBits.md
- https://bitcoin.stackexchange.com/questions/30467/what-are-the-equations-to-convert-between-bits-and-difficulty

### Example of work

- MORTY block [#215488](https://morty.kmd.dev/block/000179a65539ff4d4f358f05306b1d715293f01581fb959ecaa4894b1038829b) `000179a65539ff4d4f358f05306b1d715293f01581fb959ecaa4894b1038829b`

![MORTY block #215488](./images/morty-block-01.png?raw=true "MORTY block #215488")

### Donate

If you would like to support this project developement, you can simply do it by send some [KMD](https://komodoplatform.com/) or [assetchains](https://blog.komodoplatform.com/komodo-platform-why-assetchains-part-01-164325398efa) on `RHNSpVd5T6efj98CioNQHGLcfAZ6qecnQ6`.

### Credits

- @tpruvot author of Yiimp pool and base of this stratum implementation.

### Warnings

- This is beta software, use it on your own risk!



