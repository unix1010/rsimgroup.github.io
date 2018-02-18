#!/usr/dcs/software/supported/bin/perl

print "Content-type: text/html\n\n";

print "<html>\n";
print "<head>\n";

#open(FormLog,">>"."/usr/dcs/www/www-root/rsim/cgi-bin/log2");

while (<STDIN>){
  $Line=$_;
#  print FormLog $Line; print FormLog "\n";
  &ParseFormLine($Line);
}

#if ($FormFields{"submit"} eq "Accept the License") {
  print "<META HTTP-EQUIV=refresh CONTENT=\"0; url=http://rsim.cs.uiuc.edu/ramp/ramp20/download.html\">\n";
  print "</head>\n";

  if(open(UsersLog,">>"."/usr/dcs/www/www-root/rsim/cgi-bin/rampusers")) {

    printf(UsersLog "Name: %s\n",$FormFields{"name"});
    printf(UsersLog "Email: %s\n",$FormFields{"email"});
    printf(UsersLog "Institution: %s\n",$FormFields{"ins"});
    printf(UsersLog "Purpose: %s\n",$FormFields{"purpose"});
    printf(UsersLog "*****************************\n");
  }
  else {$error=1;}

  if ($error) {$message="Error opening users file.\n"; print FormLog "error opening file"; }
  else {
    $message=sprintf("Name: %s\n",$FormFields{"name"});
    $message=sprintf("%sEmail: %s\n",$message,$FormFields{"email"});
    $message=sprintf("%sInstitution: %s\n",$message,$FormFields{"ins"});
    $message=sprintf("%sPurpose: %s\n",$message,$FormFields{"purpose"});
  }
   &SendMail("RAMP WWW server <ramp\@cs.uiuc.edu>","Jayanth Srinivasan <srinivsn\@cs.uiuc.edu>",
    "RAMP download request","\n".$message);
# others to mail: srampell@cs.rice.edu, Partha.Ranganathan@hp.com
# }
# else {
#   print "</head>\n";
#   print "<body>\n";
#   print "<h1>License not accepted</h1>\n";
#   print "Click <a href=\"http://rsim.cs.uiuc.edu/ramp\">here</a> to return\n";
#   print "to the main RAMP page.\n";
#   print "</body>\n";
# }

print "</html>\n";

################# End of main program

sub SendMail{
  $From   =$_[0];
  $To     =$_[1];
  $Subject=$_[2];
  $Body   =$_[3];
  if(!open(SendInput,"| /usr/lib/sendmail -U -t 2>&1 1>/dev/null")){
    return 0;
  }
  print SendInput "From: $From\n";
  print SendInput "To: $To\n";
  print SendInput "Subject: $Subject\n";
  print SendInput "$Body\n";
  print SendInput ".\n";
  close SendInput;
  return 1;
}

sub ParseFormLine{
  my $Line=$_[0];

  while ($Line=~/^([^&]+)(.*)$/){
    $_=$1;
    $Line=$2;

    if ($Line=~/^&(.*)$/){
      $Line=$1;
    }

    if (/^(.*)=(.*)$/){
      $FieldName=$1;
      $FieldValue=$2;
      $FormFields{&NormalizeString($FieldName)}=&NormalizeString($FieldValue);
    }
  }
}

sub NormalizeString{
  $_=$_[0];
  my $NewString="";

  while (/^([^+]*)[+](.*)$/){
    $_= $1." ".$2;
  }

  while (/^([^%]*)%([0-9a-fA-F][0-9a-fA-F])(.*)$/){
    $NewString=sprintf("%s%s%c",$NewString,$1,hex($2));
    $_=$3;
  }

  $_=$NewString.$_;
  return $_;
}

