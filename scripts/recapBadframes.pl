#!/usr/bin/perl

use Getopt::Long qw(:config no_ignore_case no_auto_abbrev);
use Data::Dumper;
use strict;
use DBI;

my $fic = "recap.txt";
my $table = "badframes";
my %sqlInfos = ();


{
  my $day;						# date en format AAAAMMJJ
  my $daySQL;					# date en format AAAA-MM-JJ
  my $allRecords = 0;			# nombre total d'enregistrements SQL
  my $allFrames = 0;			# nombre total de trames traitee
  my $allBadFrames = 0;			# nombre total de trames invalides
  
  if ($ARGV[0] ne "")			# une date est passée en argument
  {
    $day = $ARGV[0];
	if ($day !~ /^(20\d\d)([0-1]\d)([0-3]\d)$/)
	{
	  print "La date passee en argument \($day\) n'est pas valide. Le format est AAAAMMJJ\n";
	  exit(1);
	}
	else
	{
	   $daySQL = "$1-$2-$3";		# on transforme en AAAA-MM-JJ
	}
  }
  else							# pas de date en argument. On prend la date de la veille
  {
    ($day, $daySQL) = &getDate();
  }
  
  &getenv(\%sqlInfos);			# recuperation des infos d'environnement mysql
  
  my $request = "SELECT date, duration, records, frames, badframes  FROM \`$table\` WHERE date LIKE \'$daySQL%\';";
  
  my $dbd = DBI->connect( "dbi:mysql:dbname=$sqlInfos{base};host=$sqlInfos{host};",$sqlInfos{user}, $sqlInfos{password})
    or die 'Connexion impossible à la base de données : '.DBI::errstr; 

  #print "$request\n";	
  my $prep = $dbd->prepare($request)
    or die "Impossible de préparer la requête : ".$dbd->errstr;

  $prep->execute
    or die 'Impossible d\'exécuter la requête : '.$prep->errstr;

  my $ind = 0;
  while (my($date, $duration, $records, $frames, $badframes) = $prep->fetchrow_array )
  {
    $ind++;
    #print "$date;$duration;$records;$frames;$badframes\n";
	$allRecords += $records;
	$allFrames += $frames;
	$allBadFrames += $badframes;
  }
  die "aucun enregistrement trouve pour le jour $day" if ($ind == 0);	

  &printBilan($fic, $day, $allRecords, $allFrames, $allBadFrames);
  
  $prep->finish;
  $dbd->disconnect;
}

# calcul de la date de la veille
sub getDate
{
  my (undef, undef, undef, $jour, $mois,$annee, undef, undef, undef) = localtime(time-3600*24);
  my $daySQL = sprintf("%04d-%02d-%02d", $annee + 1900, $mois + 1, $jour);
  my $day = sprintf("%04d%02d%02d", $annee + 1900, $mois + 1, $jour);
  return ($day, $daySQL);
}

sub printBilan
{
  my $fic = shift;
  my $day = shift;
  my $records = shift;
  my $frames = shift;
  my $badframes = shift;
  
  my $new = -e $fic ? 0 : 1;
  my $percent = int(($badframes * 100) / $frames);
  
  print "$day. nb records = $records. nb frames = $frames. nb badframes = $badframes. percent = $percent\n";

  die "Ouverture du fichier $fic impossible" if (! open(FIC, ">>$fic"));
    
  print FIC "day;nb records;nb frames;nb bad frames;percent\n"  if ($new); # entete du fichier
  print FIC "$day;$records;$frames;$badframes;$percent\n";
}

# recuperation des variables d'environnement de connexion a mysql
sub getenv
{
  my $sqlInfos = shift;
  
  $$sqlInfos{host} = $ENV{'DB_HOST'};
  die "variable d'environnement 'DB_HOST' pas trouvee" if ($$sqlInfos{host} eq "");  
  $$sqlInfos{port} = $ENV{'DB_PORT'};
  die "variable d'environnement 'DB_PORT' pas trouvee" if ($$sqlInfos{port} eq "");  
  $$sqlInfos{user} = $ENV{'DB_USER'};
  die "variable d'environnement 'DB_USER' pas trouvee" if ($$sqlInfos{user} eq "");  
  $$sqlInfos{password} = $ENV{'DB_PASSWORD'};
  die "variable d'environnement 'DB=PASSWORD' pas trouvee" if ($$sqlInfos{password} eq "");  
  $$sqlInfos{base} = $ENV{'DB_BASE'};
  die "variable d'environnement 'DB_BASE' pas trouvee" if ($$sqlInfos{base} eq "");    
}