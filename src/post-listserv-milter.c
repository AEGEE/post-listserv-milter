#include <libmilter/mfapi.h>
#define __USE_GNU 1
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

struct privdata
{
  char* sender;
  char* reply_to;
  char* x_cc;
  char* x_to;
  short reset;
};

// removes the non-address containing text from the parameter (see RFC
// 2822, sec 3.4 and 3.4.1) and leaves just the mails
static inline char* simplify_address(char *text) {
  printf("simplify address: '%s'->", text);
  int i = 0;
  int within_comment = 0, within_quoted = 0;
  char *temp = text;
  /*
   * first remove the comments, this is text within ( and )
   * "\" preceding a parenthesis leads to ignoring the parenthesis (rfc2822,
   * section 3.2.2. Quoted Characters).  In this implementation comments
   * may not nest, which contradicts to section 3.2.3
   */
  while (temp[i] != 0) {
    //remove if the character escapes
    if (temp[i] == '\\') temp[i++] = temp[i+1] = ' ';
    switch (temp[i]){
    case 10:
    case 13: temp[i] = ' '; break;
    //check if temp[i] starts a comment
    case '(': within_comment++; break;
    case ')': within_comment--; break;
    case '"': if (!within_comment)
      //then it is is the name of the destination, but not the e-mail -> remove
      within_quoted = !within_quoted;
      break;
    }

    if (within_comment || within_quoted
      || temp[i] == ')' || temp[i] == '"') temp[i] = ' ';
    //groups are ignored
    i++;
  }
  //at this moment we have only comma-separated addresses
  //surround with < > not surrounded addresses
  printf("-> '%s'", text);
  return strdup(text);
}

static sfsistat post_listserv_header(SMFICTX *ctx, char *headerf, char *headerv)
{
  struct privdata *priv = ((struct privdata *) smfi_getpriv(ctx));
  if (priv == NULL) {
    priv = malloc(sizeof *priv);
    if (priv == NULL) return SMFIS_TEMPFAIL;
    priv->reset = 0;
    memset(priv, '\0', sizeof *priv);
    smfi_setpriv(ctx, priv);
  }
  if (!strcmp(headerf, "Reply-To")) {
    if (!priv->reply_to) free(priv->reply_to);
    priv->reply_to = simplify_address(headerv);
  } else if (!strcmp(headerf, "Sender")) {
    if (!priv->sender) free(priv->sender);
    priv->sender = strdup(headerv);
    if (strcasestr(headerv, "b08") != NULL) priv->reset = 1;
  } else if (!strcmp(headerf, "X-To")) {
    if (!priv->x_to) free(priv->x_to);
    priv->x_to = strdup(headerv);
  } else if (!strcmp(headerf, "X-cc")) {
    if (!priv->x_cc) free(priv->x_cc);
    priv->x_cc = strdup(headerv);
  }
  return SMFIS_CONTINUE;
}

//return 1 if email receives emails from mailing list defined in file,
//or 0 if email does not receive emails from the mailing list (not subscribed
//or with NOMAIL option)
static inline int receives_from_list(FILE *file, char *email) {
  char res[100];
  fseek (file, 20, SEEK_SET);
  while (fscanf (file, "%s", res) == 1)
    if (!strcmp (res, "PW=")) break;
  //after PW= come the actual subscribers
  //email appearing before PW= could be an unsubscribed listowner
  fscanf (file, "%s", res);
  while (fscanf (file, "%s", res) == 1) {
    int i = 0;
    while(res[i])
      res[i] = tolower (res[i++]);
    if (!strcmp (res, email))
      {
	i = strlen (res);
	while (res[i - 1]!='/' && res[i - 2]!='/')
	  fscanf (file, "%s", res);
	//check if the user is in NOMAIL mode
	//this means that the sixth character & 0000 0100 = 0000 0100
	if ((res[5] & 0x08) == 0)
	  return 1;
	else
	  return 0;
      }
  }
  return 0;
}

static sfsistat post_listserv_eom(SMFICTX *ctx)
{
  struct privdata *priv = ((struct privdata *) smfi_getpriv(ctx));
  if (priv->x_cc) {
    smfi_chgheader(ctx, "X-cc", 1, NULL);
    smfi_chgheader(ctx, "CC",   1, priv->x_cc);
  };
  if (priv->x_to) {
    smfi_chgheader(ctx, "X-To", 1, NULL);
    smfi_chgheader(ctx, "To",   1, strdup(priv->x_to));
  }
  if (priv->reset) {
    smfi_chgheader(ctx, "To", 1, "<>");
  }
  if (priv->reply_to
      && strchr(priv->reply_to, ',') != NULL) {//Reply-to contains , => check if one of the address in Reply-To is subscribed to the list
    /*
     * listname is the name of the list
     * email is the email in the reply-to, which is checked for redundancy
     * result is first a temporary variable, and later the data contained in a list
     */
      char *result, *temp = simplify_address(priv->sender);
      strtok_r(temp, "<@>", &result);
      char * listname=strtok_r(NULL, "<@>", &result);
      int i = 0;
      while(listname[i])
	listname[i] = tolower(listname[i++]);
      char res[100];
      sprintf(res, "/home/listserv/home/%s.list", listname);
      free(temp);
      FILE *file = fopen(res, "rb");
      if (file != NULL) {
	temp = strdup(priv->reply_to);
      //get now the sender from Reply-To: list, sender
      //this is the second address, after the comma and the spaces
	char *email = strdup(strchr(temp, ',')+1);
	free(temp);
      //remove the text before <em@il>
	if (strchr(email, '<')!= NULL) {
	  temp = strdup(strchr(email, '<'));
	  free(email);
	  email = temp;
	}

      //now email is either space space space <em@il>
      //or just em@il
      //or space space space em@il

      //remove the spaces at the beginning
	if (email[0]==' ' ) temp = strdup(strtok_r(email," ", &result));
	else temp = strdup(email);
	free(email);
      //now temp starts either with < or with e-mail address
	if (temp[0]=='<')
	  {
	    email = strdup(temp +1);
	    free(temp);
	    *strchr(email,'>')=0;
	    temp = email;
	  }
	email = temp;
      //put the end of the string at the place of the first space
      //email contains now the second addr. in Reply-To
	i = 0;
	while(email[i])
	  email[i] = tolower(email[i++]);
	if (receives_from_list (file, email))
	  smfi_chgheader (ctx, "Reply-To", 1, strdup (priv->sender));
	free(email);
	fclose(file);
      }
  }
  if (priv->x_cc != NULL) free(priv->x_cc);
  if (priv->x_to != NULL) free(priv->x_to);
  if (priv->sender != NULL) free(priv->sender);
  if (priv->reply_to != NULL) free(priv->reply_to);
  if (priv != NULL) free(priv);
  smfi_setpriv(ctx, NULL);
  return SMFIS_CONTINUE;
}

static struct smfiDesc smfilter =
  {
    "Post Listserv Milter",/* filter name */
    SMFI_VERSION, /* version code -- do not change */
    SMFIF_CHGHDRS , /* flags */
    NULL, /* connection info filter */
    NULL, /* SMTP HELO command filter */
    NULL, /* envelope sender filter */
    NULL, /* envelope recipient filter */
    post_listserv_header, /* header filter */
    NULL, /* end of headers */
    NULL, /* body block filter */
    post_listserv_eom, /* end of message */
    NULL, /* message aborted */
    NULL, /* connection cleanup */
  };

int main(int argc, char **argv)
{
  printf("post-listserv-milter, 27 Jan 2007\nReplaces X-cc: and X-To: headers with CC: and To:\nWritten by Dilian Palauzov, dilyan.palauzov@aegee.org\nDistributed under the GNU General Public License v2\n");
  /*
   * s - socket
   * p - set pid file
   * h - prints help
   */
  int c;
  opterr = 1;
  char *pidfile = NULL;
  if (argc == 1) printf("use %s -h for help\n", argv[0]);
  while ((c = getopt(argc, argv, "s:p:h")) != -1)
    switch(c){
    case 'h':
      printf("\n%s supports the following options:\n", argv[0]);
      printf(" -h  prints this message\n");
      printf(" -s  specifies the pathname of a socket to create for communication with sendmail\n");
      printf(" -p  specifies the pathname of a pidfile\n");
      return 0;
    case 'p':
      if (optarg == NULL || *optarg == '\0')
	{
	  printf("-p requires as parameter a file to save the pid\n");
	  return -6;
	}
      pidfile = optarg;
      break;
    case 's':
      if (optarg == NULL ||  *optarg == '\0') {
	printf("-s requires as parameter a socket to connect");
	return -5;
      }
      if (smfi_setconn(optarg) == MI_FAILURE) {
	fprintf(stderr, "Connection to %s could not be created\n", optarg);
	return -1;
      }
      break;
    case 'c':
    default:
      break;
    }
  /*
  pid_t pid = fork();
  if (pid < 0)
    {
      fprintf(stderr, "fork in main() failed\n");
      exit(EXIT_FAILURE);
    }
  if (pid > 0) exit(EXIT_SUCCESS);
  umask(0);
  pid_t sid = setsid();
  if (sid < 0) {
      fprintf(stderr, "setsid in main() failed\n");
      exit(EXIT_FAILURE);
  }
  if (chdir("/") < 0) {
      fprintf(stderr, "chdir in main() mailed\n");
      exit(EXIT_FAILURE);
  }
  if (pidfile != NULL) {
    FILE *stream = fopen(pidfile, "w");
    if (stream == NULL ) {
      fprintf(stderr, "Couldn't open file %s in main()\n", pidfile);
      return -7;
    }
    fprintf(stream, "%i\n", getpid());
    fclose(stream);
  }
  */
  if (smfi_register(smfilter) == MI_FAILURE) {
      fprintf(stderr, "smfi_register failed, most probably not enough memory\n");
      return -3;
  }
  if (smfi_opensocket(1) == MI_FAILURE) {
    fprintf(stderr, "Socket %s could not be opened\n", optarg);
    return -2;
  }
  /*
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  */
  return smfi_main();
}
