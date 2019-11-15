#!/bin/bash

export DB_HOST="xxxxxxxxx"
export DB_PORT=3306
export DB_USER="xxxxxxxx"
export DB_PASSWORD="xxxxxxx"
export DB_BASE="xxxxxxxxx"


SQLrequest()
{
  if [ $# -eq 0 ]; then
    echo "ERROR SQLrequest. Manque parametre"
    return 1
  fi
  /usr/bin/mysql -u "$DB_USER" --password="$DB_PASSWORD" -h "$DB_HOST" -D "$DB_BASE" -e "$1"
  return $?
}
