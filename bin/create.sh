#!/bin/bash


amcli="amcli -u http://sh-amnod.vmi.amax.dev:18888"
token_url="https://kverso.mypinata.cloud"

token_contract=$1
owner=$2
function create(){
	$amcli push action $token_contract create '["'$owner'","1",['$1', 0],"'$token_url''$2'","'$owner'","avarta"]' -p $owner;
	$amcli push action $token_contract issue '["'$owner'",[1, ['$1' 0]],"x"]' -p $owner
}

start_line=0
for line in $(cat start_line.txt)
do
	start_line=$line
done

step=0
count=0
let init_id=6600000+$start_line-1
for line in $(cat pfp.urls | tail -n +$start_line)
do	
	#create $1 $2 $token_id
	echo $line
	create $init_id $line

	let step=$step+1
	let count=$count+1
	let init_id=$init_id+1
	#echo $token_id
	if [ $step -eq "30" ]; then
		step=0
		echo "每处理30个休眠2秒"
		echo "已处理$count个"
		sleep 2
	fi

	let start_line=$start_line+1
	echo $start_line > start_line.txt
done