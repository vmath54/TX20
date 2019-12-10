#!/bin/bash
#
#   Ce script supprime tous les enregistrements de la table 'wind' plus vieux de 2 jours
#   et les enregistrements de la table 'badframes' plus vieux de 5 jours
#

DIR=`dirname \`readlink -f $0\``

cd $DIR

. ./mysql.sh

SQLrequest " DELETE FROM wind WHERE date < NOW() - INTERVAL 2 DAY ;"
SQLrequest " DELETE FROM badframes WHERE date < NOW() - INTERVAL 5 DAY ;"


