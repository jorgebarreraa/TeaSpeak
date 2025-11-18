#!/usr/bin/env bash

while [ true ]; do

#	nc -w 10 172.17.0.2 $1&
#echo "login serveradmin LiPdiKB"; echo "servercreate"
    (echo "login serveradmin markus"; echo "use 3"; echo "channellist"; echo "clientlist") | nc -w 10 localhost $1&
	#nc -w 10 localhost $1&
    # echo "quit" | nc localhost $1&
#    echo "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" | openssl s_client -connect localhost:10101
    #nc localhost $1&
    PID=$!
    ps -p ${PID} > /dev/null 2>&1
    if [ "$?" -ne "0" ]; then
        echo -e "\nInvalid command"
    else
        echo ""
        #kill -9 ${PID}
    fi
done
