#define SERVICENAME "pam_sge-qrsh-setup"

#define PAM_SM_AUTH
#define PAM_SM_ACCOUNT
#define PAM_SM_SESSION
#define PAM_SM_PASSWORD

#define MAX_STRLEN 1024

#include <pwd.h>
#include <security/pam_modules.h>

#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <grp.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int getpppid(void);
void pam_sge_log(int priority, const char *, ...);

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	FILE *file;

	gid_t gids[NGROUPS_MAX];
	int gid_nr = 0;

	char buffer[MAX_STRLEN] = "",
	     job_dir[MAX_STRLEN] = "";
	
	static const char setup_file[] = "/var/run/sge-qrsh-setup";

	if ( ! (flags & PAM_REINITIALIZE_CRED) ) return PAM_SUCCESS;

	/* this should find the correct file if sshd privsep is off */
	sprintf(buffer, "%s.%d", setup_file, getppid());
	pam_sge_log(LOG_DEBUG, "trying to open file %s", buffer);

	if ( ! (file = fopen(buffer, "r")) ) {
		/* ok - we missed it, try it again with parent's parent -
		   sshd privsep on */
		sprintf(buffer, "%s.%d", setup_file, getpppid());
		pam_sge_log(LOG_DEBUG, "trying to open file %s", buffer);

		/* missed it again - never mind, probably there is no 
		   qrsh setup running at all... */
		if ( ! (file = fopen(buffer, "r")) ) return PAM_SUCCESS;
	}

	/* read the job spool directory */
	if ( fgets(job_dir, MAX_STRLEN, file) == NULL ) {
		fclose(file);
		return PAM_SYSTEM_ERR;
	}
	fclose(file);
	job_dir[strlen(job_dir)-1] = '\0';

	/* find the file where SGE's additional group id is stored in */
	snprintf(buffer, MAX_STRLEN, "%s/addgrpid", job_dir);
	if ( ! (file = fopen(buffer, "r")) )
		return PAM_SYSTEM_ERR;

	/* read the id */
	if ( fgets(buffer, MAX_STRLEN, file) == NULL ) {
		fclose(file);
		return PAM_SYSTEM_ERR;
	}
	fclose(file);

	/* fetch the groups this process is member of */
	if ( (gid_nr = getgroups(NGROUPS_MAX, gids)) == -1 )
		return PAM_SYSTEM_ERR;

	/* extract the group id into the groups field */
	if ( ! sscanf(buffer, "%d", &gids[gid_nr]) )
		return PAM_ABORT;

	setgroups(gid_nr+1, gids);

	/* get the job's environment variables */
	snprintf(buffer, MAX_STRLEN, "%s/environment", job_dir);
	if ( ! (file = fopen(buffer, "r")) )
		return PAM_SYSTEM_ERR;
	
	while ( fgets(buffer, MAX_STRLEN, file) ) {
		/* don't set SGE's wrong DISPLAY value... */
		if ( strncmp(buffer, "DISPLAY=", 8) == 0 ) continue;
		/* remove newline at the end of buffer */
		buffer[strlen(buffer)-1] = '\0';
		pam_putenv(pamh, buffer);
	}
	fclose(file);

	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_acct_mgmt(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_open_session(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_close_session(pam_handle_t *pamh,
	int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

int getpppid()
{
	FILE *pipe;
	char buffer[MAX_STRLEN];
	int proc_pid, proc_ppid, ppid = getppid();

	pipe = popen("/bin/ps axeo '\%p \%P'", "r");
	fgets(buffer, MAX_STRLEN, pipe);
	while ( fgets(buffer, MAX_STRLEN, pipe) ) {
		sscanf(buffer, "%5d %5d", &proc_pid, &proc_ppid);
		if ( proc_pid == ppid ) break;
	}
	pclose(pipe);

	return proc_ppid;
}

void pam_sge_log(int priority, const char *msg, ...)
{
	char buf[512];
	va_list plist;
	va_start(plist, msg);
	vsnprintf(buf, sizeof(buf), msg, plist);
	va_end(plist);
	openlog(SERVICENAME, LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);
	syslog(priority, buf);
	closelog();
}

