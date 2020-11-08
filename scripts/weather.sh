#!/bin/bash
#
# ATTENTION. Ce script dit etre lancé en user ROOT. Sinon, le binaire weather n'a pas assez de droit pour fonctionner
#

DIR=`dirname \`readlink -f $0\``
HOSTNAME=`hostname`
DATE=`date`

cd $DIR

. ./mysql.sh

# EXECTIME : le temps en secondes d'execution du programme weather. Peut etre depasse de 6 secondes. 585 = 9mn 45s
EXECTIME=585

#FICLOG=weather.log
FICLOG="/dev/null"
#FICLOG="/dev/stdout"


echo "#################### $DATE ##################### " >> $FICLOG

echo ""
echo "------- execution de weather pendant 9mn45s ------" >> $FICLOG
./weather -u -exectime $EXECTIME >> $FICLOG 2>&1
RETOUR=$?

DATE=`date`
echo "" >> $FICLOG
echo "--- $DATE, fin execution. Code retour $RETOUR ---" >> $FICLOG
echo "" >> $FICLOG


