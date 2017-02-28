# ReadMe

Pi(π) is a new generation blockchain technology. 

Pi adopts a new consensus mechanism called IPoS(Improved Proof of Stake), which helps it reach a balance between fairness and efficiency. Pi introduces a new incentive algorithm to attract users to use and to construct the Pi network.

Pi derives from BitShares ([https://github.com/bitshares/bitshares-core](https://github.com/bitshares/bitshares-core)). 

## Repository Contents

### libraries

This directory include all kinds of libraries used by different apps.

### programs

This directory include main files for different apps. 

`pi_node` is the most important app. This is the core server node which produce blockchains and process all kinds of RPCs.

`cli_wallet` is a reference wallet implement based on Pi RPC protocol.

### test

Unit tests and benchmarks.

### docs

Documentation of pi development.


## How to Build

Pi uses cmake toolchain to build.

```bash
git clone https://github.com/pidiscovery/pi.git
cd pi
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make
```

You'll find a built binary program named `pi_node` at path `pi/build/pi_node/`. 

## License

Pi is open source and permissively licensed under the MIT license. See the LICENSE.md file for more details.






