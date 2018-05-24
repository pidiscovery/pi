#!/bin/bash

CONFIG_FILE='/data/config.ini'

echo "# -----------------------------------"
echo "# Begin generating $CONFIG_FILE..."
echo "# -----------------------------------"

echo "# configuration of pi witness node" > $CONFIG_FILE

if [ $RPC_ENDPOINT ]
then
    echo "rpc-endpoint = $RPC_ENDPOINT" >> $CONFIG_FILE
fi

if [ $P2P_ENDPOINT ]
then
    echo "p2p-endpoint = $P2P_ENDPOINT" >> $CONFIG_FILE
fi

if [ $SEED_NODES ]
then
    OLD_IFS="$IFS"
    IFS="#"
    ARR_SN=($SEED_NODES)
    for SEED_NODE in ${ARR_SN[@]}
    do
        echo "seed-node = $SEED_NODE" >> $CONFIG_FILE
    done
    IFS="$OLD_IFS"
fi

if [ $CHECKPOINTS ]
then
    OLD_IFS="$IFS"
    IFS="#"
    ARR_CP=($CHECKPOINTS)
    for CHECKPOINT in ${ARR_CP[@]}
    do
        OLD_IFS1="$IFS"
        IFS=","
        ARR_CP=($CHECKPOINT)
        echo "checkpoint = [\"${ARR_CP[0]}\", \"${ARR_CP[1]}\"]" >> $CONFIG_FILE
        IFS="$OLD_IFS1"
    done
    IFS="$OLD_IFS"
fi

echo "genesis-json = /data/genesis.json" >> $CONFIG_FILE

if [ $ENABLE_STALE_PRODUCTION ]
then
    echo "enable-stale-production = $ENABLE_STALE_PRODUCTION" >> $CONFIG_FILE
else
    echo "enable-stale-production = false" >> $CONFIG_FILE
fi

echo "required-participation = false" >> $CONFIG_FILE

if [ $WITNESS_IDS ]
then
    OLD_IFS="$IFS"
    IFS="#"
    ARR_WID=($WITNESS_IDS)
    for WITNESS_ID in ${ARR_WID[@]}
    do
        echo "witness-id = \"$WITNESS_ID\"" >> $CONFIG_FILE
    done
    IFS="$OLD_IFS"
fi

if [ $SIGN_KEYS ]
then
    OLD_IFS="$IFS"
    IFS="#"
    ARR_SK=($SIGN_KEYS)
    for SIGN_KEY in ${ARR_SK[@]}
    do
        OLD_IFS1="$IFS"
        IFS=","
        ARR_PP=($SIGN_KEY)
        echo "private-key = [\"${ARR_PP[0]}\", \"${ARR_PP[1]}\"]" >> $CONFIG_FILE
        IFS="$OLD_IFS1"
    done
    IFS="$OLD_IFS"
fi

cat >> $CONFIG_FILE <<- EOF
bucket-size = [15,60,300,3600,86400]
history-per-size = 1000
EOF

echo "# $CONFIG_FILE generated:"
cat $CONFIG_FILE
echo "# -----------------------------------"
echo "# End generating $CONFIG_FILE."
echo "# -----------------------------------"


echo "witness_node -d . $WITNESS_ARGS"
witness_node -d . $WITNESS_ARGS

echo "Done running witness_node"

