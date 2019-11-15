Lecture des informations d'un anémometre La Crosse TX20 sur un raspberry

C'est un programme C, weather.c, qui s'exécute sur un raspberry, et qui recupère les informations d'un anémomètre TX20 raccordés sur le GPIO.
Il met à jour un base mysql, qui sera utilisée pour exploiter les données.
Il s'inspire fortement du développement de redstorm1 : https://github.com/redstorm1/RPi-TX20, et utilise les informations disponibles à https://www.john.geek.nz/2011/07/la-crosse-tx20-anemometer-communication-protocol/

Les sources sont dans /src/bcm2835 : ceci utilise la librairie GPIO bcm2835

Des scripts d'exécution, et utilitaires se trouvent dans /scripts

La création des tables SQL relatives à ce programme se trouve dans /sql

A noter ces dysfonctionnements :
  . Des trames erronées recues du TX20 ; environ 17%. Il n'a pas été possible de déterminer la cause, la TX20 étant installée sur un mat en haut d'un hangard, pas très accessible.
    Il faudrait pouvoir faire des tests en laboratoire.
    les causes probables sont :
	- affaiblissement / distorsion du signal lié à la longueur du cable : 20m de cable entre la TX20 et la raspberry
	- imprécision de la mesure du temps lors que l'analyse des bits de la trame (fonction delayMicroseconds)
	
  . consommation de près de 100% du CPU. Ceci est du à la boucle qui attend le début de la trame : fonction RPi_TX20_GetReading dans TX20.c
    Il serait souhaitable d'untiliser une autre librairie de gestion du GPIO, qui pourrait fonctionner par interruption pour détecter le début de trames.
	quelques essais on été fait avec la libairie pigpio, non concluants. A creuser ...