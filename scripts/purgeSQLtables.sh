#!/bin/bash
#
#   Ce script supprime tous les enregistrements de la table 'wind' plus vieux de 12h
#   et les enregistrements de la table 'badframes' plus vieux de 5 jours
#

DIR=`dirname \`readlink -f $0\``

cd $DIR

. ./mysql.sh

SQLrequest " DELETE FROM wind WHERE date < NOW() - INTERVAL 12 ;"
SQLrequest " DELETE FROM badframes WHERE date < NOW() - INTERVAL 5 DAY ;"


