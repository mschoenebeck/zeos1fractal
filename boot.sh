CLEOS=cleos

echo "create system accounts"
$CLEOS wallet unlock -n test --password PW5KjNG9Tkb97TAmDsB44N9MvLii7nAAfXFQtYk9qDsEdE4VuhNia
$CLEOS create account eosio eosio.bpay EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.msig EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.names EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.ram EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.ramfee EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.saving EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.stake EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.token EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.vpay EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
$CLEOS create account eosio eosio.rex EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
echo "create test accounts"
$CLEOS create account eosio zeos1fractal EOS7UtLPhkuCwccNaJ4B3Sak7mUyt2gYa43qvB4AmQtXs9smRjmFR
$CLEOS set account permission --add-code zeos1fractal active -p zeos1fractal@owner
$CLEOS create account eosio dummyaccount EOS7UtLPhkuCwccNaJ4B3Sak7mUyt2gYa43qvB4AmQtXs9smRjmFR
$CLEOS create account eosio aliceaccount EOS7UtLPhkuCwccNaJ4B3Sak7mUyt2gYa43qvB4AmQtXs9smRjmFR
echo "all accounts created - deploy system contracts..."
$CLEOS set contract eosio.token system_contracts/eosio.token/
$CLEOS push action eosio.token create '[ "eosio", "10000000000.0000 EOS" ]' -p eosio.token@active
$CLEOS push action eosio.token issue '[ "eosio", "1000000000.0000 EOS", "initial issuance" ]' -p eosio
$CLEOS push action eosio.token transfer '[ "eosio", "zeos1fractal", "1000000.0000 EOS", "test tokens for the fractal" ]' -p eosio

curl --request POST --url http://127.0.0.1:8888/v1/producer/schedule_protocol_feature_activations     -d '{"protocol_features_to_activate": ["0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd"]}'
sleep 1
$CLEOS set contract eosio system_contracts/eosio.boot/
$CLEOS push action eosio activate '["f0af56d2c5a48d60a4a5b5c903edfb7db3a736a94ed589d0b797df33ff9d3e1d"]' -p eosio
$CLEOS push action eosio activate '["2652f5f96006294109b3dd0bbde63693f55324af452b799ee137a81a905eed25"]' -p eosio
$CLEOS push action eosio activate '["8ba52fe7a3956c5cd3a656a3174b931d3bb2abb45578befc59f283ecd816a405"]' -p eosio
$CLEOS push action eosio activate '["ad9e3d8f650687709fd68f4b90b41f7d825a365b02c23a636cef88ac2ac00c43"]' -p eosio
$CLEOS push action eosio activate '["68dcaa34c0517d19666e6b33add67351d8c5f69e999ca1e37931bc410a297428"]' -p eosio
$CLEOS push action eosio activate '["e0fb64b1085cc5538970158d05a009c24e276fb94e1a0bf6a528b48fbc4ff526"]' -p eosio
$CLEOS push action eosio activate '["ef43112c6543b88db2283a2e077278c315ae2c84719a8b25f25cc88565fbea99"]' -p eosio
$CLEOS push action eosio activate '["4a90c00d55454dc5b059055ca213579c6ea856967712a56017487886a4d4cc0f"]' -p eosio
$CLEOS push action eosio activate '["1a99a59d87e06e09ec5b028a9cbb7749b4a5ad8819004365d02dc4379a8b7241"]' -p eosio
$CLEOS push action eosio activate '["4e7bf348da00a945489b2a681749eb56f5de00b900014e137ddae39f48f69d67"]' -p eosio
$CLEOS push action eosio activate '["4fca8bd82bbd181e714e283f83e1b45d95ca5af40fb89ad3977b653c448f78c2"]' -p eosio
$CLEOS push action eosio activate '["299dcb6af692324b899b39f16d5a530a33062804e41f09dc97e9f156b4476707"]' -p eosio
$CLEOS push action eosio activate '["d528b9f6e9693f45ed277af93474fd473ce7d831dae2180cca35d907bd10cb40"]' -p eosio
$CLEOS push action eosio activate '["5443fcf88330c586bc0e5f3dee10e7f63c76c00249c87fe4fbf7f38c082006b4"]' -p eosio
#$CLEOS push action eosio activate '["c58504ee14e94e2b467fddf9d0c37b70b78ca7783f63636cda3cbf1d476c69cf"]' -p eosio   # BLS_PRIMITIVES
#$CLEOS push action eosio activate '["0ec7e080177b2c02b278d5088611686b49d739925a92d9bfcacd7fc6b74053bd"]' -p eosio
$CLEOS push action eosio activate '["6bcb40a24e49c26d0a60513b6aeb8551d264e4717f306b81a37a5afb3b47cedc"]' -p eosio   # CRYPTO_PRIMITIVES
$CLEOS push action eosio activate '["35c2186cc36f7bb4aeaf4487b36e57039ccf45a9136aa856a5d569ecca55ef2b"]' -p eosio   # GET_BLOCK_NUM
echo "features activated"
sleep 1

echo "deploy eosio.system"
$CLEOS set contract eosio system_contracts/eosio.system/
$CLEOS push action eosio init '["0", "4,EOS"]' -p eosio@active

echo "deploy zeos1fractal contract"
$CLEOS set contract zeos1fractal . zeos1fractal.wasm zeos1fractal.abi
$CLEOS push action zeos1fractal init '{"first_event_block_height":300,"event_interval":1209600,"participate_duration":1800,"rooms_duration":4800,"fib_offset":3,"council_size":4,"num_approvals_required":5}' -p zeos1fractal@owner

echo "run tests for 'signup'"
$CLEOS system buyram eosio dummyaccount "100.0000 EOS" -p eosio@active
$CLEOS system buyram eosio aliceaccount "100.0000 EOS" -p eosio@active
$CLEOS push action zeos1fractal signup '[ "dummyaccount", "I want to be part of the fractal!", "I am a dummy user", [ { "first": "github", "second": "dummyuser" }, { "first": "telegram", "second": "@dummy" } ] ]' -p dummyaccount@active
# this test should fail because of empty links map
$CLEOS push action zeos1fractal signup '[ "aliceaccount", "test", "I am alice", [] ]' -p aliceaccount@active
