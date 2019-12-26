## Yiimp Stratum (Equihash Implementation)

Long-awaited implementation of equihash (200.9) protocol for [Yiimp](https://github.com/tpruvot/yiimp) crypto mining pool. Based on original [stratum sources](https://github.com/tpruvot/yiimp/tree/next/stratum) forked on [this](https://github.com/tpruvot/yiimp/commit/eec1befbd3fba1614db023674361e995e6a62829) commit.

Currently app is in developement state (!), it able to receive `getblocktemplate` from daemons, broadcast job to miners, accept and validate solutions, but it still not able to acquire current network diff to determine when accepted share is a block and when it should be send via `submitblock` to daemon. Work in progress, so, follow the updates.

Features:

- Equihash 200.9 support with validating solutions via `verifyEH` function.
- Support of so-called "local mode", which allows stratum binary work without Yiimp installed. In this mode it don't need yimmp mysql database and acts as a proxy between daemon `getblocktemplate` and stratum protocol for miners. To enable this mode use `CFLAGS += -DNO_MYSQL` flag in `Makefile`. Coin daemons credentials should be hardcoded in `coins_data[NUM_COINS][NUM_PARAMS]` array in `db.cpp`.
- Supporting of selecting coins via pass `-c=` param in password field in local (non mysql mode).
- Sapling compatible. This implementation of stratum don't construct coinbase tx by itself, instead of that it simply copies coinbase given by daemon in `coinbasetxn` field of `getblocktemplate`. So, if coin has sapling active coinbase tx will be with version 4 and needed `versiongroupid` set.

Useful docs:

- https://en.bitcoin.it/wiki/Stratum_mining_protocol
- https://github.com/slushpool/poclbm-zcash/wiki/Stratum-protocol-changes-for-ZCash

Donate:

If you would like to support this project developement, you can simply do it by send some KMD or assetchains on `RHNSpVd5T6efj98CioNQHGLcfAZ6qecnQ6`.

Credits:

- @tpruvot author of Yiimp pool and base of this stratum implementation.

*NB!* This is beta software, use it on your own risk!


