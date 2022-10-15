
acct="nftone.fund"
ipowner="metadao.fund"

url="https://xdao.mypinata.cloud/ipfs/QmNUC3ugBi1fbXo9rfYv5pcbGcunpKZwd4KTA2FQBsqgga"
mpush verso.itoken create '["'$acct'",1000000,[1000001,0],"'$url'","'$ipowner'"]' -p $acct
mpush verso.itoken issue '["'$acct'",[120000,[1000001,0]],""]' -p $acct
