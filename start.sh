nodeos --config-dir ./config --data-dir ./data --logconf ./logging.json --genesis-json ./genesis.json  >> "./chain.log" 2>&1 & echo $! > "./eosd.pid"
#../leap-bls/leap-master/leap/build/bin/nodeos --config-dir ./config --data-dir ./data --logconf ./logging.json --genesis-json ./genesis.json  >> "./chain.log" 2>&1 & echo $! > "./eosd.pid"
#../if-leap/leap/build/bin/nodeos --config-dir ./config --data-dir ./data --logconf ./logging.json --genesis-json ./genesis.json  >> "./chain.log" 2>&1 & echo $! > "./eosd.pid"
