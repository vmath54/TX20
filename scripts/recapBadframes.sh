#!/bin/bash

# lecture des infos badframes de la veille, et ecriture du bilan dans recap.txt

DIR=`dirname \`readlink -f $0\``
cd $DIR
. ./mysql.sh

./recapBadframes.pl 

