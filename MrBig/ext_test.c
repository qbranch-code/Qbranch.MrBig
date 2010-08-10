#include "mrbig.h"

//#define DEBUGGING "jajamensan"

#ifdef DEBUGGING
char cfgdir[256];
int dirsep = '\\';

void mrlog(char *p)
{
	printf("mrlog: %s\n", p);
}
#endif

static void pickup_file(char *fn)
{
	char full_fn[256], machname[256], testname[256];
	char b[4900], s[5000], *p;
	FILE *fp;
	size_t n;

	if (debug) mrlog("pickup_file(%s)", fn);

	if (get_option("no_ext", 0)) {
		/* no mrsend with clear status because we don't
		   know the names of the tests
		*/
		return;
	}

	full_fn[0] = machname[0] = testname[0] = '\0';
	snprcat(full_fn, sizeof full_fn, "%s%c%s", pickupdir, dirsep, fn);
	if (debug) mrlog("Full path '%s'", full_fn);

	p = strchr(fn, '.');
	if (p) {
		strncpy(machname, fn, p-fn);
		machname[p-fn] = '\0';
		strncpy(testname, p+1, sizeof testname-1);
	} else {
		strncpy(machname, mrmachine, sizeof machname-1);
		strncpy(testname, fn, sizeof testname-1);
	}
	machname[sizeof machname-1] = '\0';
	testname[sizeof testname-1] = '\0';
	if (debug) {
		mrlog("pickup_file(%s)", fn);
		mrlog("Full path '%s'", full_fn);
		mrlog("machname = '%s'", machname);
		mrlog("testname = '%s'", testname);
	}
	fp = big_fopen("pickup_file", full_fn, "r");
	if (!fp) {
		mrlog("Can't pick up file");
		return;
	}

	n = fread(b, 1, (sizeof b)-1, fp);
	b[n] = '\0';
	no_return(b);

	p = strchr(b, '\n');

	if (p == NULL) {
		mrlog("No status in pickup file");
		big_fclose("pickup_file", fp);
		return;
	}

	*p++ = '\0';

	s[0] = '\0';
	snprcat(s, sizeof s, "%s\n\n%s\n", now, p);

	mrsend(machname, testname, b, s);
	big_fclose("pickup_file", fp);
	remove(full_fn);
}

static void pickup(void)
{
	char pattern[1024];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	pattern[0] = '\0';
	snprcat(pattern, sizeof pattern, "%s%c*", pickupdir, dirsep);

	if (debug) {
		mrlog("pickup()");
		mrlog("picking up from '%s'", pattern);
	}

	hFind = FindFirstFile(pattern, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		mrlog("Invalid pickup directory");
		return;
	}

	if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		pickup_file(FindFileData.cFileName);

	while (FindNextFile(hFind, &FindFileData)) {
		if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			pickup_file(FindFileData.cFileName);
	}
	FindClose(hFind);
}

void ext_tests(void)
{
	char cfgfile[1024], cmd[1024], *p;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD n;
	int i;

	if (debug > 1) mrlog("ext_tests()");

	cfgfile[0] = '\0';
	snprcat(cfgfile, sizeof cfgfile, "%s%c%s", cfgdir, dirsep, "ext.cfg");
	read_cfg("ext", cfgfile);

	for (i = 0; get_cfg("ext", cmd, sizeof cmd, i); i++) {
		p = strchr(cmd, '\n');
		if (p) *p = '\0';
		if (cmd[0] == '#' || cmd[0] == '\0') continue;
		if (debug) mrlog("Ext test: %s", cmd);
		ZeroMemory(&si, sizeof si);
		ZeroMemory(&pi, sizeof pi);
		if (CreateProcess(NULL, cmd, NULL, NULL, FALSE,
				CREATE_NO_WINDOW|DETACHED_PROCESS,
				NULL, NULL, &si, &pi)) {
			/* Wait no more than two minutes */
			n = WaitForSingleObject(pi.hProcess, 120*1000);
			if (debug) mrlog("WaitForSingleObject returns %d", n);
			if (n == WAIT_TIMEOUT) {
				TerminateProcess(pi.hProcess, EXIT_FAILURE);
				mrlog("Terminating process");
				mrlog(cmd);
			}
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		} else {
			mrlog("CreateProcess failed");
		}
	}
	pickup();
}

