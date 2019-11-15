#include <stdio.h>
#include<signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mysql/mysql.h>
#include <sys/file.h>
#include <time.h>
#include "bcm2835.h"
#include "TX20.h"

/* ------------------------------------------------------------------------------------------------------------------------------
	Ce logiciel est destiné à récupérer les infos d'une 'station météo TX20' de la marque 'La Crosse' pour alimenter, optionnellement, une base mysql
	La TX20 est en fait une station capable d'enregistrer la vitesse du vent, et son orientation.

	Si on execute ce programme avec l'option '-h' ou '--help', une aide est apportee

	Si execution avec l'option '-u' ou '--updateDB', les infos de la station alimentent une base mysql.
	   Dans ce cas, il faut que les variables d'environnement suivantes soient positionnees avant execution :
			DB_HOST . DB_PORT . DB_USER . DB_PASSWORD . DB_BASE.
			
	Dans ce programme :
	  . WindDirection : direction du vent. valeur de 0 a 15, depuis le Nord, par pas de 1/16 de 360° (donc, 22.5°)
      . WindSpeed     :  vitesse du vent, en 1/10 eme de m/s
--------------------------------------------------------------------------------------------------------------------------------- */

#define PI_LOCKFILE "/var/run/weather.pid"		// fichier de lock, ou PIDfile

#define DEFAULT_EXEC_TIME 300   // duree par defaut du programme. 5 mn : 5 * 60
# define SQL_DELAY	4			// nbre de secondes mini entre 2 ajouts SQL

static int fdLock = -1;			// handle du fichier de lock


typedef struct 				// derniere trame valide, pour MaJ SQL
{
	int WindDir;
	int WindSpeed;
	int orientation;
	time_t lastSQLupdate;	// heure de dernier MaJ SQL
	int SQLupdate;			// a 1 si la base SQL a ete mise a jour avec cette trame, a 0 sinon
} lastGoodFrame;

typedef struct 				// les infos de connexion mysql
{
	char *host;
	int port;
	char *user;
	char *password;
	char *base;
} sql_conn;


MYSQL *mysql = NULL;
sql_conn mysqlconn;

int mysql_max_errors = 1;				// nbre d'erreurs mysql avant arret du programme
int haltOnInterrupt = 0;				// a 1 si interruption par Ctrl-C ou kill -15

int TX20mode = TX20_MODE_SYNC;			// MODE_SYNC : signal DTR toujours à LOW. La TX20 emet des trames toutes les 1.5s environ
// int TX20mode = TX20_MODE_ASYNC;		// MODE_ASYNC : signal DTR repasse à HIGH apres reception des trames. La TX20 emet une trame environ 5.2s après DRT HIGH

void doExit(int);						// definition de la fonction, car utilisee dans sig_handler, qui est declaree avant

// traitement du kill -15 et du Ctrl-C
void sig_handler(int signo)
{
    printf("Interruption du programme\n");
    haltOnInterrupt = 1;
	
	if (signo == SIGQUIT)				// ne sert qu'a eviter un warning 'unused parameter' lors de la compil
        printf("received SIGQUIT\n");
}

// arret 'propre' du programme
void doExit(int code)
{
	RPi_TX20_Terminate(TX20mode);
    if (mysql != NULL)
		mysql_close(mysql);
	
	if (fdLock != -1)			// on deverroulle le fichier PIDfile, et on le ferme
	{
		close(fdLock);
		unlink(PI_LOCKFILE);
		fdLock = -1;
	}
	exit(code);
}

// recuperation des infos de connexion mysql depuis les variables d'environnement
int getSqlInfos(sql_conn * mysqlconn)
{
	mysqlconn->host = getenv("DB_HOST");
	if (mysqlconn->host == NULL)
		return FALSE;
	mysqlconn->port = atoi(getenv("DB_PORT"));
	if (mysqlconn->port == 0)
		return FALSE;
	mysqlconn->user = getenv("DB_USER");
	if (mysqlconn->user == NULL)
		return FALSE;
	mysqlconn->password = getenv("DB_PASSWORD");
	if (mysqlconn->password == NULL)
		return FALSE;
	mysqlconn->base = getenv("DB_BASE");
	if (mysqlconn->base == NULL)
		return FALSE;
	return TRUE;
}


/* les fonctions initGrabLockFile et initCheckLockFile sont issues des sources de pigpio : https://github.com/joan2937/pigpio/blob/master/pigpio.c
   L'objectif est d'empecher une double execution du programme
   Crée un fichier de lock lors de l'execution s'il n'existe pas. S'il existe, on vérifie si le process est toujours en execution
*/

// controle du fichier PIDfile. Le supprime eventuellement 
static void initCheckLockFile(void)
{
   int fd;
   int count;
   int pid;
   int err;
   int delete;
   char str[20];

   fd = open(PI_LOCKFILE, O_RDONLY);

   if (fd != -1)		// le fichier de lock existe. On lit le PID du process en cours
   {
      delete = 1;

      count = read(fd, str, sizeof(str)-1);

      if (count)
      {
         pid = atoi(str);
         err = kill(pid, 0);	// teste si le process existe
         if (!err) delete = 0; /* process still exists */
      }

      close(fd);

      if (delete)				// le process n'exsite pas. On peut supprimer le fichier
		  unlink(PI_LOCKFILE);
   }
}

// locke le PIDfile, en mode ecriture
static int initGrabLockFile(void)
{
   int fd;
   int lockResult;
   char pidStr[20];

   initCheckLockFile();

   /* try to grab the lock file */

   fd = open(PI_LOCKFILE, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0644);

   if (fd != -1)
   {
      lockResult = flock(fd, LOCK_EX|LOCK_NB);

      if(lockResult == 0)
      {
         sprintf(pidStr, "%d\n", (int)getpid());	// on ecrit le numero de process dans le fichier PIDfile

         if (write(fd, pidStr, strlen(pidStr)) == -1)
         {
            /* ignore errors */
         }
      }
      else
      {
         close(fd);
         return -1;
      }
   }

   return fd;
}


void printUsage()
{
	printf("Usage: weather [OPTION]\nLit les informations de l'anemometre \"La Crosse TX20\", et met a jour optionnellement une base de donnes.\n");
	printf("\t-u, --updateDB\t\tmise a jour de la base de donnees\n");
	printf("\t -exectime xxxxx : le temps, en secondes, d'execution de ce programme. Par defaut, 300 (5 mn)\n");
	printf("\t-v, --verbose\t\tdonne les infos du TX20 en format clair\n");
	printf("\t-h, --help\t\tShow usage information\n\n");
	exit(0);
}

// retourne la date-heure du jour
char * getDate()
{
	static char date[sizeof "JJ/MM/AAAA HH:MM:SS"];

	time_t now = time (NULL);
    struct tm tm_now = *localtime (&now);

    strftime (date, sizeof date, "%d/%m/%Y %H:%M:%S", &tm_now);
	return date;
}

// connexion a la base mysql
void mysql_connect()
{
	mysql = mysql_init(NULL);
	if (! mysql_real_connect(mysql, mysqlconn.host, mysqlconn.user, mysqlconn.password, mysqlconn.base, mysqlconn.port, NULL, 0))
	{
		fprintf(stderr, "mysql error. Connexion à la BDD impossible.\n");
		doExit(1);
	}
}

// mise a jour de la table mysql, pour les infos immediates
int updateSQL(char *table, int orientation, float WindSpeed)
{
    int ret;
	char txt_query[256];
	
	if (mysql == NULL)
	{
		mysql_connect(mysql);
	}
	
    sprintf(txt_query, "INSERT INTO `%s`(`date`, `direction`, `speed`) VALUES (NOW(),'%d','%.1f')", table, orientation, WindSpeed);

    if ((ret = mysql_query(mysql, txt_query)) != 0)
    {
      fprintf(stderr, "SQL Error dans la requete '%s'. code : %d\n", txt_query, ret);
	  mysql_close(mysql);
	  mysql = NULL;
	  return FALSE;
    }

	return TRUE;
}

// mise a jour de la table mysql, pour les infos aggregees. En fin d'execution du programme
int updateSQLbadframes(char *table, int duration, int nbUpdateSQL, int frames, int badframes)
{
    int ret;
	char txt_query[256];

	if (mysql == NULL)
	{
		mysql_connect(mysql);
	}

    sprintf(txt_query, "INSERT INTO `%s`(`date`, `duration`, `records`, `frames`, `badframes`) VALUES (NOW(),'%d', '%d', '%d','%d')", table, duration, nbUpdateSQL, frames, badframes);

    if ((ret = mysql_query(mysql, txt_query)) != 0)
    {
      fprintf(stderr, "SQL Error dans la requete '%s'. code : %d\n", txt_query, ret);
	  mysql_close(mysql);
	  mysql = NULL;
	  return FALSE;
    }

	return TRUE;
}

/* lecture des arguments passes au programme */
void readArgs(int argc, char *argv[], int *verbose, int *exectime, int *updateDB)
{
	int i;
	
	for (i = 1; i < argc ; i++)
	{
		if ((strcmp(argv[i],"--updateDB") == 0) || (strcmp(argv[i],"-u") == 0))
			*updateDB = 1;
		else if ((strcmp(argv[i],"--verbose") == 0) || (strcmp(argv[i],"-v") == 0))
            *verbose = 1;
		else if (strcmp(argv[i],"-exectime") == 0)
		{
			if (argc <= ++i)
			{
				printf("  ## Erreur argument exectime ##\n\n");
				printUsage();
				exit(1);
			}
			else
			{
				*exectime = atoi(argv[i]);
			}
		}
		else if ((strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-h") == 0))
		{
			printUsage();
			exit(0);
		}
		else
		{
			printf("   argument '%s' inconnu\n\n", argv[i]);
			printUsage();
			exit(1);
		}
	}	
}

// --------------- programme principal -------------------------

int main (int argc, char *argv[])
{
	int verbose = 0;
	int exectime = DEFAULT_EXEC_TIME;
	int updateDB = 0;

	int WindSpeed = 0;
	int WindDir = 0;
	int orientation = 0;
	lastFrame *lf;
	lastGoodFrame lgf;
	lgf.SQLupdate = 1;                  // on marque cette trame initiale comme si elle avait été envoyée à la BdeD
	lgf.lastSQLupdate = 0;				// pour MaJ de la DdeD dès la 1ere trame valide

	int nbUpdateSQL = 0;
	int result;
	int frames = 0;
	int badframes = 0;
	
	time_t start;
	struct timespec start_cycle, stop_cycle;
	clockid_t clk_id = CLOCK_MONOTONIC;
	
	// on intercepte Ctrl-C, kill -15, ... pour terminer proprement
	if (signal(SIGINT, sig_handler) == SIG_ERR)  // Ctrl-C
		printf("\ncan't catch SIGINT\n");
    if (signal(SIGTERM, sig_handler) == SIG_ERR)  // kill -15
		printf("\ncan't catch SIGTERM\n");
    if (signal(SIGQUIT, sig_handler) == SIG_ERR) 
		printf("\ncan't catch SIGQUIT\n");
	
	readArgs(argc, argv, &verbose, &exectime, &updateDB);		// lecture des arguments passes au programme

	printf ("debut execution %s UTC. updateDB = %d, exectime = %d, verbose = %d\n\n", getDate(), updateDB, exectime, verbose);

	// on verifie que le programme s'execute avec les droits root. Necessaire pour le GPIO
	if (getuid() != 0)
	{
		printf("\n#### Execution impossible. Ce programme doit être exécuté avec les droits de root ####\n");
		exit(1);
	}

	
	// on verifie maintenant qu'une instance de ce programme n'est pas deja en execution
	fdLock = initGrabLockFile();

    if (fdLock < 0)
	{
		fprintf(stderr, "Une autre instance de ce programme est en cours. Voir %s. Execution impossible\n", PI_LOCKFILE);
		exit(1);
	}

	// si option updateDB, on recupere les infos de connexion dans les variables d'environnement	
	if (updateDB && (! getSqlInfos(&mysqlconn)))
	{
		fprintf(stderr, "Les informations de connexion mysql n'ont pas pu etre recuperees depuis les variables d'environnement\n");
		fprintf(stderr, "Les variables d'environnement sont : DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_BASE\n");
		doExit(1);
	}
	// printf ("host : %s, port : %d, user : %s, password : %s, base : %s\n", mysqlconn.host, mysqlconn.port, mysqlconn.user, mysqlconn.password, mysqlconn.base); exit(0);
	
	start = time(NULL);
	
	if (! RPi_TX20_Initialize(TX20mode))
		exit(1);

	for (;;)  // boucle infinie. on attend la fin d'exectime, ou une interruption par Ctrl-C ou kill
	{

		if ((time(NULL)- start > exectime) || haltOnInterrupt)
		{
			break;
		}

		frames++;
		clock_gettime(clk_id, &start_cycle);
		
		//Read from the TX20
		result = RPi_TX20_GetReading(TX20mode, &WindDir, &WindSpeed);
		clock_gettime(clk_id, &stop_cycle);		
		double duration = (double) (stop_cycle.tv_sec - start_cycle.tv_sec) + (double)(stop_cycle.tv_nsec - start_cycle.tv_nsec) / 1000000000;

		switch(result)
		{
			case INVALID_PIN_STATE :
			{
				fprintf(stderr, "Erreur GPIO. %d\n", frames);
				doExit(1);
			}
			
			case NO_FRAME :
			{
				fprintf(stderr, "Pas de trame detectee apres signal DTR. %d\n", frames);
				doExit(1);
			}
			
			case FRAME_NOK :
			{
				badframes++;
				lf = getLastFrame();
				printf("%2.1fs. Trame invalide. startframe=%d, windir=%d-%d, windspeed=%d-%d, checksum=%d-%d\n", duration,
				        lf->startframe, lf->winddir, lf->winddir2, lf->windspeed, lf->windspeed2, lf->checksum, lf->chk);
				printf("    %s\n", getLastDecodeFrame());
				break;
			}
			
		}		
				
		if (result == FRAME_OK)
		{
			orientation = (int) WindDir * 22.5;     // on transforme en degres
			if (verbose)
			{
				printf("%2.1fs. Wind Direction: %s (%d).", duration, TX20_Directions[WindDir], orientation);
				printf("     Wind Speed: %.1f m/s, %.1f km/h\n", WindSpeed * 0.10, WindSpeed * 0.36);
			}
			lgf.WindDir = WindDir;
			lgf.WindSpeed = WindSpeed;
			lgf.orientation = orientation;
			lgf.SQLupdate = 0;
		}

		if (updateDB && (lgf.SQLupdate == 0) && (time(NULL) - lgf.lastSQLupdate >= SQL_DELAY))  // ajout BdeD
		{				
			if (! updateSQL("wind", orientation, (float) WindSpeed / 10))
			{
				if (--mysql_max_errors < 0)
					doExit(1);
			}
			else
			{
				lgf.lastSQLupdate = time(NULL);
				lgf.SQLupdate = 1;				// pour ne pas enregistrer 2 fois la même trame
				nbUpdateSQL++;
			}
		}

		//bcm2835_delay(1000);		// pause, en micro secondes. NE PAS UTILISER :  perturbe fortement le calcul de temps de clock_gettime.
		
	}
	
	int duration = time(NULL) - start; 
	
	if (updateDB)
	{
		if (lgf.SQLupdate == 0)		// il reste une trame pour la BdD
		{
			if (! updateSQL("wind", lgf.orientation, (float) lgf.WindSpeed / 10))
			{
				doExit(1);
			}
			nbUpdateSQL++;
			updateSQL("last5min", orientation, (float) WindSpeed / 10);
		}
		
		if (! updateSQLbadframes("badframes", duration, nbUpdateSQL, frames, badframes))
		{
			doExit(1);
		}
	}
		
	float percent = badframes == 0 ? 0 : (float) badframes * 100 / frames;
	printf ("fin d'exécution %s UTC. %d secondes. nb frames : %d, nb trames en erreur : %d -> %d%s\n", getDate(), duration, frames, badframes, (int) percent, "%");

	doExit(0);
}
