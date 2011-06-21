#!/bin/bash

if [ $# -ne 3 ]
then
  echo "Usage: `basename $0` test version measure"
  exit 1
fi


path=`dirname $0`

#db init
create_table="create table timing (id int auto increment primary key, testcase varchar(255), version varchar(255), measure double);"
sqliteDB="${path}/timing.sqlite"

if [[  -z $VERSION ]]; then
  VERSION=$2
fi


if [[ -z `which sqlite3` ]]; then
  echo "Recording to ${path}/timing.txt"
  echo "$1;$2;$3;" | tee -a ${path}/timing.txt
else
  echo "Recording to $sqliteDB"
  if [[ ! -e "$sqliteDB" ]]; then
    echo "Initialising DB"
    echo $create_table | sqlite3 $sqliteDB
  fi
  echo "INSERT INTO timing (testcase,version,measure) VALUES ('$1','${VERSION}',$3);" | sqlite3 $sqliteDB 
fi



